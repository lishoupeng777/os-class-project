#include "vfs.h"

// ============================================================
// 路径解析与目录操作
// 负责：namei() / iname() / get_cwd_path()
// ============================================================

// ----------------------------------------------------------
// 路径名逐分量解析
// ----------------------------------------------------------
int namei(char *path, minode **mip) {
    if (current_user == NULL) {
        printf("Please login first!\n");
        return -1;
    }

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

// ----------------------------------------------------------
// 在目录中插入目录项
// ----------------------------------------------------------
int iname(minode *dp, char *name, int *ino) {
    if ((dp->dino.di_mode & S_IFDIR) == 0) return -1;

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

// ----------------------------------------------------------
// 获取当前工作目录的绝对路径字符串
// 从 cwd 开始向上查找父目录，直到根目录
// 参数: buf - 输出缓冲区, size - 缓冲区大小
// 返回: buf 指针
// ----------------------------------------------------------
char *get_cwd_path(char *buf, int size) {
    if (current_user == NULL || current_user->u_cwd == NULL) {
        buf[0] = '\0';
        return buf;
    }

    int cwd_ino = current_user->u_cwd->ino;
    if (cwd_ino == ROOT_INODE) {
        buf[0] = '/'; buf[1] = '\0';
        return buf;
    }

    // 用 dinode 磁盘数据直接遍历（不经过 iget/iput 避免引用计数问题）
    char path_stack[MAXPATH / DIRSIZ][DIRSIZ];
    int depth = 0;
    int current_ino = cwd_ino;
    int guard = 0;

    while (current_ino != ROOT_INODE && guard < 256) {
        guard++;

        // 直接从磁盘读取当前目录的 dinode
        dinode *dip = (dinode *)(virtual_disk + DINODESTART + (current_ino - 1) * DINODESIZ);
        int blk = dip->di_addr[0];
        if (blk <= 0) break;

        dir_entry *de = (dir_entry *)(virtual_disk + DATASTART + blk * BLOCKSIZ);
        int parent_ino = de[1].de_ino;  // ".." 指向父目录

        if (parent_ino <= 0 || parent_ino == current_ino) break;

        // 在父目录中查找当前 inode 对应的文件名
        dinode *pdip = (dinode *)(virtual_disk + DINODESTART + (parent_ino - 1) * DINODESIZ);
        int pblk = pdip->di_addr[0];
        if (pblk <= 0) break;

        dir_entry *pde = (dir_entry *)(virtual_disk + DATASTART + pblk * BLOCKSIZ);
        int found = 0;
        for (int i = 0; i < DIRNUM && pde[i].de_ino != 0; i++) {
            if (pde[i].de_ino == current_ino) {
                strncpy(path_stack[depth], pde[i].de_name, DIRSIZ - 1);
                path_stack[depth][DIRSIZ - 1] = '\0';
                depth++;
                found = 1;
                break;
            }
        }

        if (!found) break;
        current_ino = parent_ino;
    }

    // 构建路径字符串
    buf[0] = '/';
    int pos = 1;
    for (int i = depth - 1; i >= 0; i--) {
        int len = strlen(path_stack[i]);
        if (pos + len + 1 >= size) break;
        memcpy(buf + pos, path_stack[i], len);
        pos += len;
        if (i > 0) buf[pos++] = '/';
    }
    buf[pos] = '\0';

    return buf;
}