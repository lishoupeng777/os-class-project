#include "vfs.h"

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
    new_dir->dino.di_mode = S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC;
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
