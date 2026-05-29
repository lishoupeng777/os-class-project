#include "vfs.h"

// ============================================================
// 全局变量定义
// 这些变量在整个文件系统中共享访问
// ============================================================

char *virtual_disk = NULL;        // 虚拟磁盘内存缓冲区
super_block *sb = NULL;           // 超级块指针
minode *hinode[NHINO] = {NULL};   // Inode Hash 表（NHINO=128）
sys_open_file sys_ofile[SYSOPENFILE];  // 全局打开文件表
user users[USERNUM];              // 用户表
user *current_user = NULL;        // 当前登录用户
int user_count = 0;               // 系统用户数