#ifndef VFS_H
#define VFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 虚拟磁盘参数
#define BLOCKSIZ 512           // 块大小（字节）
#define SYSOPENFILE 40         // 系统打开文件表项数
#define DIRNUM 128             // 每目录最大条目数
#define DIRSIZ 14              // 目录条目名最大长度
#define PWDSIZ 12              // 密码最大长度
#define PWDNUM 32              // 用户密码表大小
#define NOFILE 20              // 每用户最大打开文件数
#define NADDR 10               // Inode 中的块地址数（8直接 + 1一级间接 + 1二级间接）
#define NHINO 128              // Inode Hash 表项数
#define USERNUM 10             // 最大用户数
#define DINODESIZ 32           // Dinode 大小（字节）
#define DINODEBLK 32           // Inode 表块数
#define FILEBLK 512            // 数据块总数
#define NICFREE 50             // 超级块中的空闲块栈大小
#define NICINOD 50             // 超级块中的空闲 Inode 栈大小
#define DINODESTART (2 * BLOCKSIZ)                    // Inode 表起始地址
#define DATASTART ((2 + DINODEBLK) * BLOCKSIZ)        // 数据块起始地址

#define DISK_FILE "virtual_disk.bin"  // 虚拟磁盘文件名
#define DISK_SIZE (1024 * 1024 * 4)   // 虚拟磁盘大小（4MB）

#define SUPER_BLOCK 1     // 超级块编号
#define ROOT_INODE 1      // 根目录 Inode 编号

// Inode 模式位
#define IALLOC 0x8000     // Inode 已分配标志
#define IFREE 0x0000      // Inode 空闲标志

// 文件类型位
#define S_IFREG 0x8000    // 普通文件
#define S_IFDIR 0x4000    // 目录
#define S_IFLNK 0xA000    // 符号链接（symlink）

// 权限位
#define S_IREAD 0x0100    // 所有者读权限
#define S_IWRITE 0x0080   // 所有者写权限
#define S_IEXEC 0x0040    // 所有者执行权限

// 文件打开模式
#define O_RDONLY 0x01     // 只读
#define O_WRONLY 0x02     // 只写
#define O_RDWR 0x03       // 读写
#define O_APPEND 0x08     // 追加

#define MAXPATH 256        // 最大路径长度
#define MAXLINE 256        // 最大命令行长度

// 超级块结构：存储文件系统元数据
typedef struct super_block {
    int isize;              // Inode 表大小（块数）
    int ifree_num;          // 空闲 Inode 数量
    int ifree[NICINOD];     // 空闲 Inode 号栈
    int ifree_ptr;          // 空闲 Inode 栈指针
    int fsize;              // 数据块总数
    int ffree_num;          // 空闲块数量
    int ffree[NICFREE];     // 空闲块号栈（FAT-like）
    int ffree_ptr;          // 空闲块栈指针
    int s_modified;         // 修改标志
} super_block;

// 磁盘 Inode：存储在磁盘上的文件元数据
typedef struct dinode {
    uint16_t di_mode;       // 文件类型和权限（类型在高 4 位，权限在低 12 位）
    uint8_t di_nlink;       // 硬链接计数
    uint8_t di_uid;         // 文件所有者 UID
    uint16_t di_gid;        // 文件所有者 GID
    uint32_t di_size;       // 文件大小（字节）
    uint32_t di_addr[NADDR];// 块地址数组（0-7直接，8为一级间接，9为二级间接）
    uint16_t di_atime;      // 最后访问时间
    uint16_t di_mtime;      // 最后修改时间
} dinode;

// 内存 Inode：缓存在内存中的 Inode（含附加管理信息）
typedef struct minode {
    dinode dino;            // 磁盘 Inode 副本
    int dev;                // 设备号
    int ino;                // Inode 号
    int ref_count;          // 引用计数（缓存管理用）
    int m_flag;             // 修改标志（需写回磁盘时为 1）
    struct minode *next;    // Hash 链表指针
    struct minode *prev;
} minode;

// 目录项：存储在目录数据块中
typedef struct dir_entry {
    char de_name[DIRSIZ];   // 文件/目录名（最多 14 字符）
    uint16_t de_ino;        // 对应的 Inode 号
} dir_entry;

// 系统打开文件表项：全局维护
typedef struct sys_open_file {
    int of_mode;            // 打开模式（O_RDONLY/O_WRONLY/O_RDWR）
    int of_count;           // 打开计数
    minode *of_minode;      // 指向内存 Inode
    int of_offset;          // 当前读写位置
} sys_open_file;

// 用户结构：用户信息和相关资源
typedef struct user {
    char u_name[DIRSIZ];    // 用户名
    char u_passwd[PWDSIZ];  // 密码
    uint8_t u_uid;          // 用户 ID
    uint8_t u_gid;          // 组 ID
    int u_ofile[NOFILE];    // 用户打开文件表（系统打开文件表的索引）
    minode *u_cwd;          // 当前工作目录 Inode
} user;

extern char *virtual_disk;
extern super_block *sb;
extern minode *hinode[NHINO];
extern sys_open_file sys_ofile[SYSOPENFILE];
extern user users[USERNUM];
extern user *current_user;
extern int user_count;

#define ihash(x) (&hinode[(int)(x) & 127])

void init_structures();
void format_disk();
int load_from_disk();
void save_to_disk();

int balloc();
void bfree(int blkno);
int ialloc();
void ifree(int ino);
minode *iget(int ino);
void iput(minode *mip);
int namei(char *path, minode **mip);
int iname(minode *dp, char *name, int *ino);
int access(minode *mip, int mode);
int alloc_block(minode *mip, int offset);
char *get_cwd_path(char *buf, int size);

void cmd_format(char *arg);
void cmd_login(char *arg);
void cmd_logout(char *arg);
void cmd_mkdir(char *arg);
void cmd_chdir(char *arg);
void cmd_dir(char *arg);
void cmd_create(char *arg);
void cmd_open(char *arg);
void cmd_close(char *arg);
void cmd_read(char *arg);
void cmd_write(char *arg);
void cmd_delete(char *arg);
void cmd_copy(char *arg);
void cmd_link(char *arg);
void cmd_pwd(char *arg);
void cmd_touch(char *arg);
void cmd_cat(char *arg);
void cmd_chmod(char *arg);
void cmd_chown(char *arg);
void cmd_useradd(char *arg);
void cmd_userdel(char *arg);
void cmd_whoami(char *arg);
void cmd_stat(char *arg);
void cmd_rename(char *arg);
void cmd_passwd(char *arg);
void cmd_symlink(char *arg);

// 新增：在指定父目录下创建子目录（供创建用户 home 用）
int create_subdir(int parent_ino, char *dirname, uint8_t uid, uint8_t gid);

// 隐患修复相关函数声明
void check_inode_cache_health(void);  // 隐患 #2: 缓存泄漏检查

#endif