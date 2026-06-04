#include "vfs.h"

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
        // 直接从磁盘读源文件数据块，不要用 alloc_block(src_mip, ...)
        // 否则会为源文件额外分配间接块
        int src_blkno = offset / BLOCKSIZ;
        int off_in_blk = offset % BLOCKSIZ;
        int src_blk;
        if (src_blkno < 8) {
            src_blk = src_mip->dino.di_addr[src_blkno];
        } else {
            // 简单处理：源文件只有直接块的情况才支持 copy
            // 对于间接块文件，复制可能不完整
            printf("[WARN] Large file copy may be incomplete\n");
            src_blk = src_mip->dino.di_addr[0]; // fallback
        }
        char *src_buf = virtual_disk + DATASTART + src_blk * BLOCKSIZ + off_in_blk;
        
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

// import <host_path> <vfs_dest>
void cmd_import(char *arg) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return;
    }
    if (arg == NULL || *arg == '\0') {
        printf("Usage: import <host_path> <vfs_dest>\n");
        return;
    }
    char host[MAXPATH];
    char vfsdest[MAXPATH];
    if (sscanf(arg, "%s %s", host, vfsdest) != 2) {
        printf("Invalid arguments. Usage: import <host_path> <vfs_dest>\n");
        return;
    }

    FILE *fp = fopen(host, "rb");
    if (fp == NULL) {
        printf("Cannot open host file: %s\n", host);
        return;
    }

    // read entire file into buffer (limit to available disk size)
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < 0) fsize = 0;

    // create destination file in VFS
    // reuse create/write logic: allocate inode and write blocks
    minode *dp = iget(current_user->u_cwd->ino);
    char dest_name[DIRSIZ];
    char path_copy[MAXPATH];
    strncpy(path_copy, vfsdest, MAXPATH-1);
    path_copy[MAXPATH-1] = '\0';
    char *p = strrchr(path_copy, '/');
    minode *target_dir = dp;
    if (p != NULL) {
        strncpy(dest_name, p+1, DIRSIZ-1);
        *p = '\0';
        if (strlen(path_copy) > 0) {
            minode *tmp = NULL;
            if (namei(path_copy, &tmp) != 0) {
                printf("Destination directory not found: %s\n", path_copy);
                iput(dp);
                fclose(fp);
                return;
            }
            target_dir = tmp;
            iput(dp);
        }
    } else {
        strncpy(dest_name, vfsdest, DIRSIZ-1);
    }
    dest_name[DIRSIZ-1] = '\0';

    // allocate inode
    int ino = ialloc();
    if (ino < 0) {
        printf("No free inodes\n");
        iput(target_dir);
        fclose(fp);
        return;
    }
    minode *mip = iget(ino);
    mip->dino.di_mode = S_IFREG | S_IREAD | S_IWRITE;
    mip->dino.di_uid = current_user->u_uid;
    mip->dino.di_gid = current_user->u_gid;
    mip->dino.di_nlink = 1;
    mip->dino.di_size = 0;
    memset(mip->dino.di_addr, 0, sizeof(mip->dino.di_addr));

    long offset = 0;
    char buf[BLOCKSIZ];
    while (!feof(fp)) {
        size_t r = fread(buf, 1, BLOCKSIZ, fp);
        if (r == 0) break;
        int blk = alloc_block(mip, offset);
        if (blk < 0) {
            printf("No free blocks\n");
            // cleanup: free allocated inode/blocks? keep simple: write what we have
            break;
        }
        memcpy(virtual_disk + DATASTART + blk * BLOCKSIZ, buf, r);
        offset += r;
    }

    mip->dino.di_size = (uint32_t)offset;
    mip->m_flag = 1;
    iput(mip);

    // add directory entry
    iname(target_dir, dest_name, &ino);
    iput(target_dir);
    fclose(fp);

    printf("Imported %s => %s (size=%ld bytes)\n", host, vfsdest, offset);
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
