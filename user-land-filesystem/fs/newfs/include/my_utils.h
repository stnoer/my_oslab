#ifndef _MY_UYILS_H_
#define _MY_UYILS_H_

#include <bits/types.h>
typedef int    boolean;

typedef __uint8_t uint8_t;

/* 函数声明 */

/* 驱动操作 */
int my_read(int offset, uint8_t* out_content, int size);    // 驱动读操作
int my_write(int offset, uint8_t* in_content, int size);    // 驱动写操作

/* 索引节点管理 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry* dentry);  // 分配索引节点
int newfs_sync_inode(struct newfs_inode* inode);                    // 同步索引节点到磁盘
struct newfs_inode* newfs_read_inode(struct newfs_dentry* dentry, int ino); // 从磁盘读取索引节点

/* 目录项管理 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode* inode, int dir);  // 获取目录项
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry); // 分配目录项

/* 路径解析 */
char* newfs_get_fname(const char* path);        // 获取文件名
int newfs_calc_lvl(const char* path);          // 计算路径层级
struct newfs_dentry* newfs_lookup(const char* path, boolean* is_find, boolean* is_root); // 查找目录项

/* 文件系统挂载 */
int my_mount(struct custom_options options);   // 挂载文件系统

/* 文件系统取消挂载 */
int my_umount();

#endif // NEWFS_H
