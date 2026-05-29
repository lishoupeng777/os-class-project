#include "vfs.h"

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
    
    printf("Enter content (Ctrl+Z then Enter to end):\n");
    char content[MAXLINE * 100] = {0};
    char line[MAXLINE];
    while (fgets(line, MAXLINE, stdin) != NULL) {
        strcat(content, line);
    }
    
    int start_offset = of->of_offset;
    int len = strlen(content);
    int written = 0;
    
    while (written < len) {
        int file_offset = start_offset + written;
        int blk = alloc_block(mip, file_offset);
        int off_in_blk = file_offset % BLOCKSIZ;
        int write_len = BLOCKSIZ - off_in_blk;
        if (write_len > len - written) write_len = len - written;
        
        char *dst = virtual_disk + DATASTART + blk * BLOCKSIZ + off_in_blk;
        memcpy(dst, content + written, write_len);
        
        written += write_len;
    }
    
    // 从文件头写入时，文件大小 = 新内容长度（截断旧内容）
    // 追加写入时，只增大文件大小
    if (start_offset == 0) {
        mip->dino.di_size = len;
    } else {
        uint32_t new_end = (uint32_t)(start_offset + len);
        if (new_end > mip->dino.di_size) {
            mip->dino.di_size = new_end;
        }
    }
    mip->m_flag = 1;
    
    // 写入完成后重置文件指针到开头，方便后续读取
    of->of_offset = 0;
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
        if (sys_ofile[i].of_minode != NULL &&
            sys_ofile[i].of_minode->ino == mip->ino &&
            sys_ofile[i].of_count > 0) {
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
    
    // 用局部变量保存 ino，然后调用 iput 安全释放 mip
    int deleted_ino = mip->ino;
    iput(mip);
    ifree(deleted_ino);
    
    // 删除目录项（压缩填充空洞，避免 namei/dir 扫描中断）
    minode *dp = iget(current_user->u_cwd->ino);
    dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + dp->dino.di_addr[0] * BLOCKSIZ);
    int found_idx = -1;
    int last_idx = -1;
    for (int i = 0; i < DIRNUM; i++) {
        if (de[i].de_ino == 0) { last_idx = i; break; }
        last_idx = i + 1;
        if (de[i].de_ino == deleted_ino) found_idx = i;
    }
    // 找到后，将后面的条目全部前移一位
    if (found_idx >= 0) {
        for (int i = found_idx; i < last_idx - 1; i++) {
            de[i] = de[i + 1];
        }
        de[last_idx - 1].de_ino = 0;
        de[last_idx - 1].de_name[0] = '\0';
        dp->dino.di_size -= sizeof(dir_entry);
        dp->m_flag = 1;
    }
    iput(dp);
    
    printf("File deleted!\n");
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
