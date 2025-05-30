# 创建环境
conda create -n gptfs_env python=3.10 -y

# 激活环境
conda activate gptfs_env

# 安装 Python 依赖
pip install fusepy requests

# 安装系统依赖 (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y libfuse-dev

# 设置执行权限
chmod +x /home/hqs123/os_homework/fuse/gptfs.py
