#include "vfs.h"

// ============================================================
// 磁盘 I/O 与文件系统生命周期
// 负责：初始化、格式化、加载、保存
// ============================================================

// 用户持久化存储位置：虚拟磁盘末尾的预留区域
#define USER_SAVE_OFFSET (DISK_SIZE - BLOCKSIZ)

// ----------------------------------------------------------
// 初始化内存结构
// ----------------------------------------------------------
void init_structures() {
    memset(sys_ofile, 0, sizeof(sys_ofile));
    memset(users, 0, sizeof(users));
    current_user = NULL;
    user_count = 0;

    for (int i = 0; i < NHINO; i++) {
        hinode[i] = NULL;
    }
}

// ----------------------------------------------------------
// 将用户表序列化写入磁盘缓冲区中的预留区域
// ----------------------------------------------------------
static void save_users_to_buffer() {
    char *user_save = virtual_disk + USER_SAVE_OFFSET;
    *(int *)user_save = user_count;
    user_save += sizeof(int);

    for (int i = 0; i < user_count; i++) {
        memset(user_save, 0, DIRSIZ);
        strncpy(user_save, users[i].u_name, DIRSIZ - 1);
        user_save += DIRSIZ;

        memset(user_save, 0, PWDSIZ);
        strncpy(user_save, users[i].u_passwd, PWDSIZ - 1);
        user_save += PWDSIZ;

        *(uint8_t *)user_save = users[i].u_uid;
        user_save += sizeof(uint8_t);

        *(uint8_t *)user_save = users[i].u_gid;
        user_save += sizeof(uint8_t);

        // 文件描述符不保存到磁盘
        memset(users[i].u_ofile, -1, sizeof(users[i].u_ofile));
        user_save += sizeof(users[i].u_ofile);

        uint16_t cwd_ino = (users[i].u_cwd != NULL) ? (uint16_t)users[i].u_cwd->ino : 0;
        *(uint16_t *)user_save = cwd_ino;
        user_save += sizeof(uint16_t);
    }
}

// ----------------------------------------------------------
// 从磁盘缓冲区中的预留区域反序列化用户表
// ----------------------------------------------------------
static void load_users_from_buffer() {
    char *user_save = virtual_disk + USER_SAVE_OFFSET;
    user_count = *(int *)user_save;
    user_save += sizeof(int);

    if (user_count < 0 || user_count > USERNUM) {
        user_count = 0;
        printf("Warning: User data corrupted, resetting user table.\n");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        memset(users[i].u_name, 0, DIRSIZ);
        strncpy(users[i].u_name, user_save, DIRSIZ - 1);
        user_save += DIRSIZ;

        memset(users[i].u_passwd, 0, PWDSIZ);
        strncpy(users[i].u_passwd, user_save, PWDSIZ - 1);
        user_save += PWDSIZ;

        users[i].u_uid = *(uint8_t *)user_save;
        user_save += sizeof(uint8_t);

        users[i].u_gid = *(uint8_t *)user_save;
        user_save += sizeof(uint8_t);

        memset(users[i].u_ofile, -1, sizeof(users[i].u_ofile));
        user_save += sizeof(users[i].u_ofile);

        uint16_t cwd_ino = *(uint16_t *)user_save;
        user_save += sizeof(uint16_t);

        if (cwd_ino > 0) {
            users[i].u_cwd = iget(cwd_ino);
        } else {
            users[i].u_cwd = NULL;
        }
    }
}

// ----------------------------------------------------------
// 在指定目录下创建子目录
// 参数: parent_inode - 父目录 inode 号, dirname - 子目录名, uid/gid - 所有者
// 返回值: 子目录 inode 号, -1 失败
// ----------------------------------------------------------
int create_subdir(int parent_ino, char *dirname, uint8_t uid, uint8_t gid) {
    minode *dp = iget(parent_ino);
    if (dp == NULL) return -1;

    int ino = ialloc();
    if (ino < 0) { iput(dp); return -1; }

    minode *new_dir = iget(ino);
    new_dir->dino.di_mode = S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC
                          | (S_IREAD >> 3) | (S_IWRITE >> 3) | (S_IEXEC >> 3)
                          | (S_IREAD >> 6) | (S_IWRITE >> 6) | (S_IEXEC >> 6);
    new_dir->dino.di_nlink = 2;
    new_dir->dino.di_uid = uid;
    new_dir->dino.di_gid = gid;
    new_dir->dino.di_size = 2 * sizeof(dir_entry);
    memset(new_dir->dino.di_addr, 0, sizeof(new_dir->dino.di_addr));

    int blk = balloc();
    new_dir->dino.di_addr[0] = blk;
    new_dir->m_flag = 1;

    dir_entry *new_de = (dir_entry *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
    strcpy(new_de[0].de_name, ".");
    new_de[0].de_ino = ino;
    strcpy(new_de[1].de_name, "..");
    new_de[1].de_ino = dp->ino;

    iput(new_dir);

    iname(dp, dirname, &ino);
    dp->dino.di_nlink++;
    dp->m_flag = 1;
    iput(dp);

    return ino;
}

// ----------------------------------------------------------
// 创建根目录
// ----------------------------------------------------------
static int create_root_directory() {
    int root_ino = ialloc();
    if (root_ino < 0) return -1;

    minode *root = iget(root_ino);
    // 根目录只给 root 完全权限，其他人只有读和执行
    root->dino.di_mode = S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC
                       | (S_IREAD >> 3) | (S_IEXEC >> 3)
                       | (S_IREAD >> 6) | (S_IEXEC >> 6);
    root->dino.di_nlink = 2;
    root->dino.di_uid = 0;
    root->dino.di_gid = 0;
    root->dino.di_size = 0;
    memset(root->dino.di_addr, 0, sizeof(root->dino.di_addr));
    root->m_flag = 1;
    iput(root);

    root = iget(root_ino);
    int blk = balloc();
    root->dino.di_addr[0] = blk;
    root->dino.di_size = 2 * sizeof(dir_entry);
    root->m_flag = 1;

    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
    strcpy(de[0].de_name, ".");
    de[0].de_ino = root_ino;
    strcpy(de[1].de_name, "..");
    de[1].de_ino = root_ino;

    iput(root);
    return root_ino;
}

// ----------------------------------------------------------
// 初始化超级块
// ----------------------------------------------------------
static void init_super_block() {
    sb = (super_block *)(virtual_disk + BLOCKSIZ);
    sb->isize = DINODEBLK;
    sb->fsize = FILEBLK;
    sb->s_modified = 1;

    for (int i = 0; i < NICINOD; i++) {
        sb->ifree[i] = i + 1;
    }
    sb->ifree_num = NICINOD;
    sb->ifree_ptr = NICINOD - 1;

    int blkno = 0;
    for (int i = 0; i < NICFREE; i++) {
        sb->ffree[i] = blkno++;
    }
    sb->ffree_num = NICFREE;
    sb->ffree_ptr = NICFREE - 1;

    for (int i = NICFREE; i < FILEBLK; i += NICFREE) {
        int block = i / NICFREE + DINODEBLK + 2;
        int *p = (int *)(virtual_disk + block * BLOCKSIZ);
        int count = (i + NICFREE < FILEBLK) ? NICFREE : FILEBLK - i;
        for (int j = 0; j < count; j++) {
            p[j] = i + j;
        }
        p[count] = -1;
    }
}

// ----------------------------------------------------------
// 创建初始 root 用户及 /home 目录
// ----------------------------------------------------------
static void create_root_user(int root_ino) {
    strcpy(users[0].u_name, "root");
    strcpy(users[0].u_passwd, "root");
    users[0].u_uid = 0;
    users[0].u_gid = 0;
    memset(users[0].u_ofile, -1, sizeof(users[0].u_ofile));
    users[0].u_cwd = iget(root_ino);
    user_count = 1;

    // root 自动创建 /home 目录（共享但各用户只能进自己的）
    int home_ino = create_subdir(root_ino, "home", 0, 0);
    if (home_ino > 0) {
        printf("  Created /home directory\n");
    }
}

// ----------------------------------------------------------
// 格式化虚拟磁盘
// ----------------------------------------------------------
void format_disk() {
    printf("Formatting virtual disk filesystem...\n");

    if (virtual_disk == NULL) {
        virtual_disk = (char *)malloc(DISK_SIZE);
        if (virtual_disk == NULL) {
            printf("Memory allocation failed!\n");
            exit(1);
        }
    }
    memset(virtual_disk, 0, DISK_SIZE);

    init_super_block();

    int root_ino = create_root_directory();
    if (root_ino < 0) {
        printf("Root directory creation failed!\n");
        return;
    }

    sb->s_modified = 0;
    create_root_user(root_ino);
    save_users_to_buffer();

    printf("Format completed!\n");
}

// ----------------------------------------------------------
// 从虚拟磁盘文件加载
// ----------------------------------------------------------
int load_from_disk() {
    FILE *fp = fopen(DISK_FILE, "rb");
    if (fp == NULL) return 0;

    virtual_disk = (char *)malloc(DISK_SIZE);
    if (virtual_disk == NULL) { fclose(fp); return 0; }

    size_t n = fread(virtual_disk, 1, DISK_SIZE, fp);
    fclose(fp);

    if (n == 0) { free(virtual_disk); virtual_disk = NULL; return 0; }

    sb = (super_block *)(virtual_disk + BLOCKSIZ);
    load_users_from_buffer();

    printf("Successfully restored system state from virtual disk file.\n");
    return 1;
}

// ----------------------------------------------------------
// 保存整个虚拟磁盘到文件
// ----------------------------------------------------------
void save_to_disk() {
    if (virtual_disk == NULL) return;

    save_users_to_buffer();

    FILE *fp = fopen(DISK_FILE, "wb");
    if (fp != NULL) {
        fwrite(virtual_disk, 1, DISK_SIZE, fp);
        fclose(fp);
        printf("Filesystem state saved to host disk successfully.\n");
    } else {
        printf("Save failed!\n");
    }
}