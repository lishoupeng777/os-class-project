# Simulated UNIX Filesystem VFS 使用手册

## 1. 项目简介

本项目是一个教学用途的模拟 UNIX 文件系统。它使用 C 语言实现，提供交互式命令行界面，支持用户登录、目录管理、文件读写、链接、权限控制、导入宿主机文件以及虚拟磁盘持久化。

## 2. 运行环境

- Windows 11
- MinGW GCC
- Visual Studio Code

## 3. 编译方法

在项目根目录下执行：

```powershell
cd "d:\trae project\os"
mkdir -Force bin
gcc -Wall -Wextra -g -Iinclude src\core\main.c src\core\vfs_globals.c src\core\vfs_disk.c src\core\vfs_block.c src\core\vfs_inode.c src\core\vfs_path.c src\core\vfs_perm.c src\commands\commands_system.c src\commands\commands_dir.c src\commands\commands_file_io.c src\commands\commands_file_ops.c src\commands\commands_user.c -o bin\vfs.exe
```

## 4. 启动方法

```powershell
cd "d:\trae project\os"
.\bin\vfs.exe
```

程序启动后会进入交互式提示符，登录后一般显示为 `用户名@vfs:当前路径$`。

## 5. 核心命令

### 用户管理

- `login`：登录系统
- `logout`：退出当前用户
- `whoami`：查看当前登录用户
- `users`：查看系统所有用户
- `useradd <name>`：添加用户（仅 root）
- `userdel <name>`：删除用户（仅 root）
- `passwd [user]`：修改密码

### 目录与文件

- `mkdir <dir>`：创建目录
- `chdir <dir>`：切换目录
- `pwd`：显示当前工作目录
- `dir`：列出目录内容
- `touch <file>`：创建空文件
- `create <file>`：创建文件并可选择写入内容
- `cat <file>`：输出文件内容
- `delete <file>`：删除文件
- `rename <old> <new>`：重命名文件或目录

### 文件 I/O

- `open <file>`：打开文件
- `close <fd>`：关闭文件描述符
- `read <fd> <size>`：读取文件内容
- `write <fd>`：向文件写入内容

### 权限与链接

- `chmod <mode> <file>`：修改权限，例如 `chmod 755 a.txt`
- `chown <uid> <file>`：修改所有者（仅 root）
- `link <src> <dest>`：创建硬链接
- `symlink <target> <link>`：创建符号链接
- `stat <file>`：查看文件元信息

### 系统维护与调试

- `format`：格式化虚拟磁盘
- `sbinfo [blkno]`：查看超级块空闲栈；可选块号用于按整数数组查看指定块内容
- `help`：显示帮助
- `quit`：退出并保存到 `virtual_disk.bin`

## 6. 推荐演示流程

1. `login` 登录 root
2. `users` 查看用户
3. `mkdir demo`、`chdir demo`、`pwd` 验证目录功能
4. `touch a.txt`、`open a.txt`、`write 0`、`read 0 100` 验证文件读写
5. `stat a.txt` 查看 inode、权限和大小
6. `link a.txt hard.txt`、`symlink a.txt soft.txt` 演示链接功能
7. `import D:/test.txt big.txt` 演示宿主文件导入
8. `sbinfo` 查看超级块空闲栈状态
9. `quit` 保存并退出，再次启动后确认数据仍然存在

## 7. 空间与结构说明

- 虚拟磁盘容器大小为 4MB
- 当前文件系统用于文件内容的数据块总数为 512 块，每块 512 字节
- 文件实际可用数据区约为 256KB
- inode 采用 8 个直接块 + 1 个一级间接块 + 1 个二级间接块的混合索引方式
- 空闲块采用超级块中的成组链接法管理

## 8. 注意事项

- `delete` 只删除普通文件，目录删除需使用对应目录删除逻辑
- `copy` 对大文件的间接块支持有限，验收时建议优先用小文件或 `import` 演示大文件
- `quit` 前请确认内容已保存，程序会自动写回虚拟磁盘
