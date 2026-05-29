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
