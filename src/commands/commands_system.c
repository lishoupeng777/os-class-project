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

    printf("=== Super Block Info ===\n");
    printf("isize=%d, fsize=%d\n", sb->isize, sb->fsize);
    printf("ifree_num=%d, ifree_ptr=%d\n", sb->ifree_num, sb->ifree_ptr);
    printf("ffree_num=%d, ffree_ptr=%d\n", sb->ffree_num, sb->ffree_ptr);
    printf("s_modified=%d\n", sb->s_modified);

    printf("\nCurrent free-block stack (top -> bottom):\n");
    if (sb->ffree_ptr < 0) {
        printf("  <empty>\n");
    } else {
        for (int i = sb->ffree_ptr; i >= 0; i--) {
            printf("  [%2d] %d\n", i, sb->ffree[i]);
        }
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
