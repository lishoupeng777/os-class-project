#include "vfs.h"

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
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].u_name, username) == 0) {
            printf("User already exists!\n");
            return;
        }
    }
    
    char passwd[PWDSIZ];
    printf("Password: ");
    scanf("%s", passwd);
    getchar();
    
    strcpy(users[user_count].u_name, username);
    strcpy(users[user_count].u_passwd, passwd);
    users[user_count].u_uid = user_count;
    users[user_count].u_gid = user_count;
    memset(users[user_count].u_ofile, -1, sizeof(users[user_count].u_ofile));
    
    // 创建用户的家目录 /home/username
    minode *root = iget(ROOT_INODE);
    minode *home_dp = NULL;
    int home_ino = -1;
    // 查找 /home 目录的 inode
    if (namei("home", &home_dp) == 0) {
        home_ino = home_dp->ino;
        iput(home_dp);
    } else {
        // /home 不存在则创建
        home_ino = create_subdir(ROOT_INODE, "home", 0, 0);
    }
    iput(root);
    
    int user_home_ino = create_subdir(home_ino, username, user_count, user_count);
    if (user_home_ino > 0) {
        users[user_count].u_cwd = iget(user_home_ino);
        printf("  Home directory /home/%s created\n", username);
    } else {
        users[user_count].u_cwd = iget(ROOT_INODE);
    }
    
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
    
    for (int i = 1; i < user_count; i++) {
        if (strcmp(users[i].u_name, username) == 0) {
            if (current_user->u_uid == i) {
                printf("Cannot delete current user!\n");
                return;
            }
            
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
    
    if (arg != NULL && *arg != '\0') {
        if (current_user->u_uid != 0) {
            printf("Permission denied! Only root can change other users' passwords.\n");
            return;
        }
        
        strncpy(target_user, arg, DIRSIZ - 1);
        target_user[DIRSIZ - 1] = '\0';
        
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
        target_uid = current_user->u_uid;
        
        printf("Old password: ");
        scanf("%s", old_passwd);
        getchar();
        
        if (strcmp(old_passwd, current_user->u_passwd) != 0) {
            printf("Invalid password!\n");
            return;
        }
    }
    
    printf("New password: ");
    scanf("%s", new_passwd);
    getchar();
    
    if (strlen(new_passwd) == 0) {
        printf("Password cannot be empty!\n");
        return;
    }
    
    if (strlen(new_passwd) >= PWDSIZ) {
        printf("Password too long!\n");
        return;
    }
    
    printf("Confirm new password: ");
    scanf("%s", confirm_passwd);
    getchar();
    
    if (strcmp(new_passwd, confirm_passwd) != 0) {
        printf("Passwords do not match!\n");
        return;
    }
    
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
    
    mip->dino.di_uid = (uint8_t)uid;
    mip->m_flag = 1;
    iput(mip);
    
    printf("Ownership changed successfully!\n");
}
