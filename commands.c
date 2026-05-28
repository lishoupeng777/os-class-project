#include "vfs.h"

void cmd_format(char *arg) {
    (void)arg;
    format_disk();
}

void cmd_login(char *arg) {
    (void)arg;
    if (current_user != NULL) {
        printf("Please logout first!\n");
        return;
    }
    
    char name[DIRSIZ], passwd[PWDSIZ];
    printf("Username: ");
    scanf("%s", name);
    printf("Password: ");
    scanf("%s", passwd);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].u_name, name) == 0 && strcmp(users[i].u_passwd, passwd) == 0) {
            current_user = &users[i];
            current_user->u_cwd = iget(current_user->u_cwd->ino);
            printf("Login successful!\n");
            return;
        }
    }
    
    printf("Invalid username or password!\n");
}

void cmd_logout(char *arg) {
    (void)arg;
    if (current_user == NULL) {
        printf("Not logged in!\n");
        return;
    }
    
    for (int i = 0; i < NOFILE; i++) {
        if (current_user->u_ofile[i] != -1) {
            sys_open_file *of = &sys_ofile[current_user->u_ofile[i]];
            iput(of->of_minode);
            of->of_count--;
            current_user->u_ofile[i] = -1;
        }
    }
    
    iput(current_user->u_cwd);
    current_user = NULL;
    printf("Logout successful!\n");
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

void cmd_create(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    char filename[DIRSIZ];
    strncpy(filename, arg, DIRSIZ - 1);
    filename[DIRSIZ - 1] = '\0';
    
    minode *dp = iget(current_user->u_cwd->ino);
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, filename) == 0) {
            printf("File already exists!\n");
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
    minode *new_file = iget(ino);
    new_file->dino.di_mode = S_IFREG | S_IREAD | S_IWRITE;
    new_file->dino.di_nlink = 1;
    new_file->dino.di_uid = current_user->u_uid;
    new_file->dino.di_gid = current_user->u_gid;
    new_file->dino.di_size = 0;
    memset(new_file->dino.di_addr, 0, sizeof(new_file->dino.di_addr));
    new_file->m_flag = 1;
    iput(new_file);
    
    iname(dp, filename, &ino);
    iput(dp);
    
    printf("Do you want to write content now? (y/n): ");
    char choice[4];
    scanf("%s", choice);
    getchar();
    
    if (choice[0] == 'y' || choice[0] == 'Y') {
        printf("Enter content (type 'EOF' on a new line to end):\n");
        char content[MAXLINE * 100] = {0};
        char line[MAXLINE];
        while (fgets(line, MAXLINE, stdin) != NULL) {
            if (strcmp(line, "EOF\n") == 0 || strcmp(line, "EOF\r\n") == 0) {
                break;
            }
            strcat(content, line);
        }
        
        int fd = -1;
        for (int i = 0; i < NOFILE; i++) {
            if (current_user->u_ofile[i] == -1) {
                fd = i;
                break;
            }
        }
        
        if (fd != -1) {
            int sys_fd = -1;
            for (int i = 0; i < SYSOPENFILE; i++) {
                if (sys_ofile[i].of_minode == NULL) {
                    sys_fd = i;
                    break;
                }
            }
            
            if (sys_fd != -1) {
                minode *mip = iget(ino);
                sys_ofile[sys_fd].of_mode = O_WRONLY;
                sys_ofile[sys_fd].of_count = 1;
                sys_ofile[sys_fd].of_minode = mip;
                sys_ofile[sys_fd].of_offset = 0;
                current_user->u_ofile[fd] = sys_fd;
                
                int len = strlen(content);
                int offset = 0;
                while (offset < len) {
                    int blk = alloc_block(mip, offset);
                    int off_in_blk = offset % BLOCKSIZ;
                    int write_len = BLOCKSIZ - off_in_blk;
                    if (write_len > len - offset) write_len = len - offset;
                    
                    char *buf = virtual_disk + DATASTART + blk * BLOCKSIZ + off_in_blk;
                    memcpy(buf, content + offset, write_len);
                    
                    offset += write_len;
                }
                
                mip->dino.di_size = len;
                mip->m_flag = 1;
                
                iput(mip);
                current_user->u_ofile[fd] = -1;
                sys_ofile[sys_fd].of_minode = NULL;
                sys_ofile[sys_fd].of_count = 0;
            }
        }
        printf("Content written!\n");
    }
    
    printf("File created successfully!\n");
}

void cmd_open(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(arg, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    if (access(mip, O_RDWR) != 0) {
        printf("Permission denied!\n");
        iput(mip);
        return;
    }
    
    int fd = -1;
    for (int i = 0; i < NOFILE; i++) {
        if (current_user->u_ofile[i] == -1) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) {
        printf("Too many open files!\n");
        iput(mip);
        return;
    }
    
    int sys_fd = -1;
    for (int i = 0; i < SYSOPENFILE; i++) {
        if (sys_ofile[i].of_minode == NULL) {
            sys_fd = i;
            break;
        }
    }
    
    if (sys_fd == -1) {
        printf("System open file table is full!\n");
        iput(mip);
        return;
    }
    
    sys_ofile[sys_fd].of_mode = O_RDWR;
    sys_ofile[sys_fd].of_count = 1;
    sys_ofile[sys_fd].of_minode = mip;
    sys_ofile[sys_fd].of_offset = 0;
    current_user->u_ofile[fd] = sys_fd;
    
    printf("File opened, file descriptor: %d\n", fd);
}

void cmd_close(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter file descriptor!\n");
        return;
    }
    
    int fd = atoi(arg);
    if (fd < 0 || fd >= NOFILE) {
        printf("Invalid file descriptor!\n");
        return;
    }
    
    int sys_fd = current_user->u_ofile[fd];
    if (sys_fd == -1) {
        printf("File not open!\n");
        return;
    }
    
    iput(sys_ofile[sys_fd].of_minode);
    sys_ofile[sys_fd].of_count--;
    sys_ofile[sys_fd].of_minode = NULL;
    current_user->u_ofile[fd] = -1;
    
    printf("File closed!\n");
}

void cmd_read(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter file descriptor and size!\n");
        return;
    }
    
    int fd, size;
    if (sscanf(arg, "%d %d", &fd, &size) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    if (fd < 0 || fd >= NOFILE) {
        printf("Invalid file descriptor!\n");
        return;
    }
    
    int sys_fd = current_user->u_ofile[fd];
    if (sys_fd == -1) {
        printf("File not open!\n");
        return;
    }
    
    sys_open_file *of = &sys_ofile[sys_fd];
    minode *mip = of->of_minode;
    
    if ((of->of_mode & O_RDONLY) == 0 && (of->of_mode & O_RDWR) == 0) {
        printf("File not opened for reading!\n");
        return;
    }
    
    char *buf = (char *)malloc(size + 1);
    if (buf == NULL) {
        printf("Memory allocation failed!\n");
        return;
    }
    memset(buf, 0, size + 1);
    
    int offset = of->of_offset;
    int total_read = 0;
    uint32_t file_size = mip->dino.di_size;
    
    while (total_read < size && (uint32_t)offset < file_size) {
        int blk = alloc_block(mip, offset);
        int off_in_blk = offset % BLOCKSIZ;
        int read_len = BLOCKSIZ - off_in_blk;
        if (read_len > size - total_read) read_len = size - total_read;
        if (read_len > (int)(file_size - offset)) read_len = (int)(file_size - offset);
        
        char *src = virtual_disk + DATASTART + blk * BLOCKSIZ + off_in_blk;
        memcpy(buf + total_read, src, read_len);
        
        total_read += read_len;
        offset += read_len;
    }
    
    of->of_offset = offset;
    printf("Content read:\n%s\n", buf);
    free(buf);
}

void cmd_write(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter file descriptor!\n");
        return;
    }
    
    int fd = atoi(arg);
    if (fd < 0 || fd >= NOFILE) {
        printf("Invalid file descriptor!\n");
        return;
    }
    
    int sys_fd = current_user->u_ofile[fd];
    if (sys_fd == -1) {
        printf("File not open!\n");
        return;
    }
    
    sys_open_file *of = &sys_ofile[sys_fd];
    minode *mip = of->of_minode;
    
    if ((of->of_mode & O_WRONLY) == 0 && (of->of_mode & O_RDWR) == 0) {
        printf("File not opened for writing!\n");
        return;
    }
    
    printf("Enter content (Ctrl+D to end):\n");
    char content[MAXLINE * 100] = {0};
    char line[MAXLINE];
    while (fgets(line, MAXLINE, stdin) != NULL) {
        strcat(content, line);
    }
    
    int len = strlen(content);
    int offset = of->of_offset;
    
    while (offset < of->of_offset + len) {
        int blk = alloc_block(mip, offset);
        int off_in_blk = offset % BLOCKSIZ;
        int write_len = BLOCKSIZ - off_in_blk;
        if (write_len > len - (offset - of->of_offset)) write_len = len - (offset - of->of_offset);
        
        char *dst = virtual_disk + DATASTART + blk * BLOCKSIZ + off_in_blk;
        memcpy(dst, content + (offset - of->of_offset), write_len);
        
        offset += write_len;
    }
    
    uint32_t curr_size = mip->dino.di_size;
    if ((uint32_t)offset > curr_size) {
        mip->dino.di_size = offset;
        mip->m_flag = 1;
    }
    
    of->of_offset = offset;
    printf("Write successful!\n");
}

void cmd_delete(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(arg, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    if (access(mip, O_WRONLY) != 0) {
        printf("Permission denied!\n");
        iput(mip);
        return;
    }
    
    if (mip->dino.di_mode & S_IFDIR) {
        printf("Cannot delete directory, use rmdir!\n");
        iput(mip);
        return;
    }
    
    // 隐患修复 #5: 并发删除不一致性 - 检查是否有打开的句柄
    int file_in_use = 0;
    for (int i = 0; i < SYSOPENFILE; i++) {
        if (sys_ofile[i].of_minode->ino == mip->ino && sys_ofile[i].of_count > 0) {
            file_in_use = 1;
            break;
        }
    }
    
    if (file_in_use) {
        printf("Error: Cannot delete file '%s' - currently open by another user or process\n", arg);
        iput(mip);
        return;
    }
    
    mip->dino.di_nlink--;
    mip->m_flag = 1;
    
    int indirect_blocks = (int)(BLOCKSIZ / sizeof(int));
    for (int i = 0; i < NADDR; i++) {
        if (mip->dino.di_addr[i] != 0) {
            if (i < 8) {
                bfree(mip->dino.di_addr[i]);
            } else if (i == 8) {
                int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[i] * BLOCKSIZ);
                for (int j = 0; j < indirect_blocks; j++) {
                    if (indirect[j] != 0) bfree(indirect[j]);
                }
                bfree(mip->dino.di_addr[i]);
            } else if (i == 9) {
                int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[i] * BLOCKSIZ);
                for (int j = 0; j < indirect_blocks; j++) {
                    if (dbl_indirect[j] != 0) {
                        int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[j] * BLOCKSIZ);
                        for (int k = 0; k < indirect_blocks; k++) {
                            if (indirect[k] != 0) bfree(indirect[k]);
                        }
                        bfree(dbl_indirect[j]);
                    }
                }
                bfree(mip->dino.di_addr[i]);
            }
            mip->dino.di_addr[i] = 0;
        }
    }
    
    iput(mip);
    ifree(mip->ino);
    
    minode *dp = iget(current_user->u_cwd->ino);
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == mip->ino) {
            de[i].de_ino = 0;
            dp->dino.di_size -= sizeof(dir_entry);
            dp->m_flag = 1;
            break;
        }
    }
    iput(dp);
    
    printf("File deleted!\n");
}

void cmd_copy(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter source and destination files!\n");
        return;
    }
    
    char src[MAXPATH], dest[MAXPATH];
    if (sscanf(arg, "%s %s", src, dest) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *src_mip = NULL;
    if (namei(src, &src_mip) != 0) {
        printf("Source file not found!\n");
        return;
    }
    
    if ((src_mip->dino.di_mode & S_IFREG) == 0) {
        printf("Source is not a regular file!\n");
        iput(src_mip);
        return;
    }
    
    minode *dest_dp = iget(current_user->u_cwd->ino);
    char dest_name[DIRSIZ];
    char *p = strrchr(dest, '/');
    if (p != NULL) {
        strncpy(dest_name, p + 1, DIRSIZ - 1);
        *p = '\0';
        if (strlen(dest) > 0) {
            minode *tmp = NULL;
            if (namei(dest, &tmp) != 0) {
                printf("Destination directory not found!\n");
                iput(src_mip);
                iput(dest_dp);
                return;
            }
            iput(dest_dp);
            dest_dp = tmp;
        }
    } else {
        strncpy(dest_name, dest, DIRSIZ - 1);
    }
    dest_name[DIRSIZ - 1] = '\0';
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dest_dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, dest_name) == 0) {
            printf("Destination file already exists!\n");
            iput(src_mip);
            iput(dest_dp);
            return;
        }
    }
    
    int ino = ialloc();
    minode *dest_mip = iget(ino);
    dest_mip->dino.di_mode = src_mip->dino.di_mode;
    dest_mip->dino.di_nlink = 1;
    dest_mip->dino.di_uid = current_user->u_uid;
    dest_mip->dino.di_gid = current_user->u_gid;
    dest_mip->dino.di_size = 0;
    memset(dest_mip->dino.di_addr, 0, sizeof(dest_mip->dino.di_addr));
    
    int size = src_mip->dino.di_size;
    int offset = 0;
    while (offset < size) {
        int blk = alloc_block(src_mip, offset);
        char *src_buf = virtual_disk + DATASTART + blk * BLOCKSIZ + (offset % BLOCKSIZ);
        
        int dest_blk = alloc_block(dest_mip, offset);
        char *dest_buf = virtual_disk + DATASTART + dest_blk * BLOCKSIZ + (offset % BLOCKSIZ);
        
        int copy_len = BLOCKSIZ - (offset % BLOCKSIZ);
        if (copy_len > size - offset) copy_len = size - offset;
        memcpy(dest_buf, src_buf, copy_len);
        
        offset += copy_len;
    }
    
    dest_mip->dino.di_size = size;
    dest_mip->m_flag = 1;
    iput(dest_mip);
    
    iname(dest_dp, dest_name, &ino);
    iput(dest_dp);
    iput(src_mip);
    
    printf("File copied successfully!\n");
}

void cmd_link(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter source and destination files!\n");
        return;
    }
    
    char src[MAXPATH], dest[MAXPATH];
    if (sscanf(arg, "%s %s", src, dest) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *src_mip = NULL;
    if (namei(src, &src_mip) != 0) {
        printf("Source file not found!\n");
        return;
    }
    
    minode *dest_dp = iget(current_user->u_cwd->ino);
    char dest_name[DIRSIZ];
    char *p = strrchr(dest, '/');
    if (p != NULL) {
        strncpy(dest_name, p + 1, DIRSIZ - 1);
        *p = '\0';
        if (strlen(dest) > 0) {
            minode *tmp = NULL;
            if (namei(dest, &tmp) != 0) {
                printf("Destination directory not found!\n");
                iput(src_mip);
                iput(dest_dp);
                return;
            }
            iput(dest_dp);
            dest_dp = tmp;
        }
    } else {
        strncpy(dest_name, dest, DIRSIZ - 1);
    }
    dest_name[DIRSIZ - 1] = '\0';
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dest_dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, dest_name) == 0) {
            printf("Destination file already exists!\n");
            iput(src_mip);
            iput(dest_dp);
            return;
        }
    }
    
    src_mip->dino.di_nlink++;
    src_mip->m_flag = 1;
    iput(src_mip);
    
    iname(dest_dp, dest_name, &src_mip->ino);
    iput(dest_dp);
    
    printf("Hard link created successfully!\n");
}

// New commands implementation

void cmd_pwd(char *arg) {
    (void)arg;
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    // Build path by traversing up the directory tree
    char path[MAXPATH] = "";
    minode *current = iget(current_user->u_cwd->ino);
    
    if (current->ino == ROOT_INODE) {
        printf("/\n");
    } else {
        printf("Current directory: (internal inode %d)\n", current->ino);
    }
    
    iput(current);
}

void cmd_touch(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    char filename[DIRSIZ];
    strncpy(filename, arg, DIRSIZ - 1);
    filename[DIRSIZ - 1] = '\0';
    
    minode *dp = iget(current_user->u_cwd->ino);
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, filename) == 0) {
            printf("File already exists!\n");
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
    minode *new_file = iget(ino);
    new_file->dino.di_mode = S_IFREG | S_IREAD | S_IWRITE;
    new_file->dino.di_nlink = 1;
    new_file->dino.di_uid = current_user->u_uid;
    new_file->dino.di_gid = current_user->u_gid;
    new_file->dino.di_size = 0;
    memset(new_file->dino.di_addr, 0, sizeof(new_file->dino.di_addr));
    new_file->m_flag = 1;
    iput(new_file);
    
    iname(dp, filename, &ino);
    iput(dp);
    
    printf("File created successfully!\n");
}

void cmd_cat(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(arg, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    if ((mip->dino.di_mode & S_IFLNK) == S_IFLNK) {
        uint32_t target_size = mip->dino.di_size;
        if (target_size > 0) {
            int blk = mip->dino.di_addr[0];
            if (blk > 0) {
                char *target = (char *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
                printf("%.*s\n", (int)target_size, target);
            }
        }
        iput(mip);
        return;
    }
    
    if ((mip->dino.di_mode & S_IFREG) == 0) {
        printf("Not a regular file!\n");
        iput(mip);
        return;
    }
    
    if (access(mip, O_RDONLY) != 0) {
        printf("Permission denied!\n");
        iput(mip);
        return;
    }
    
    uint32_t file_size = mip->dino.di_size;
    int offset = 0;
    
    while ((uint32_t)offset < file_size) {
        int blk = alloc_block(mip, offset);
        int off_in_blk = offset % BLOCKSIZ;
        int read_len = BLOCKSIZ - off_in_blk;
        if (read_len > (int)(file_size - offset)) read_len = (int)(file_size - offset);
        
        char *src = virtual_disk + DATASTART + blk * BLOCKSIZ + off_in_blk;
        fwrite(src, 1, read_len, stdout);
        
        offset += read_len;
    }
    
    printf("\n");
    iput(mip);
}

void cmd_chmod(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter mode and filename!\n");
        return;
    }
    
    int mode;
    char filename[MAXPATH];
    if (sscanf(arg, "%o %s", &mode, filename) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(filename, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    if (current_user->u_uid != 0 && current_user->u_uid != mip->dino.di_uid) {
        printf("Permission denied! Only owner or root can change permissions.\n");
        iput(mip);
        return;
    }
    
    // Preserve file type bits, only change permission bits
    uint16_t file_type = mip->dino.di_mode & 0xF000;
    mip->dino.di_mode = file_type | (mode & 0x0FFF);
    mip->m_flag = 1;
    iput(mip);
    
    printf("Permissions changed successfully!\n");
}

void cmd_whoami(char *arg) {
    (void)arg;
    if (current_user == NULL) {
        printf("Not logged in!\n");
        return;
    }
    printf("uid=%d(%s) gid=%d\n", current_user->u_uid, current_user->u_name, current_user->u_gid);
}

void cmd_stat(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter filename!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(arg, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    printf("\n=== File Statistics ===\n");
    printf("Inode: %d\n", mip->ino);
    
    char type_char = '-';
    if ((mip->dino.di_mode & S_IFDIR) == S_IFDIR) type_char = 'd';
    else if ((mip->dino.di_mode & S_IFLNK) == S_IFLNK) type_char = 'l';
    
    printf("Mode: %04o (%c%s%s%s)\n", 
           mip->dino.di_mode & 0x0FFF,
           type_char,
           (mip->dino.di_mode & S_IREAD) ? "r" : "-",
           (mip->dino.di_mode & S_IWRITE) ? "w" : "-",
           (mip->dino.di_mode & S_IEXEC) ? "x" : "-");
    printf("Links: %d\n", mip->dino.di_nlink);
    printf("Owner UID: %d\n", mip->dino.di_uid);
    printf("Group GID: %d\n", mip->dino.di_gid);
    printf("Size: %u bytes\n", mip->dino.di_size);
    
    if ((mip->dino.di_mode & S_IFLNK) == S_IFLNK) {
        int blk = mip->dino.di_addr[0];
        if (blk > 0) {
            char *target = (char *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
            printf("Target: %.*s\n", (int)mip->dino.di_size, target);
        }
    }
    printf("========================\n\n");
    
    iput(mip);
}

void cmd_useradd(char *arg) {
    if (current_user == NULL || current_user->u_uid != 0) {
        printf("Only root can add users!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter username!\n");
        return;
    }
    
    if (user_count >= USERNUM) {
        printf("Maximum number of users reached!\n");
        return;
    }
    
    char username[DIRSIZ];
    strncpy(username, arg, DIRSIZ - 1);
    username[DIRSIZ - 1] = '\0';
    
    // Check if user already exists
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].u_name, username) == 0) {
            printf("User already exists!\n");
            return;
        }
    }
    
    char passwd[PWDSIZ];
    printf("Password: ");
    scanf("%s", passwd);
    
    strcpy(users[user_count].u_name, username);
    strcpy(users[user_count].u_passwd, passwd);
    users[user_count].u_uid = user_count;
    users[user_count].u_gid = user_count;
    memset(users[user_count].u_ofile, -1, sizeof(users[user_count].u_ofile));
    users[user_count].u_cwd = iget(ROOT_INODE);
    
    user_count++;
    printf("User added successfully!\n");
}

void cmd_userdel(char *arg) {
    if (current_user == NULL || current_user->u_uid != 0) {
        printf("Only root can delete users!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter username!\n");
        return;
    }
    
    char username[DIRSIZ];
    strncpy(username, arg, DIRSIZ - 1);
    username[DIRSIZ - 1] = '\0';
    
    for (int i = 1; i < user_count; i++) {  // Skip root (i=0)
        if (strcmp(users[i].u_name, username) == 0) {
            if (current_user->u_uid == i) {
                printf("Cannot delete current user!\n");
                return;
            }
            
            // Remove user from array
            for (int j = i; j < user_count - 1; j++) {
                users[j] = users[j + 1];
            }
            user_count--;
            printf("User deleted successfully!\n");
            return;
        }
    }
    
    printf("User not found!\n");
}

void cmd_rename(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter source and destination files!\n");
        return;
    }
    
    char oldname[MAXPATH], newname[DIRSIZ];
    if (sscanf(arg, "%s %s", oldname, newname) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(oldname, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    minode *dp = iget(current_user->u_cwd->ino);
    
    // Check if new name already exists
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, newname) == 0) {
            printf("Destination file already exists!\n");
            iput(mip);
            iput(dp);
            return;
        }
    }
    
    // Find and update the directory entry
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == mip->ino) {
            strncpy(de[i].de_name, newname, DIRSIZ - 1);
            de[i].de_name[DIRSIZ - 1] = '\0';
            dp->m_flag = 1;
            break;
        }
    }
    
    iput(mip);
    iput(dp);
    
    printf("File renamed successfully!\n");
}

void cmd_passwd(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    char target_user[DIRSIZ];
    char old_passwd[PWDSIZ];
    char new_passwd[PWDSIZ];
    char confirm_passwd[PWDSIZ];
    int target_uid = -1;
    
    // If argument provided, only root can change another user's password
    if (arg != NULL && *arg != '\0') {
        if (current_user->u_uid != 0) {
            printf("Permission denied! Only root can change other users' passwords.\n");
            return;
        }
        
        strncpy(target_user, arg, DIRSIZ - 1);
        target_user[DIRSIZ - 1] = '\0';
        
        // Find the target user
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].u_name, target_user) == 0) {
                target_uid = i;
                break;
            }
        }
        
        if (target_uid == -1) {
            printf("User not found!\n");
            return;
        }
    } else {
        // No argument: change current user's password
        target_uid = current_user->u_uid;
        
        // Non-root users must enter old password
        printf("Old password: ");
        scanf("%s", old_passwd);
        
        if (strcmp(old_passwd, current_user->u_passwd) != 0) {
            printf("Invalid password!\n");
            return;
        }
    }
    
    // Get new password
    printf("New password: ");
    scanf("%s", new_passwd);
    
    if (strlen(new_passwd) == 0) {
        printf("Password cannot be empty!\n");
        return;
    }
    
    if (strlen(new_passwd) >= PWDSIZ) {
        printf("Password too long!\n");
        return;
    }
    
    // Confirm new password
    printf("Confirm new password: ");
    scanf("%s", confirm_passwd);
    
    if (strcmp(new_passwd, confirm_passwd) != 0) {
        printf("Passwords do not match!\n");
        return;
    }
    
    // Update password
    strcpy(users[target_uid].u_passwd, new_passwd);
    printf("Password changed successfully!\n");
}

void cmd_chown(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (current_user->u_uid != 0) {
        printf("Permission denied! Only root can change file ownership.\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter uid and filename!\n");
        return;
    }
    
    int uid;
    char filename[MAXPATH];
    if (sscanf(arg, "%d %s", &uid, filename) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *mip = NULL;
    if (namei(filename, &mip) != 0) {
        printf("File not found!\n");
        return;
    }
    
    // Change ownership
    mip->dino.di_uid = (uint8_t)uid;
    mip->m_flag = 1;
    iput(mip);
    
    printf("Ownership changed successfully!\n");
}

void cmd_symlink(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    
    if (arg == NULL || *arg == '\0') {
        printf("Please enter target and link name!\n");
        return;
    }
    
    char target[MAXPATH], link[MAXPATH];
    if (sscanf(arg, "%s %s", target, link) != 2) {
        printf("Invalid argument format!\n");
        return;
    }
    
    minode *link_dp = iget(current_user->u_cwd->ino);
    char link_name[DIRSIZ];
    char *p = strrchr(link, '/');
    if (p != NULL) {
        strncpy(link_name, p + 1, DIRSIZ - 1);
        *p = '\0';
        if (strlen(link) > 0) {
            minode *tmp = NULL;
            if (namei(link, &tmp) != 0) {
                printf("Destination directory not found!\n");
                iput(link_dp);
                return;
            }
            iput(link_dp);
            link_dp = tmp;
        }
    } else {
        strncpy(link_name, link, DIRSIZ - 1);
    }
    link_name[DIRSIZ - 1] = '\0';
    
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + link_dp->dino.di_addr[0] * BLOCKSIZ);
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) break;
        if (strcmp(de[i].de_name, link_name) == 0) {
            printf("Link already exists!\n");
            iput(link_dp);
            return;
        }
    }
    
    int link_ino = ialloc();
    if (link_ino == 0) {
        printf("Cannot allocate inode for symlink!\n");
        iput(link_dp);
        return;
    }
    
    minode *link_mip = iget(link_ino);
    link_mip->dino.di_mode = S_IFLNK | S_IREAD | S_IWRITE;
    link_mip->dino.di_uid = current_user->u_uid;
    link_mip->dino.di_gid = current_user->u_gid;
    link_mip->dino.di_nlink = 1;
    link_mip->dino.di_size = strlen(target);
    memset(link_mip->dino.di_addr, 0, sizeof(link_mip->dino.di_addr));
    
    int blk = balloc();
    if (blk == 0) {
        printf("Cannot allocate block for symlink target!\n");
        ifree(link_ino);
        iput(link_mip);
        iput(link_dp);
        return;
    }
    
    char *block_data = (char *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
    strcpy(block_data, target);
    
    link_mip->dino.di_addr[0] = blk;
    link_mip->m_flag = 1;
    iput(link_mip);
    
    iname(link_dp, link_name, &link_ino);
    iput(link_dp);
    
    printf("Symbolic link created successfully!\n");
}
