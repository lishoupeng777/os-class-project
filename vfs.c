#include "vfs.h"

// 全局变量声明
char *virtual_disk = NULL;        // 虚拟磁盘内存缓冲区
super_block *sb = NULL;           // 超级块指针
minode *hinode[NHINO] = {NULL};   // Inode Hash 表（NHINO=128）
sys_open_file sys_ofile[SYSOPENFILE];  // 全局打开文件表
user users[USERNUM];              // 用户表
user *current_user = NULL;        // 当前登录用户
int user_count = 0;               // 系统用户数

// 初始化内存结构
void init_structures() {
    // 清空打开文件表和用户表
    memset(sys_ofile, 0, sizeof(sys_ofile));
    memset(users, 0, sizeof(users));
    current_user = NULL;
    user_count = 0;
    
    // 清空 Inode Hash 表
    for (int i = 0; i < NHINO; i++) {
        hinode[i] = NULL;
    }
}

// 格式化虚拟磁盘
// 初始化文件系统结构、超级块、根目录等
void format_disk() {
    printf("Formatting virtual disk filesystem...\n");
    
    // 分配虚拟磁盘内存（4MB）
    if (virtual_disk == NULL) {
        virtual_disk = (char *)malloc(DISK_SIZE);
        if (virtual_disk == NULL) {
            printf("Memory allocation failed!\n");
            exit(1);
        }
    }
    memset(virtual_disk, 0, DISK_SIZE);
    
    // 初始化超级块（Block 1）
    sb = (super_block *)(virtual_disk + BLOCKSIZ);
    sb->isize = DINODEBLK;      // Inode 表占 32 块
    sb->fsize = FILEBLK;        // 数据区 512 块
    sb->s_modified = 1;
    
    // 初始化空闲 Inode 栈（前 50 个 inode 号）
    for (int i = 0; i < NICINOD; i++) {
        sb->ifree[i] = i + 1;   // Inode 号从 1 开始
    }
    sb->ifree_num = NICINOD;
    sb->ifree_ptr = NICINOD - 1;
    
    // 初始化空闲块栈（前 50 个块号）
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
    
    int root_ino = ialloc();
    minode *root = iget(root_ino);
    root->dino.di_mode = S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC;
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
    sb->s_modified = 0;
    
    strcpy(users[0].u_name, "root");
    strcpy(users[0].u_passwd, "root");
    users[0].u_uid = 0;
    users[0].u_gid = 0;
    memset(users[0].u_ofile, -1, sizeof(users[0].u_ofile));
    users[0].u_cwd = iget(root_ino);
    user_count = 1;
    
    printf("Format completed!\n");
}

int load_from_disk() {
    FILE *fp = fopen(DISK_FILE, "rb");
    if (fp == NULL) {
        return 0;
    }
    
    virtual_disk = (char *)malloc(DISK_SIZE);
    if (virtual_disk == NULL) {
        fclose(fp);
        return 0;
    }
    
    size_t n = fread(virtual_disk, 1, DISK_SIZE, fp);
    fclose(fp);
    
    if (n == 0) {
        free(virtual_disk);
        virtual_disk = NULL;
        return 0;
    }
    
    sb = (super_block *)(virtual_disk + BLOCKSIZ);
    printf("Successfully restored system state from virtual disk file.\n");
    return 1;
}

void save_to_disk() {
    if (virtual_disk != NULL) {
        FILE *fp = fopen(DISK_FILE, "wb");
        if (fp != NULL) {
            fwrite(virtual_disk, 1, DISK_SIZE, fp);
            fclose(fp);
            printf("Filesystem state saved to host disk successfully.\n");
        } else {
            printf("Save failed!\n");
        }
    }
}

int balloc() {
    if (sb->ffree_num == 0) {
        printf("Disk space exhausted!\n");
        return -1;
    }
    
    int blkno = sb->ffree[sb->ffree_ptr--];
    sb->ffree_num--;
    
    // 隐患修复 #4: 多级寻址越界检查
    if (blkno < DATASTART / BLOCKSIZ || blkno >= DATASTART / BLOCKSIZ + FILEBLK) {
        printf("[ERROR] balloc: invalid block number %d (expected range [%d, %d))\n",
               blkno, DATASTART / BLOCKSIZ, DATASTART / BLOCKSIZ + FILEBLK);
        return -1;
    }
    
    if (sb->ffree_ptr < 0 && sb->ffree_num > 0) {
        int next_block = blkno;
        int *p = (int *)(virtual_disk + DATASTART + next_block * BLOCKSIZ);
        sb->ffree_ptr = NICFREE - 1;
        sb->ffree_num = NICFREE;
        for (int i = 0; i < NICFREE; i++) {
            sb->ffree[i] = p[i];
        }
        blkno = sb->ffree[sb->ffree_ptr--];
        sb->ffree_num--;
    }
    
    sb->s_modified = 1;
    return blkno;
}

void bfree(int blkno) {
    if (blkno < 0 || blkno >= FILEBLK) {
        return;
    }
    
    if (sb->ffree_ptr >= NICFREE - 1) {
        int *p = (int *)(virtual_disk + DATASTART + blkno * BLOCKSIZ);
        for (int i = 0; i <= sb->ffree_ptr; i++) {
            p[i] = sb->ffree[i];
        }
        p[sb->ffree_ptr + 1] = -1;
        sb->ffree[0] = blkno;
        sb->ffree_ptr = 0;
    } else {
        sb->ffree[++sb->ffree_ptr] = blkno;
    }
    sb->ffree_num++;
    sb->s_modified = 1;
}

int ialloc() {
    if (sb->ifree_num == 0) {
        printf("No more i-nodes available!\n");
        return -1;
    }
    
    int ino = sb->ifree[sb->ifree_ptr--];
    sb->ifree_num--;
    sb->s_modified = 1;
    
    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memset(dip, 0, DINODESIZ);
    dip->di_mode = IALLOC;
    
    return ino;
}

void ifree(int ino) {
    if (ino < 1 || ino > DINODEBLK * BLOCKSIZ / DINODESIZ) {
        return;
    }
    
    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memset(dip, 0, DINODESIZ);
    dip->di_mode = IFREE;
    
    if (sb->ifree_ptr < NICINOD - 1) {
        sb->ifree[++sb->ifree_ptr] = ino;
        sb->ifree_num++;
        sb->s_modified = 1;
    }
}

minode *iget(int ino) {
    if (ino < 1) return NULL;
    
    minode **hash_head = ihash(ino);
    minode *mip = *hash_head;
    
    while (mip != NULL) {
        if (mip->ino == ino) {
            mip->ref_count++;
            return mip;
        }
        mip = mip->next;
    }
    
    mip = (minode *)malloc(sizeof(minode));
    if (mip == NULL) return NULL;
    
    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memcpy(&mip->dino, dip, DINODESIZ);
    mip->dev = 0;
    mip->ino = ino;
    mip->ref_count = 1;
    mip->m_flag = 0;
    
    mip->next = *hash_head;
    mip->prev = NULL;
    if (*hash_head != NULL) {
        (*hash_head)->prev = mip;
    }
    *hash_head = mip;
    
    return mip;
}

void iput(minode *mip) {
    if (mip == NULL) return;
    
    // 隐患修复 #2: Inode 引用计数泄漏检查
    if (mip->ref_count <= 0) {
        printf("[WARNING] iput() called on inode with ref_count=%d (possible leak)\n", 
               mip->ref_count);
        return;
    }
    
    mip->ref_count--;
    if (mip->ref_count > 0) return;
    
    if (mip->m_flag) {
        dinode *dip = (dinode *)(virtual_disk + DINODESTART + (mip->ino - 1) * DINODESIZ);
        memcpy(dip, &mip->dino, DINODESIZ);
        sb->s_modified = 1;
    }
    
    minode **hash_head = ihash(mip->ino);
    if (mip->prev != NULL) {
        mip->prev->next = mip->next;
    } else {
        *hash_head = mip->next;
    }
    if (mip->next != NULL) {
        mip->next->prev = mip->prev;
    }
    
    free(mip);
}

// 隐患修复 #2: 缓存健康检查函数
void check_inode_cache_health() {
    int total_ref_count = 0;
    int leaked_count = 0;
    
    for (int i = 0; i < NHINO; i++) {
        minode *mip = hinode[i];
        while (mip != NULL) {
            total_ref_count += mip->ref_count;
            // 检查是否存在孤立的高引用计数inode
            if (mip->ref_count > 5) {
                printf("[WARNING] Inode %d has high ref_count=%d (possible leak)\n",
                       mip->ino, mip->ref_count);
                leaked_count++;
            }
            mip = mip->next;
        }
    }
    
    if (total_ref_count > 0 || leaked_count > 0) {
        printf("[CACHE STATUS] Total ref_count=%d, Suspected leaks=%d\n", 
               total_ref_count, leaked_count);
    }
}

int namei(char *path, minode **mip) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return -1;
    }
    
    // 隐患修复 #3: 符号链接循环检测 - 深度限制
    #define MAX_SYMLINK_DEPTH 10
    int symlink_depth = 0;
    
    minode *cur = NULL;
    char tmp[MAXPATH];
    char *p, *q;
    
    strcpy(tmp, path);
    
    if (tmp[0] == '/') {
        cur = iget(ROOT_INODE);
        p = tmp + 1;
    } else {
        cur = iget(current_user->u_cwd->ino);
        p = tmp;
    }
    
    if ((cur->dino.di_mode & S_IFDIR) == 0) {
        iput(cur);
        return -1;
    }
    
    while (*p != '\0') {
        // 检查符号链接深度
        if (symlink_depth > MAX_SYMLINK_DEPTH) {
            printf("Error: Too many symbolic links (possible cycle)\n");
            iput(cur);
            return -1;
        }
        
        q = p;
        while (*q != '\0' && *q != '/') q++;
        char name[DIRSIZ];
        int len = q - p;
        if (len >= DIRSIZ) len = DIRSIZ - 1;
        strncpy(name, p, len);
        name[len] = '\0';
        
        dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + cur->dino.di_addr[0] * BLOCKSIZ);
        int found = 0;
        for (int i = 0; i < DIRNUM; i++) {
            if (de[i].de_ino == 0) break;
            if (strcmp(de[i].de_name, name) == 0) {
                int ino = de[i].de_ino;
                iput(cur);
                cur = iget(ino);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            iput(cur);
            return -1;
        }
        
        if (*q == '/') p = q + 1;
        else p = q;
    }
    
    *mip = cur;
    return 0;
}

int iname(minode *dp, char *name, int *ino) {
    if ((dp->dino.di_mode & S_IFDIR) == 0) {
        return -1;
    }
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) {
            strncpy(de[i].de_name, name, DIRSIZ - 1);
            de[i].de_name[DIRSIZ - 1] = '\0';
            de[i].de_ino = *ino;
            dp->dino.di_size += sizeof(dir_entry);
            dp->m_flag = 1;
            return 0;
        }
        if (strcmp(de[i].de_name, name) == 0) {
            return -1;
        }
    }
    
    return -1;
}

int access(minode *mip, int mode) {
    if (current_user == NULL) return -1;
    
    // Root user (uid=0) bypasses all permission checks
    if (current_user->u_uid == 0) return 0;
    
    uint16_t perm = mip->dino.di_mode;
    
    // === THREE-LEVEL PERMISSION MODEL ===
    // Bits 8-10 (0o0700): Owner (user) permissions - checked at bits 8-10 (no shift)
    // Bits 5-7  (0o0070): Group permissions       - checked at bits 5-7  (>>3)
    // Bits 2-4  (0o0007): Other permissions       - checked at bits 2-4  (>>6)
    
    // S_IREAD  = 0x0100 (bit 8, owner read)
    // S_IWRITE = 0x0080 (bit 7, owner write)
    // S_IEXEC  = 0x0040 (bit 6, owner execute)
    
    // Check owner permissions first
    if (current_user->u_uid == mip->dino.di_uid) {
        // Owner permissions (no shift needed)
        if ((mode & O_RDONLY) && !(perm & S_IREAD)) return -1;
        if ((mode & O_WRONLY) && !(perm & S_IWRITE)) return -1;
        if ((mode & O_RDWR) && !((perm & S_IREAD) && (perm & S_IWRITE))) return -1;
        return 0;
    }
    
    // Check group permissions if user is in file's group
    if (current_user->u_gid == mip->dino.di_gid) {
        // Group permissions (shifted right by 3 bits: 0o0070)
        // (S_IREAD >> 3)  = 0x0020 (group read bit)
        // (S_IWRITE >> 3) = 0x0010 (group write bit)
        // (S_IEXEC >> 3)  = 0x0008 (group execute bit)
        if ((mode & O_RDONLY) && !(perm & (S_IREAD >> 3))) return -1;
        if ((mode & O_WRONLY) && !(perm & (S_IWRITE >> 3))) return -1;
        if ((mode & O_RDWR) && !((perm & (S_IREAD >> 3)) && (perm & (S_IWRITE >> 3)))) return -1;
        return 0;
    }
    
    // Check other permissions (all other users)
    // Other permissions (shifted right by 6 bits: 0o0007)
    // (S_IREAD >> 6)  = 0x0004 (other read bit)
    // (S_IWRITE >> 6) = 0x0002 (other write bit)
    // (S_IEXEC >> 6)  = 0x0001 (other execute bit)
    if ((mode & O_RDONLY) && !(perm & (S_IREAD >> 6))) return -1;
    if ((mode & O_WRONLY) && !(perm & (S_IWRITE >> 6))) return -1;
    if ((mode & O_RDWR) && !((perm & (S_IREAD >> 6)) && (perm & (S_IWRITE >> 6)))) return -1;
    
    return 0;
}

int alloc_block(minode *mip, int offset) {
    int blkno = offset / BLOCKSIZ;
    int addr_idx;
    int indirect_blocks = (int)(BLOCKSIZ / sizeof(int));
    
    if (blkno < 8) {
        addr_idx = blkno;
        if (mip->dino.di_addr[addr_idx] == 0) {
            mip->dino.di_addr[addr_idx] = balloc();
            mip->m_flag = 1;
        }
        return mip->dino.di_addr[addr_idx];
    } else if (blkno < 8 + indirect_blocks) {
        blkno -= 8;
        addr_idx = 8;
        if (mip->dino.di_addr[addr_idx] == 0) {
            mip->dino.di_addr[addr_idx] = balloc();
            mip->m_flag = 1;
            int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
            memset(indirect, 0, BLOCKSIZ);
        }
        int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        if (indirect[blkno] == 0) {
            indirect[blkno] = balloc();
            mip->m_flag = 1;
        }
        return indirect[blkno];
    } else {
        blkno -= 8 + BLOCKSIZ / sizeof(int);
        int first_idx = blkno / (BLOCKSIZ / sizeof(int));
        int second_idx = blkno % (BLOCKSIZ / sizeof(int));
        addr_idx = 9;
        
        if (mip->dino.di_addr[addr_idx] == 0) {
            mip->dino.di_addr[addr_idx] = balloc();
            mip->m_flag = 1;
            int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
            memset(dbl_indirect, 0, BLOCKSIZ);
        }
        
        int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        if (dbl_indirect[first_idx] == 0) {
            dbl_indirect[first_idx] = balloc();
            mip->m_flag = 1;
            int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
            memset(indirect, 0, BLOCKSIZ);
        }
        
        int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
        if (indirect[second_idx] == 0) {
            indirect[second_idx] = balloc();
            mip->m_flag = 1;
        }
        return indirect[second_idx];
    }
}