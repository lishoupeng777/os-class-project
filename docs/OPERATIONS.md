操作手册 — Simulated Filesystem VFS

概述
- 这是一个基于教学的模拟文件系统（VFS），交互式命令行程序。运行后输入命令执行对应操作。

快速编译和运行
- 在 Windows（MinGW）上，用 gcc 编译：

```powershell
cd "d:\trae project\os"
mkdir -Force bin
gcc -Wall -Wextra -g -Iinclude src\core\main.c src\core\vfs_globals.c src\core\vfs_disk.c src\core\vfs_block.c src\core\vfs_inode.c src\core\vfs_path.c src\core\vfs_perm.c src\commands\commands_system.c src\commands\commands_dir.c src\commands\commands_file_io.c src\commands\commands_file_ops.c src\commands\commands_user.c -o bin\vfs.exe
```

- 运行：

```powershell
cd "d:\trae project\os"
.\bin\vfs.exe
```

主命令说明（在程序内输入）
- login           : 登录（按提示输入用户名、密码）
- logout          : 登出当前用户
- whoami          : 显示当前用户信息
- users           : 列出系统所有用户（UID GID 用户名）
- useradd <name>  : 添加新用户（仅 root 可用）
- userdel <name>  : 删除用户（仅 root 可用）
- passwd [user]   : 修改密码（无参数修改自己；root 可指定用户）

目录与文件操作
- mkdir <dir>     : 创建目录
- chdir <dir>     : 切换目录（支持 ..）
- pwd             : 显示当前工作目录路径
- dir             : 列出当前目录内容
- touch <file>    : 创建空文件
- create <file>   : 创建文件并可写入内容
- cat <file>      : 显示文件内容
- delete <file>   : 删除文件
- rename <old> <new> : 重命名文件

文件 I/O
- open <file>     : 打开文件，返回 fd
- close <fd>      : 关闭 fd
- read <fd> <size>: 从 fd 读取数据
- write <fd>      : 向 fd 写入数据（交互式）

权限与链接
- chmod <mode> <file> : 改变文件权限（八进制，如 755）
- chown <uid> <file>   : 改变文件所有者（仅 root）
- link <src> <dest>    : 创建硬链接
- symlink <target> <link> : 创建符号链接
- stat <file>          : 显示文件元信息

系统维护与调试
- format               : 格式化虚拟磁盘（会初始化 root 和 /home）
- sbinfo [blkno]       : 打印超级块空闲栈信息；可选参数 `blkno` 用于以整型清单展示该数据块的内容，用于检查成组链接法
- help                 : 查看帮助（命令列表）
- quit                 : 退出并保存到磁盘

检验“成组链接法”（快速步骤）
1. 运行程序并执行 `sbinfo` 查看当前 `ffree_ptr` 和 `ffree` 列表。
2. 使用一系列分配（例如创建大量文件或写入）使空闲栈耗尽超过一组（>50 块），再次运行 `sbinfo`，观察栈重载（新 `ffree_ptr` 值变化）或使用 `sbinfo <blkno>` 查看分组块的整型链表（查找 -1 结束标志）。

示例交互
```text
VFS> login
Username: root
Password: root
Login successful!
root@vfs:/$ users
UID   GID   USERNAME
0     0     root
1     1     alice
Total users: 2
VFS> sbinfo
=== Super Block Info ===
ffree_num=45, ffree_ptr=44
Current free-block stack (top -> bottom):
  [44] 44
  ...
  [ 0] 0
VFS>
```

注意事项
- 我已清理掉示例代码仓库中的文档副本和编译产物（如 `bin` 中的可执行与虚拟磁盘文件），代码文件保留在 `src/` 与 `include/`。
- 请在运行前确保 `bin` 目录存在且可写，若存在旧的 `virtual_disk.bin`，程序会尝试加载它；格式化会覆盖数据。

如果你需要我把 `OPERATIONS.md` 改为更精简或更详细的版本（或放到 `docs/`），告诉我具体风格，我会调整.