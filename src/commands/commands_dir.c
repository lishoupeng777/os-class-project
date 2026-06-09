#include "vfs.h"

// 校验文件名/目录名是否合法：不能为空、不能含 '/'、不能是 "." 或 ".."
int is_valid_name(const char *name) {
    if (name == NULL || *name == '\0') return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (const char *p = name; *p; p++) {
        if (*p == '/') return 0;
    }
    return 1;
}

void cmd_mkdir(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter directory name!\n");
        return;
    }
    
    char dirname[DIRSIZ];
    strncpy(dirname, arg, DIRSIZ - 1);
    dirname[DIRSIZ - 1] = '\0';
    
    if (!is_valid_name(dirname)) {
        printf("Invalid directory name!\n");
        return;
    }
    
    minode *dp = iget(current_user->u_cwd->ino);
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, dirname) == 0) {
            printf("Directory already exists!\n");
            iput(dp);
            return;
        }
    }
    
    if (access(dp, O_WRONLY) != 0) {
        printf("Permission denied!\n");
        iput(dp);
        return;
    }
    
    int ino = ialloc();
    minode *new_dir = iget(ino);
    new_dir->dino.di_mode = S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC
                          | (S_IREAD >> 3) | (S_IWRITE >> 3) | (S_IEXEC >> 3)
                          | (S_IREAD >> 6) | (S_IWRITE >> 6) | (S_IEXEC >> 6);
    new_dir->dino.di_nlink = 2;
    new_dir->dino.di_uid = current_user->u_uid;
    new_dir->dino.di_gid = current_user->u_gid;
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
    
    printf("Directory created successfully!\n");
}

void cmd_chdir(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter directory name!\n");
        return;
    }
    
    minode *new_cwd = NULL;
    if (namei(arg, &new_cwd) != 0) {
        printf("Directory not found!\n");
        return;
    }
    
    if ((new_cwd->dino.di_mode & S_IFDIR) == 0) {
        printf("Not a directory!\n");
        iput(new_cwd);
        return;
    }
    
    if (access(new_cwd, O_RDONLY) != 0) {
        printf("Permission denied!\n");
        iput(new_cwd);
        return;
    }
    
    iput(current_user->u_cwd);
    current_user->u_cwd = new_cwd;
    printf("Current directory changed!\n");
}

void cmd_dir(char *arg) {
    (void)arg;
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    minode *dp = iget(current_user->u_cwd->ino);
    
    if ((dp->dino.di_mode & S_IFDIR) == 0) {
        iput(dp);
        return;
    }
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    
    printf("\n");
    for (int i = 0; i < DIRNUM && de[i].de_ino != 0; i++) {
        minode *mip = iget(de[i].de_ino);
        char type = '-';
        if ((mip->dino.di_mode & S_IFDIR) == S_IFDIR) type = 'd';
        else if ((mip->dino.di_mode & S_IFLNK) == S_IFLNK) type = 'l';
        
        char perm[10] = "---------";
        if (mip->dino.di_mode & S_IREAD) perm[0] = 'r';
        if (mip->dino.di_mode & S_IWRITE) perm[1] = 'w';
        if (mip->dino.di_mode & S_IEXEC) perm[2] = 'x';
        if (mip->dino.di_mode & (S_IREAD >> 3)) perm[3] = 'r';
        if (mip->dino.di_mode & (S_IWRITE >> 3)) perm[4] = 'w';
        if (mip->dino.di_mode & (S_IEXEC >> 3)) perm[5] = 'x';
        if (mip->dino.di_mode & (S_IREAD >> 6)) perm[6] = 'r';
        if (mip->dino.di_mode & (S_IWRITE >> 6)) perm[7] = 'w';
        if (mip->dino.di_mode & (S_IEXEC >> 6)) perm[8] = 'x';
        
        printf("%c%s %d %d %d %s\n", type, perm, mip->dino.di_nlink, 
               mip->dino.di_uid, mip->dino.di_size, de[i].de_name);
        iput(mip);
    }
    printf("\n");
    
    iput(dp);
}

void cmd_rmdir(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter directory name!\n");
        return;
    }
    
    minode *target = NULL;
    if (namei(arg, &target) != 0) {
        printf("Directory not found!\n");
        return;
    }
    
    if ((target->dino.di_mode & S_IFDIR) == 0) {
        printf("Not a directory!\n");
        iput(target);
        return;
    }
    
    // 禁止删除 . 和 ..
    char dirname[DIRSIZ];
    strncpy(dirname, arg, DIRSIZ - 1);
    dirname[DIRSIZ - 1] = '\0';
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        printf("Cannot delete '.' or '..'!\n");
        iput(target);
        return;
    }
    
    // 禁止删除根目录
    if (target->ino == ROOT_INODE) {
        printf("Cannot delete root directory!\n");
        iput(target);
        return;
    }
    
    // 检查目录是否为空（只有 . 和 ..）
    // 目录大小按实际目录项数量维护，避免把块内未使用区域当成有效项。
    if (target->dino.di_size > 2 * sizeof(dir_entry)) {
        printf("Directory not empty!\n");
        iput(target);
        return;
    }

    int blk = target->dino.di_addr[0];
    
    // 检查目录是否在系统打开文件表中被使用
    for (int i = 0; i < SYSOPENFILE; i++) {
        if (sys_ofile[i].of_minode != NULL &&
            sys_ofile[i].of_minode->ino == target->ino &&
            sys_ofile[i].of_count > 0) {
            printf("Error: Directory is currently open!\n");
            iput(target);
            return;
        }
    }
    
    // 获取父目录
    dir_entry *target_de = (dir_entry *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
    int parent_ino = target_de[1].de_ino; // ".." 指向父目录
    minode *parent = iget(parent_ino);
    
    if (parent == NULL) {
        printf("Error: Cannot access parent directory!\n");
        iput(target);
        return;
    }
    
    if (access(parent, O_WRONLY) != 0) {
        printf("Permission denied!\n");
        iput(target);
        iput(parent);
        return;
    }
    
    // 在父目录中查找并删除对应的目录项
    dir_entry *pde = (dir_entry *)(virtual_disk + DATASTART + parent->dino.di_addr[0] * BLOCKSIZ);
    int found_idx = -1;
    int last_idx = -1;
    for (int i = 0; i < DIRNUM; i++) {
        if (pde[i].de_ino == 0) { last_idx = i; break; }
        last_idx = i + 1;
        if (pde[i].de_ino == target->ino) found_idx = i;
    }
    
    if (found_idx < 0) {
        printf("Error: Directory entry not found in parent!\n");
        iput(target);
        iput(parent);
        return;
    }
    
    // 释放目录的数据块
    if (blk > 0) {
        bfree(blk);
    }
    
    // 释放 inode
    int deleted_ino = target->ino;
    iput(target);
    ifree(deleted_ino);
    
    // 删除父目录中的目录项（压缩空洞）
    for (int i = found_idx; i < last_idx - 1; i++) {
        pde[i] = pde[i + 1];
    }
    pde[last_idx - 1].de_ino = 0;
    pde[last_idx - 1].de_name[0] = '\0';
    parent->dino.di_size -= sizeof(dir_entry);
    parent->dino.di_nlink--;
    parent->m_flag = 1;
    iput(parent);
    
    printf("Directory removed successfully!\n");
}

void cmd_pwd(char *arg) {
    (void)arg;
    if (current_user == NULL) {
        printf("Not logged in!\n");
        return;
    }
    
    char path[MAXPATH];
    get_cwd_path(path, sizeof(path));
    printf("%s\n", path);
}
