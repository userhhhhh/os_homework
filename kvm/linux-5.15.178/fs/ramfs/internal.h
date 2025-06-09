/* SPDX-License-Identifier: GPL-2.0-or-later */
/* internal.h: ramfs internal definitions
 *
 * Copyright (C) 2005 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */


extern const struct inode_operations ramfs_file_inode_operations;

// ------------------code added------------------

/* 持久化支持相关函数和变量 */
extern char *ramfs_sync_dir;
extern int ramfs_bind(const char *sync_dir);
extern int ramfs_file_flush(struct file *file);
// ------------------code added------------------