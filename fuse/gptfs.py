#!/usr/bin/env python3

import os
import sys
import errno
import stat
import time
import threading
import requests
from urllib.error import URLError
import json
from datetime import datetime
from fuse import FUSE, FuseOSError, Operations, LoggingMixIn

# GPT API 配置
API_KEY = "sk-c4ea82a347374cb6b48c55008ab14092"
API_URL = "https://api.deepseek.com/v1/chat/completions"
API_MODEL = "deepseek-chat"

class GPTfs(LoggingMixIn, Operations):
    def __init__(self):
        self.files = {}          # 存储文件内容
        self.file_attrs = {}     # 存储文件属性
        self.locks = {}          # 存储文件锁
        self.sessions = set()    # 存储已创建的会话目录
        self.uid = os.getuid()   # 获取当前用户ID
        self.gid = os.getgid()   # 获取当前用户组ID
        
        # 确保根目录存在
        now = time.time()
        self.file_attrs['/'] = {
            'st_mode': stat.S_IFDIR | 0o755,
            'st_nlink': 2,
            'st_size': 0,
            'st_ctime': now,
            'st_mtime': now,
            'st_atime': now,
            'st_uid': self.uid,
            'st_gid': self.gid
        }

    def _full_path(self, path):
        """获取文件的绝对路径"""
        if path.startswith("/"):
            return path
        else:
            return "/" + path

    def _get_session_from_path(self, path):
        """从路径中提取会话ID"""
        parts = path.strip('/').split('/')
        if len(parts) > 0:
            return parts[0]
        return None

    def _get_file_from_path(self, path):
        """从路径中提取文件名"""
        parts = path.strip('/').split('/')
        if len(parts) > 1:
            return parts[1]
        return None

    def _is_special_file(self, path):
        """检查是否为特殊文件（input, output, error）"""
        file_name = self._get_file_from_path(path)
        return file_name in ['input', 'output', 'error']

    def _initialize_session(self, session_id):
        """初始化一个新的会话目录及其文件"""
        if session_id in self.sessions:
            return
            
        session_path = f"/{session_id}"
        now = time.time()
        
        # 创建会话目录
        self.file_attrs[session_path] = {
            'st_mode': stat.S_IFDIR | 0o755,
            'st_nlink': 2,
            'st_size': 0,
            'st_ctime': now,
            'st_mtime': now,
            'st_atime': now,
            'st_uid': self.uid,
            'st_gid': self.gid
        }
        
        # 创建会话文件
        for file_name in ['input', 'output', 'error']:
            file_path = f"{session_path}/{file_name}"
            self.files[file_path] = b''
            self.file_attrs[file_path] = {
                'st_mode': stat.S_IFREG | 0o644,
                'st_nlink': 1,
                'st_size': 0,
                'st_ctime': now,
                'st_mtime': now,
                'st_atime': now,
                'st_uid': self.uid,
                'st_gid': self.gid
            }
            self.locks[file_path] = threading.Lock()
            
        self.sessions.add(session_id)

    def _call_gpt_api(self, prompt):
        """调用 DeepSeek API 处理用户输入"""
        if not API_KEY:
            return b"", b"Error: DeepSeek API key not found. Please set the DEEPSEEK_API_KEY environment variable."

        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {API_KEY}"
        }
        
        data = {
            "model": API_MODEL,
            "messages": [
                {"role": "user", "content": prompt}
            ],
            "temperature": 0.7,
            "max_tokens": 2000  # DeepSeek 可能需要指定最大输出长度
        }
        
        try:
            response = requests.post(API_URL, headers=headers, data=json.dumps(data), timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                # DeepSeek 的响应格式可能与 OpenAI 不同，请根据实际情况调整
                message = result['choices'][0]['message']['content']
                return message.encode('utf-8'), b""
            else:
                error_msg = f"API Error: {response.status_code} - {response.text}"
                return b"", error_msg.encode('utf-8')
                
        except requests.exceptions.RequestException as e:
            error_msg = f"Network Error: {str(e)}"
            return b"", error_msg.encode('utf-8')
        except Exception as e:
            error_msg = f"Unexpected Error: {str(e)}"
            return b"", error_msg.encode('utf-8')

    # FUSE API 方法实现
    def getattr(self, path, fh=None):
        """获取文件属性"""
        path = self._full_path(path)
        
        if path not in self.file_attrs:
            raise FuseOSError(errno.ENOENT)
            
        return self.file_attrs[path]

    def readdir(self, path, fh):
        """读取目录内容"""
        path = self._full_path(path)
        
        # 根目录
        if path == '/':
            return ['.', '..'] + list(self.sessions)
        
        # 会话目录
        session_id = self._get_session_from_path(path)
        if session_id in self.sessions:
            return ['.', '..', 'input', 'output', 'error']
            
        raise FuseOSError(errno.ENOENT)

    def mkdir(self, path, mode):
        """创建目录（新会话）"""
        path = self._full_path(path)
        
        # 只能在根目录下创建会话目录
        if path.count('/') != 1:
            raise FuseOSError(errno.EPERM)
            
        session_id = path.strip('/')
        if not session_id:
            raise FuseOSError(errno.EINVAL)
            
        self._initialize_session(session_id)
        return 0

    def open(self, path, flags):
        """打开文件"""
        path = self._full_path(path)
        
        if path not in self.file_attrs:
            raise FuseOSError(errno.ENOENT)
            
        # 返回一个假的文件句柄
        return 0

    def read(self, path, size, offset, fh):
        """读取文件内容"""
        path = self._full_path(path)
        
        if path not in self.files:
            raise FuseOSError(errno.ENOENT)
            
        with self.locks.get(path, threading.Lock()):
            data = self.files[path]
            return data[offset:offset + size]

    def write(self, path, data, offset, fh):
        """写入文件内容"""
        path = self._full_path(path)
        
        if path not in self.files:
            raise FuseOSError(errno.ENOENT)
            
        with self.locks.get(path, threading.Lock()):
            # 获取会话ID和文件名
            session_id = self._get_session_from_path(path)
            file_name = self._get_file_from_path(path)
            
            # 只允许写入 input 文件
            if file_name != 'input':
                raise FuseOSError(errno.EPERM)
                
            # 更新文件内容
            content = self.files[path]
            new_content = content[:offset] + data
            if offset + len(data) < len(content):
                new_content += content[offset + len(data):]
            self.files[path] = new_content
            
            # 更新文件属性
            self.file_attrs[path]['st_size'] = len(new_content)
            self.file_attrs[path]['st_mtime'] = time.time()
            
            # 异步处理 GPT 请求
            threading.Thread(target=self._process_input, args=(session_id,)).start()
            
            return len(data)

    def truncate(self, path, length):
        """截断文件"""
        path = self._full_path(path)
        
        if path not in self.files:
            raise FuseOSError(errno.ENOENT)
            
        with self.locks.get(path, threading.Lock()):
            # 获取文件名
            file_name = self._get_file_from_path(path)
            
            # 只允许截断 input 文件
            if file_name != 'input':
                raise FuseOSError(errno.EPERM)
                
            # 截断文件
            self.files[path] = self.files[path][:length]
            self.file_attrs[path]['st_size'] = length
            self.file_attrs[path]['st_mtime'] = time.time()

    def _process_input(self, session_id):
        """处理用户输入，调用 GPT API"""
        input_path = f"/{session_id}/input"
        output_path = f"/{session_id}/output"
        error_path = f"/{session_id}/error"
        
        # 获取输入内容
        with self.locks.get(input_path, threading.Lock()):
            input_data = self.files.get(input_path, b'').decode('utf-8')
        
        if not input_data.strip():
            # 输入为空，不处理
            return
            
        # 调用 GPT API
        output_data, error_data = self._call_gpt_api(input_data)
        
        # 更新输出和错误文件
        now = time.time()
        
        with self.locks.get(output_path, threading.Lock()):
            self.files[output_path] = output_data
            self.file_attrs[output_path]['st_size'] = len(output_data)
            self.file_attrs[output_path]['st_mtime'] = now
            
        with self.locks.get(error_path, threading.Lock()):
            self.files[error_path] = error_data
            self.file_attrs[error_path]['st_size'] = len(error_data)
            self.file_attrs[error_path]['st_mtime'] = now

    def create(self, path, mode, fi=None):
        """创建文件 - 不允许创建新文件"""
        raise FuseOSError(errno.EPERM)

    def unlink(self, path):
        """删除文件 - 不允许删除文件"""
        raise FuseOSError(errno.EPERM)

    def rmdir(self, path):
        """删除目录 - 不允许删除会话"""
        raise FuseOSError(errno.EPERM)

    def chmod(self, path, mode):
        """修改文件权限 - 不允许修改权限"""
        raise FuseOSError(errno.EPERM)

    def chown(self, path, uid, gid):
        """修改文件所有者 - 不允许修改所有者"""
        raise FuseOSError(errno.EPERM)

    def utimens(self, path, times=None):
        """更新文件时间戳"""
        path = self._full_path(path)
        
        if path not in self.file_attrs:
            raise FuseOSError(errno.ENOENT)
            
        now = time.time()
        atime, mtime = times if times else (now, now)
        self.file_attrs[path]['st_atime'] = atime
        self.file_attrs[path]['st_mtime'] = mtime

def main(mountpoint):
    FUSE(GPTfs(), mountpoint, nothreads=False, foreground=True)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <mountpoint>')
        sys.exit(1)
        
    main(sys.argv[1])
