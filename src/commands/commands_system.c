#include "vfs.h"

void cmd_format(char *arg) {
    (void)arg;
    format_disk();
}

void cmd_sbinfo(char *arg) {
    if (sb == NULL) {
        printf("Super block not initialized!\n");
        return;
    }

    printf("========================================\n");
    printf("      Virtual Disk Layout Info\n");
    printf("========================================\n");
    printf("Block size          : %d bytes\n", BLOCKSIZ);
    printf("Disk total size     : %d MB (%d bytes)\n", DISK_SIZE / (1024*1024), DISK_SIZE);

    printf("\n--- Block Allocation ---\n");
    printf("  Block 0            : Boot block (reserved)\n");
    printf("  Block 1            : Super block\n");
    printf("  Blocks 2~%d       : Inode table (%d blocks, %d inodes)\n",
           1 + DINODEBLK, DINODEBLK, DINODEBLK * BLOCKSIZ / DINODESIZ);
    printf("  Blocks %d~%d     : Data blocks (%d blocks)\n",
           1 + DINODEBLK + 1, 1 + DINODEBLK + FILEBLK, FILEBLK);

    printf("\n========================================\n");
    printf("      Super Block Info\n");
    printf("========================================\n");
    printf("  isize (Inode tbl blks) : %d\n", sb->isize);
    printf("  fsize (Data blks)      : %d\n", sb->fsize);
    printf("  s_modified             : %d\n", sb->s_modified);

    printf("\n--- Free Inodes ---\n");
    printf("  ifree_num (free)       : %d\n", sb->ifree_num);
    printf("  ifree_ptr              : %d\n", sb->ifree_ptr);
    printf("  Stack content (top -> bottom):\n");
    if (sb->ifree_ptr < 0) {
        printf("    <empty>\n");
    } else {
        for (int i = sb->ifree_ptr; i >= 0; i--) {
            printf("    [%2d] %d\n", i, sb->ifree[i]);
        }
    }

    printf("\n--- Free Data Blocks ---\n");
    printf("  ffree_num (in stack)   : %d\n", sb->ffree_num);
    printf("  ffree_ptr              : %d\n", sb->ffree_ptr);
    printf("  Stack content (top -> bottom):\n");
    if (sb->ffree_ptr < 0) {
        printf("    <empty>\n");
    } else {
        for (int i = sb->ffree_ptr; i >= 0; i--) {
            printf("    [%2d] %d\n", i, sb->ffree[i]);
        }
    }

    // 估算成组链接中的空闲块总数
    // 当前栈中已有 ffree_num 个，分组链表中每块存 50 个
    int total_avail_approx = sb->ffree_num;
    // 尝试追踪分组链表来汇总全部空闲块
    {
        int *dp = (int *)(virtual_disk + DATASTART);
        int group_blocks = 0;
        int chain_len = 0;
        for (int i = 0; i < FILEBLK; i++) {
            if (dp[i * (BLOCKSIZ / sizeof(int)) + NICFREE] == -1) {
                int cnt = 0;
                for (int j = 0; j < NICFREE; j++) {
                    if (dp[i * (BLOCKSIZ / sizeof(int)) + j] != -1) cnt++;
                    else break;
                }
                group_blocks += cnt;
                chain_len++;
            }
        }
        printf("\n  Grouped-link chains    : %d block(s)\n", chain_len);
        printf("  Blocks in chains       : ~%d\n", group_blocks);
        printf("  Estimated total free   : ~%d / %d\n",
               total_avail_approx + group_blocks, FILEBLK);
        printf("  Used blocks            : ~%d\n",
               FILEBLK - (total_avail_approx + group_blocks));
    }

    if (arg == NULL || *arg == '\0') {
        printf("\nTip: sbinfo <blkno> can dump a data block as int list to inspect grouped-link payload.\n");
        return;
    }

    int blkno = -1;
    if (sscanf(arg, "%d", &blkno) != 1) {
        printf("Invalid argument. Usage: sbinfo [blkno]\n");
        return;
    }
    if (blkno < 0 || blkno >= FILEBLK) {
        printf("Invalid blkno: %d (expected 0..%d)\n", blkno, FILEBLK - 1);
        return;
    }

    int *p = (int *)(virtual_disk + DATASTART + blkno * BLOCKSIZ);
    int max_ints = BLOCKSIZ / (int)sizeof(int);
    int show = NICFREE + 1;
    if (show > max_ints) {
        show = max_ints;
    }

    printf("\nBlock %d as int payload:\n", blkno);
    for (int i = 0; i < show; i++) {
        printf("  p[%2d] = %d\n", i, p[i]);
        if (p[i] == -1) {
            break;
        }
    }
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
    getchar();
    printf("Password: ");
    scanf("%s", passwd);
    getchar();
    
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
