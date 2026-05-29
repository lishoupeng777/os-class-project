#include "vfs.h"

void print_help() {
    printf("\n=== Simulated UNIX Filesystem Command List ===\n");
    printf("format          - Format virtual disk\n");
    printf("login           - User login\n");
    printf("logout          - User logout\n");
    printf("whoami          - Display current user\n");
    printf("useradd <name>  - Add new user\n");
    printf("userdel <name>  - Delete user\n");
    printf("passwd [user]   - Change password (no arg: own; arg: root changes user)\n");
    printf("mkdir <dir>     - Create directory\n");
    printf("chdir <dir>     - Change directory\n");
    printf("pwd             - Print working directory\n");
    printf("dir             - List directory contents\n");
    printf("touch <file>    - Create empty file\n");
    printf("create <file>   - Create file (with write)\n");
    printf("cat <file>      - Display file content\n");
    printf("open <file>     - Open file\n");
    printf("close <fd>      - Close file\n");
    printf("read <fd> <size> - Read file content\n");
    printf("write <fd>      - Write to file\n");
    printf("delete <file>   - Delete file\n");
    printf("rename <old> <new> - Rename file\n");
    printf("chmod <mode> <file> - Change file permissions\n");
    printf("chown <uid> <file> - Change file owner (root only)\n");
    printf("copy <src> <dest> - Copy file\n");
    printf("link <src> <dest> - Create hard link\n");
    printf("symlink <target> <link> - Create symbolic link\n");
    printf("stat <file>     - Show file statistics\n");
    printf("help            - Show this help\n");
    printf("quit            - Exit and save\n");
    printf("=============================================\n");
}

int main() {
    char cmd_line[MAXLINE];
    char cmd[32], arg[MAXLINE];

    printf("[1] Welcome to Simulated Filesystem VFS\n");

    init_structures();
    if (!load_from_disk()) {
        printf("Virtual disk file not found, creating new filesystem...\n");
        format_disk();
    }

    printf("\nType 'help' for available commands\n");

    while (1) {
        // 显示动态提示符：用户名@vfs:路径$
        if (current_user != NULL) {
            char cwd_path[MAXPATH];
            get_cwd_path(cwd_path, sizeof(cwd_path));
            printf("\n%s@vfs:%s$ ", current_user->u_name, cwd_path);
        } else {
            printf("\nVFS> ");
        }
        if (fgets(cmd_line, MAXLINE, stdin) == NULL) break;
        
        cmd_line[strcspn(cmd_line, "\n")] = '\0';
        
        memset(cmd, 0, sizeof(cmd));
        memset(arg, 0, sizeof(arg));
        
        if (sscanf(cmd_line, "%s %[^\n]", cmd, arg) == 2) {
            arg[strcspn(arg, "\n")] = '\0';
        } else {
            sscanf(cmd_line, "%s", cmd);
        }
        
        if (cmd[0] == '\0') continue;

        if (strcmp(cmd, "format") == 0) {
            cmd_format(arg);
        } else if (strcmp(cmd, "login") == 0) {
            cmd_login(arg);
        } else if (strcmp(cmd, "logout") == 0) {
            cmd_logout(arg);
        } else if (strcmp(cmd, "whoami") == 0) {
            cmd_whoami(arg);
        } else if (strcmp(cmd, "useradd") == 0) {
            cmd_useradd(arg);
        } else if (strcmp(cmd, "userdel") == 0) {
            cmd_userdel(arg);
        } else if (strcmp(cmd, "passwd") == 0) {
            cmd_passwd(arg);
        } else if (strcmp(cmd, "mkdir") == 0) {
            cmd_mkdir(arg);
        } else if (strcmp(cmd, "chdir") == 0) {
            cmd_chdir(arg);
        } else if (strcmp(cmd, "pwd") == 0) {
            cmd_pwd(arg);
        } else if (strcmp(cmd, "dir") == 0) {
            cmd_dir(arg);
        } else if (strcmp(cmd, "touch") == 0) {
            cmd_touch(arg);
        } else if (strcmp(cmd, "create") == 0) {
            cmd_create(arg);
        } else if (strcmp(cmd, "cat") == 0) {
            cmd_cat(arg);
        } else if (strcmp(cmd, "open") == 0) {
            cmd_open(arg);
        } else if (strcmp(cmd, "close") == 0) {
            cmd_close(arg);
        } else if (strcmp(cmd, "read") == 0) {
            cmd_read(arg);
        } else if (strcmp(cmd, "write") == 0) {
            cmd_write(arg);
        } else if (strcmp(cmd, "delete") == 0) {
            cmd_delete(arg);
        } else if (strcmp(cmd, "rename") == 0) {
            cmd_rename(arg);
        } else if (strcmp(cmd, "chmod") == 0) {
            cmd_chmod(arg);
        } else if (strcmp(cmd, "chown") == 0) {
            cmd_chown(arg);
        } else if (strcmp(cmd, "copy") == 0) {
            cmd_copy(arg);
        } else if (strcmp(cmd, "link") == 0) {
            cmd_link(arg);
        } else if (strcmp(cmd, "symlink") == 0) {
            cmd_symlink(arg);
        } else if (strcmp(cmd, "stat") == 0) {
            cmd_stat(arg);
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "quit") == 0) {
            save_to_disk();
            printf("Thank you for using Simulated Filesystem, goodbye!\n");
            break;
        } else {
            printf("Unknown command, type 'help' for available commands\n");
        }
    }

    if (virtual_disk != NULL) {
        free(virtual_disk);
    }

    return 0;
}