/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

/*
 * NOTE! This filesystem is probably most useful
 * not as a real filesystem, but as an example of
 * how virtual filesystems can be written.
 *
 * It doesn't get much simpler than this. Consider
 * that this file implements the full semantics of
 * a POSIX-compliant read-write filesystem.
 *
 * Note in particular how the filesystem does not
 * need to implement any data structures of its own
 * to keep track of the virtual data: using the VFS
 * caches is sufficient.
 */

#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs_context.h>
#include <linux/fs_parser.h>
#include "internal.h"

#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/file.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/fs_struct.h>
#include <linux/buffer_head.h>

// 在文件中添加全局变量和互斥锁
char *ramfs_sync_dir = NULL;
DEFINE_MUTEX(ramfs_sync_mutex);
static struct proc_dir_entry *ramfs_proc_dir;

/* 常量定义 */
#define RAMFS_PROC_DIR "fs/ramfs"
#define RAMFS_BIND_ENTRY "bind"
#define RAMFS_SYNC_ENTRY "sync"
#define RAMFS_MAX_PATH 256

int ramfs_bind(const char *sync_dir);
int ramfs_file_flush(struct file *file);

struct ramfs_mount_opts {
	umode_t mode;
};

struct ramfs_fs_info {
	struct ramfs_mount_opts mount_opts;
};

#define RAMFS_DEFAULT_MODE	0755

static const struct super_operations ramfs_ops;
static const struct inode_operations ramfs_dir_inode_operations;

struct inode *ramfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(&init_user_ns, inode, dir, mode);
		inode->i_mapping->a_ops = &ram_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &ramfs_file_inode_operations;
			inode->i_fop = &ramfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &ramfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			inode_nohighmem(inode);
			break;
		}
	}
	return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
static int
ramfs_mknod(struct user_namespace *mnt_userns, struct inode *dir,
	    struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode * inode = ramfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = current_time(dir);
	}
	return error;
}

static int ramfs_mkdir(struct user_namespace *mnt_userns, struct inode *dir,
		       struct dentry *dentry, umode_t mode)
{
	int retval = ramfs_mknod(&init_user_ns, dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
	return retval;
}

static int ramfs_create(struct user_namespace *mnt_userns, struct inode *dir,
			struct dentry *dentry, umode_t mode, bool excl)
{
	return ramfs_mknod(&init_user_ns, dir, dentry, mode | S_IFREG, 0);
}

static int ramfs_symlink(struct user_namespace *mnt_userns, struct inode *dir,
			 struct dentry *dentry, const char *symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = ramfs_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = current_time(dir);
		} else
			iput(inode);
	}
	return error;
}

static int ramfs_tmpfile(struct user_namespace *mnt_userns,
			 struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;

	inode = ramfs_get_inode(dir->i_sb, dir, mode, 0);
	if (!inode)
		return -ENOSPC;
	d_tmpfile(dentry, inode);
	return 0;
}

static const struct inode_operations ramfs_dir_inode_operations = {
	.create		= ramfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= ramfs_symlink,
	.mkdir		= ramfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= ramfs_mknod,
	.rename		= simple_rename,
	.tmpfile	= ramfs_tmpfile,
};

/*
 * Display the mount options in /proc/mounts.
 */
static int ramfs_show_options(struct seq_file *m, struct dentry *root)
{
	struct ramfs_fs_info *fsi = root->d_sb->s_fs_info;

	if (fsi->mount_opts.mode != RAMFS_DEFAULT_MODE)
		seq_printf(m, ",mode=%o", fsi->mount_opts.mode);
	return 0;
}

static const struct super_operations ramfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= ramfs_show_options,
};

enum ramfs_param {
	Opt_mode,
};

const struct fs_parameter_spec ramfs_fs_parameters[] = {
	fsparam_u32oct("mode",	Opt_mode),
	{}
};

static int ramfs_parse_param(struct fs_context *fc, struct fs_parameter *param)
{
	struct fs_parse_result result;
	struct ramfs_fs_info *fsi = fc->s_fs_info;
	int opt;

	opt = fs_parse(fc, ramfs_fs_parameters, param, &result);
	if (opt < 0) {
		/*
		 * We might like to report bad mount options here;
		 * but traditionally ramfs has ignored all mount options,
		 * and as it is used as a !CONFIG_SHMEM simple substitute
		 * for tmpfs, better continue to ignore other mount options.
		 */
		if (opt == -ENOPARAM)
			opt = 0;
		return opt;
	}

	switch (opt) {
	case Opt_mode:
		fsi->mount_opts.mode = result.uint_32 & S_IALLUGO;
		break;
	}

	return 0;
}

static int ramfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct ramfs_fs_info *fsi = sb->s_fs_info;
	struct inode *inode;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
	sb->s_magic		= RAMFS_MAGIC;
	sb->s_op		= &ramfs_ops;
	sb->s_time_gran		= 1;

	inode = ramfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}

static int ramfs_get_tree(struct fs_context *fc)
{
	return get_tree_nodev(fc, ramfs_fill_super);
}

static void ramfs_free_fc(struct fs_context *fc)
{
	kfree(fc->s_fs_info);
}

static const struct fs_context_operations ramfs_context_ops = {
	.free		= ramfs_free_fc,
	.parse_param	= ramfs_parse_param,
	.get_tree	= ramfs_get_tree,
};

int ramfs_init_fs_context(struct fs_context *fc)
{
	struct ramfs_fs_info *fsi;

	fsi = kzalloc(sizeof(*fsi), GFP_KERNEL);
	if (!fsi)
		return -ENOMEM;

	fsi->mount_opts.mode = RAMFS_DEFAULT_MODE;
	fc->s_fs_info = fsi;
	fc->ops = &ramfs_context_ops;
	return 0;
}

void ramfs_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}

static struct file_system_type ramfs_fs_type = {
	.name		= "ramfs",
	.init_fs_context = ramfs_init_fs_context,
	.parameters	= ramfs_fs_parameters,
	.kill_sb	= ramfs_kill_sb,
	.fs_flags	= FS_USERNS_MOUNT,
};

/**
 * parse_mount_path - 解析挂载路径和目标目录
 * @buffer: 用户输入的字符串
 * @ramfs_path: 输出参数，保存 ramfs 挂载路径
 * @sync_path: 输出参数，保存同步目录路径
 *
 * 格式: "/path/to/ramfs /path/to/sync/dir"
 * 返回: 0 表示成功，负数表示错误
 */
static int parse_mount_path(const char *buffer, char *ramfs_path, char *sync_path)
{
    int i = 0, j = 0;
    
    /* 跳过开头的空格 */
    while (buffer[i] == ' ' || buffer[i] == '\t')
        i++;
        
    /* 解析 ramfs 路径 */
    while (buffer[i] && buffer[i] != ' ' && buffer[i] != '\t' && buffer[i] != '\n') {
        if (j >= RAMFS_MAX_PATH - 1)
            return -ENAMETOOLONG;
        ramfs_path[j++] = buffer[i++];
    }
    ramfs_path[j] = '\0';
    
    /* 路径不能为空 */
    if (j == 0)
        return -EINVAL;
        
    /* 跳过中间的空格 */
    while (buffer[i] == ' ' || buffer[i] == '\t')
        i++;
        
    /* 解析同步目录路径 */
    j = 0;
    while (buffer[i] && buffer[i] != ' ' && buffer[i] != '\t' && buffer[i] != '\n') {
        if (j >= RAMFS_MAX_PATH - 1)
            return -ENAMETOOLONG;
        sync_path[j++] = buffer[i++];
    }
    sync_path[j] = '\0';
    
    /* 路径不能为空 */
    if (j == 0)
        return -EINVAL;
        
    return 0;
}

/**
 * ramfs_bind - 绑定同步目录
 * @ramfs_path: ramfs 的挂载路径
 * @sync_dir: 同步目录的路径
 *
 * 设置用于持久化存储ramfs文件的目录
 * 返回0表示成功，负数表示错误码
 */
int ramfs_bind(const char *sync_dir)
{
    char *new_dir;
    int ret = 0;
    struct path path;

    if (!sync_dir)
        return -EINVAL;

    /* 验证同步目录路径是否有效 */
    ret = kern_path(sync_dir, LOOKUP_FOLLOW, &path);
    if (ret)
        return ret;

    /* 确保它是一个目录 */
    if (!S_ISDIR(d_inode(path.dentry)->i_mode)) {
        ret = -ENOTDIR;
        goto out;
    }

    /* 确保我们有写权限 */
    if (!(d_inode(path.dentry)->i_mode & S_IWUSR)) {
        ret = -EACCES;
        goto out;
    }

    mutex_lock(&ramfs_sync_mutex);

    /* 复制路径字符串 */
    new_dir = kstrdup(sync_dir, GFP_KERNEL);
    if (!new_dir) {
        ret = -ENOMEM;
        mutex_unlock(&ramfs_sync_mutex);
        goto out;
    }

    /* 如果已存在旧目录，释放它 */
    kfree(ramfs_sync_dir);
    ramfs_sync_dir = new_dir;

    mutex_unlock(&ramfs_sync_mutex);
    pr_info("RAMfs: Bound to sync directory %s\n", ramfs_sync_dir);

out:
    path_put(&path);
    return ret;
}
EXPORT_SYMBOL(ramfs_bind);

/**
 * ramfs_file_flush - 将ramfs文件刷新到持久存储
 * @file: 要持久化的文件
 *
 * 把ramfs文件的内容写入到已绑定的同步目录中
 * 返回0表示成功，负数表示错误码
 */
int ramfs_file_flush(struct file *file)
{
    struct dentry *dentry = file->f_path.dentry;
    struct inode *inode = d_inode(dentry);
    const char *filename = dentry->d_name.name;
    char *filepath = NULL;
    struct file *sync_file = NULL;
    struct file *read_file = NULL;  // 新增：专门用于读取的文件句柄
    loff_t pos = 0;
    int ret = 0;
    char *buf = NULL;
    ssize_t bytes_read, bytes_written;
    size_t len;
    loff_t size;
    char *temp_path = NULL;
	char fullpath[PATH_MAX];
    char *pathname;
    // pid_t pid;

    pr_info("RAMfs: flushing file %s\n", file->f_path.dentry->d_name.name);
    
    // 确保检查 file 参数是否有效
    if (!file || IS_ERR(file)) {
        pr_err("RAMfs: Invalid file pointer passed to ramfs_file_flush\n");
        return -EINVAL;
    }

    // 检查文件是否属于RAMfs
    if (!file->f_path.dentry || !file->f_path.dentry->d_sb || 
        file->f_path.dentry->d_sb->s_type != &ramfs_fs_type) {
        pr_info("RAMfs: Not a ramfs file, skipping flush\n");
        return 0;  // 返回成功但不做任何事
    }

    /* 如果没有设置同步目录，直接返回成功 */
    mutex_lock(&ramfs_sync_mutex);
    if (!ramfs_sync_dir) {
        mutex_unlock(&ramfs_sync_mutex);
        return 0;
    }
    
    /* 复制同步目录路径，避免在后续操作中持有mutex */
    temp_path = kstrdup(ramfs_sync_dir, GFP_KERNEL);
    mutex_unlock(&ramfs_sync_mutex);
    
    if (!temp_path)
        return -ENOMEM;

    /* 构造完整的文件路径 */
    filepath = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!filepath) {
        kfree(temp_path);
        return -ENOMEM;
    }
	pr_info("RAMfs: filepath %s\n", filepath);

    /* 创建临时文件路径 */
    snprintf(filepath, PATH_MAX, "%s/.%s.tmp", temp_path, filename);
    // pid = task_pid_nr(current);
    // snprintf(filepath, PATH_MAX, "%s/.%s.%d.tmp", temp_path, filename, pid);
    
    /* 以"写+截断"模式打开临时文件 */
    sync_file = filp_open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (IS_ERR(sync_file)) {
        ret = PTR_ERR(sync_file);
        pr_err("RAMfs: Failed to open temp file %s: %d\n", filepath, ret);
        goto out_free_path;
    }

	pathname = d_path(&file->f_path, fullpath, PATH_MAX);
    if (IS_ERR(pathname)) {
        ret = PTR_ERR(pathname);
        pr_err("RAMfs: Failed to get file path: %d\n", ret);
        goto out_close_file;
    }
    
    // pr_info("RAMfs: Opening source file with full path: %s\n", pathname);
    
    /* 以只读模式打开源文件 */
    read_file = filp_open(pathname, O_RDONLY, 0);
    if (IS_ERR(read_file)) {
        ret = PTR_ERR(read_file);
        pr_err("RAMfs: Failed to open source file for reading: %d\n", ret);
        goto out_close_file;
    }

    /* 分配缓冲区以复制文件内容 */
    size = i_size_read(inode);
    
    /* 使用页大小作为缓冲区大小 */
    len = PAGE_SIZE;
    buf = kmalloc(len, GFP_KERNEL);
    if (!buf) {
        ret = -ENOMEM;
        goto out_close_read_file;
    }

    /* 将文件指针重置到开头 */
    pos = 0;
    
    /* 逐块读取文件内容并写入临时文件 */
    while (pos < size) {
        loff_t read_pos = pos;
        loff_t write_pos = pos;
        size_t bytes_to_read = min_t(size_t, len, size - pos);
        
        /* 从ramfs文件读取 */
        bytes_read = kernel_read(read_file, buf, bytes_to_read, &read_pos);
        if (bytes_read < 0) {
            ret = bytes_read;
            pr_err("RAMfs: Failed to read from source file: %d\n", ret);
            goto out_free_buf;
        }
        
        if (bytes_read == 0)
            break;
        
        /* 写入同步文件 */
        bytes_written = kernel_write(sync_file, buf, bytes_read, &write_pos);
        if (bytes_written < 0) {
            ret = bytes_written;
            pr_err("RAMfs: Failed to write to temp file: %d\n", ret);
            goto out_free_buf;
        }
        
        pos += bytes_written;
    }
    
    /* 确保数据被写入磁盘 */
    ret = vfs_fsync(sync_file, 0);
    if (ret) {
        pr_err("RAMfs: Failed to sync temp file: %d\n", ret);
        goto out_free_buf;
    }
    
    /* 关闭临时文件 */
    filp_close(sync_file, NULL);
    sync_file = NULL;
    filp_close(read_file, NULL);
    read_file = NULL;
    
    /* 构造最终文件路径 */
    snprintf(filepath, PATH_MAX, "%s/%s", temp_path, filename);
    
    /* 原子重命名临时文件到最终文件 */
    {
        struct path old_path, new_dir_path;
        struct dentry *new_dentry;
        char *tmp_path = kmalloc(PATH_MAX, GFP_KERNEL);
        char *final_name = NULL;
        struct renamedata rd;  // 使用 renamedata 结构体
        struct inode *dir_inode;
        
        if (!tmp_path) {
            ret = -ENOMEM;
            goto out_free_buf;
        }
        
        /* 获取临时文件路径 */
        snprintf(tmp_path, PATH_MAX, "%s/.%s.tmp", temp_path, filename);
        // snprintf(filepath, PATH_MAX, "%s/.%s.%d.tmp", temp_path, filename, pid);
        ret = kern_path(tmp_path, 0, &old_path);
        if (ret) {
            pr_err("RAMfs: Failed to get temp file path: %d\n", ret);
            kfree(tmp_path);
            goto out_free_buf;
        }
        
        /* 获取目标目录路径 */
        ret = kern_path(temp_path, 0, &new_dir_path);
        if (ret) {
            pr_err("RAMfs: Failed to get sync dir path: %d\n", ret);
            path_put(&old_path);
            kfree(tmp_path);
            goto out_free_buf;
        }

        /* 获取目录的 inode 并锁定 */
        dir_inode = d_inode(new_dir_path.dentry);
        inode_lock(dir_inode);
        
        /* 在目标目录中查找或创建文件名 */
        final_name = (char *)filename;
        new_dentry = lookup_one_len(final_name, new_dir_path.dentry, strlen(final_name));
        if (IS_ERR(new_dentry)) {
            ret = PTR_ERR(new_dentry);
            pr_err("RAMfs: Failed to lookup target file: %d\n", ret);
            inode_unlock(dir_inode);  // 确保解锁
            path_put(&old_path);
            path_put(&new_dir_path);
            kfree(tmp_path);
            goto out_free_buf;
        }

        memset(&rd, 0, sizeof(rd));
        rd.old_dentry = old_path.dentry;
        rd.old_dir = d_inode(old_path.dentry->d_parent);
        rd.new_dentry = new_dentry;
        rd.new_dir = dir_inode;  // 使用已获取的指针
        rd.flags = 0;
        
        /* 执行原子重命名操作 */
        ret = vfs_rename(&rd);
        
        if (ret)
            pr_err("RAMfs: Failed to rename temp file to target: %d\n", ret);
        else
            pr_info("RAMfs: Successfully synced file %s\n", filename);
        
        dput(new_dentry);
        inode_unlock(dir_inode);  // 解锁 inode
        path_put(&old_path);
        path_put(&new_dir_path);
        kfree(tmp_path);
    }
    
    kfree(buf);
    kfree(filepath);
    kfree(temp_path);
    return ret;

out_free_buf:
    kfree(buf);
out_close_read_file:
    if (read_file && !IS_ERR(read_file))
        filp_close(read_file, NULL);
out_close_file:
    if (sync_file && !IS_ERR(sync_file))
        filp_close(sync_file, NULL);
out_free_path:
    kfree(filepath);
    kfree(temp_path);
    return ret;
}
EXPORT_SYMBOL(ramfs_file_flush);

/* 处理 /proc/fs/ramfs/bind 的读操作 */
static ssize_t ramfs_proc_bind_read(struct file *file, char __user *buf,
                                    size_t count, loff_t *ppos)
{
    char temp[PATH_MAX];
    int len;
    
    mutex_lock(&ramfs_sync_mutex);
    if (!ramfs_sync_dir) {
        len = snprintf(temp, PATH_MAX, "No sync directory bound\n");
    } else {
        len = snprintf(temp, PATH_MAX, "%s\n", ramfs_sync_dir);
    }
    mutex_unlock(&ramfs_sync_mutex);
    
    if (*ppos >= len)
        return 0;
        
    if (count > len - *ppos)
        count = len - *ppos;
        
    if (copy_to_user(buf, temp + *ppos, count))
        return -EFAULT;
        
    *ppos += count;
    return count;
}

/* 处理 /proc/fs/ramfs/bind 的写操作 */
static ssize_t ramfs_proc_bind_write(struct file *file, const char __user *buf,
                                     size_t count, loff_t *ppos)
{
    char *kbuf, *ramfs_path, *sync_path;
    int ret;
    
    if (count >= PATH_MAX)
        return -EINVAL;
    
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
        
    ramfs_path = kmalloc(RAMFS_MAX_PATH, GFP_KERNEL);
    if (!ramfs_path) {
        kfree(kbuf);
        return -ENOMEM;
    }
    
    sync_path = kmalloc(RAMFS_MAX_PATH, GFP_KERNEL);
    if (!sync_path) {
        kfree(kbuf);
        kfree(ramfs_path);
        return -ENOMEM;
    }
    
    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    kbuf[count] = '\0';
    
    ret = parse_mount_path(kbuf, ramfs_path, sync_path);
    if (ret)
        goto out;
    
    /* 验证 ramfs 挂载点 */
    /* 注意：这里简化了，实际应该检查挂载点是否为 ramfs 类型 */
    
    /* 绑定同步目录 */
    ret = ramfs_bind(sync_path);
    if (ret == 0)
        ret = count;
    
out:
    kfree(kbuf);
    kfree(ramfs_path);
    kfree(sync_path);
    return ret;
}

/* 处理 /proc/fs/ramfs/sync 的写操作 */
static ssize_t ramfs_proc_sync_write(struct file *file, const char __user *buf,
                                    size_t count, loff_t *ppos)
{
    char *kbuf;
    int ret;
    struct path path;
    struct file *target_file;
    
    if (count >= PATH_MAX)
        return -EINVAL;
    
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
        
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    /* 确保字符串以 NULL 结尾 */
    kbuf[count] = '\0';
    
    /* 删除尾部的换行符 */
    if (count > 0 && kbuf[count-1] == '\n')
        kbuf[count-1] = '\0';
    
    /* 查找文件 */
    ret = kern_path(kbuf, 0, &path);
    if (ret) {
        kfree(kbuf);
        return ret;
    }
    
    /* 打开文件 */
    target_file = dentry_open(&path, O_RDWR, current_cred());
    path_put(&path);
    
    if (IS_ERR(target_file)) {
        ret = PTR_ERR(target_file);
        kfree(kbuf);
        return ret;
    }
    
    /* 执行同步操作 */
    ret = ramfs_file_flush(target_file);
    
    filp_close(target_file, NULL);
    kfree(kbuf);
    
    return ret == 0 ? count : ret;
}

/* 定义 /proc/fs/ramfs/bind 的文件操作 */
static const struct proc_ops ramfs_bind_fops = {
    .proc_read = ramfs_proc_bind_read,
    .proc_write = ramfs_proc_bind_write,
};

/* 定义 /proc/fs/ramfs/sync 的文件操作 */
static const struct proc_ops ramfs_sync_fops = {
    .proc_write = ramfs_proc_sync_write,
};

/* 初始化 proc 接口 */
static int __init ramfs_init_proc(void)
{
    /* 创建 /proc/fs/ramfs 目录 */
    ramfs_proc_dir = proc_mkdir(RAMFS_PROC_DIR, NULL);
    if (!ramfs_proc_dir)
        return -ENOMEM;
        
    /* 创建 /proc/fs/ramfs/bind 文件 */
    if (!proc_create(RAMFS_BIND_ENTRY, 0644, ramfs_proc_dir, &ramfs_bind_fops))
        goto remove_dir;
        
    /* 创建 /proc/fs/ramfs/sync 文件 */
    if (!proc_create(RAMFS_SYNC_ENTRY, 0200, ramfs_proc_dir, &ramfs_sync_fops))
        goto remove_bind;
        
    pr_info("RAMfs: Persistence interface initialized\n");
    return 0;
    
remove_bind:
    remove_proc_entry(RAMFS_BIND_ENTRY, ramfs_proc_dir);
remove_dir:
    remove_proc_entry(RAMFS_PROC_DIR, NULL);
    return -ENOMEM;
}

/* 清理 proc 接口 */
static void __exit ramfs_exit_proc(void)
{
    remove_proc_entry(RAMFS_SYNC_ENTRY, ramfs_proc_dir);
    remove_proc_entry(RAMFS_BIND_ENTRY, ramfs_proc_dir);
    remove_proc_entry(RAMFS_PROC_DIR, NULL);
}

/* 将 proc 初始化添加到模块初始化 */
static int __init init_ramfs_fs(void)
{
    int ret;
    
    ret = ramfs_init_proc();
    if (ret)
        pr_warn("RAMfs: Failed to initialize procfs interface\n");
        
    return register_filesystem(&ramfs_fs_type);
}

/* 将 proc 清理添加到模块退出 */
static void __exit exit_ramfs_fs(void)
{
    ramfs_exit_proc();
    unregister_filesystem(&ramfs_fs_type);
    kfree(ramfs_sync_dir);
}

module_init(init_ramfs_fs)
module_exit(exit_ramfs_fs)

