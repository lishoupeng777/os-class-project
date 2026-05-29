# 多用户、多级目录结构文件系统模拟系统

## 项目概述

这是一个基于 C 语言实现的虚拟文件系统，模拟 UNIX 风格的文件系统结构，支持多用户、多级目录、文件权限管理等核心功能。该项目参考了张尧学《操作系统教程习题与解答》中的文件系统设计理论。

## 核心特性

### 1. **多用户支持**
- 用户认证系统（login/logout）
- 用户管理（useradd/userdel）
- 基于 UID/GID 的权限控制
- 密码保护

### 2. **多级目录结构**
- 支持创建递归目录（mkdir）
- 目录导航（chdir）
- 路径解析（绝对路径和相对路径）
- 当前工作目录显示（pwd）

### 3. **文件管理**
- 文件创建、读取、写入、删除
- 硬链接支持
- 文件复制和重命名
- 文件统计信息显示

### 4. **权限管理**
- Unix 风格权限位（owner/group/other 的 rwx）
- chmod 命令更改权限
- 权限检查机制

### 5. **持久化存储**
- 虚拟磁盘（virtual_disk.bin）
- 文件系统状态保存和恢复

## 文件系统架构

### 存储结构

```
Virtual Disk (4MB)
├── Block 0: Boot Block
├── Block 1: Superblock
│   ├── Inode Free 栈 (50 个 Inode)
│   ├── Block Free 栈 (50 个 Block)
│   └── 文件系统元数据
├── Block 2-33: Inode Table (32 块，共 128 个 Inode)
└── Block 34+: Data Blocks (512 块数据块)
```

### 关键数据结构

#### Dinode (磁盘 Inode)
```c
struct dinode {
    uint16_t di_mode;      // 文件类型和权限
    uint8_t  di_nlink;     // 链接计数
    uint8_t  di_uid;       // 所有者 UID
    uint16_t di_gid;       // 所有者 GID
    uint32_t di_size;      // 文件大小
    uint32_t di_addr[10];  // 块地址（8 直接 + 1 一级间接 + 1 二级间接）
    uint16_t di_atime;     // 访问时间
    uint16_t di_mtime;     // 修改时间
}
```

#### Minode (内存 Inode)
- 在内存中缓存的 Dinode
- Hash 表实现快速查找
- 引用计数机制

### 块分配算法

采用"空闲块栈"结构：
- 超级块中维护 50 个空闲块号栈
- 栈满时，栈顶块作为"链接块"，包含下一组 50 个空闲块号
- 空闲块回收时加入栈

### Inode 分配算法

采用"空闲 Inode 栈"结构：
- 超级块中维护 50 个空闲 Inode 号栈
- 类似块分配的链接机制

## 命令参考

### 用户管理

| 命令 | 说明 | 示例 |
|------|------|------|
| `login` | 用户登录 | `login` 后输入用户名和密码 |
| `logout` | 用户登出 | `logout` |
| `whoami` | 显示当前用户信息 | `whoami` |
| `useradd <name>` | 添加用户（仅 root） | `useradd alice` |
| `userdel <name>` | 删除用户（仅 root） | `userdel alice` |

### 目录操作

| 命令 | 说明 | 示例 |
|------|------|------|
| `mkdir <dir>` | 创建目录 | `mkdir projects` |
| `chdir <dir>` | 改变目录 | `chdir projects` 或 `chdir ..` |
| `pwd` | 显示当前目录 | `pwd` |
| `dir` | 列出目录内容 | `dir` |

### 文件操作

| 命令 | 说明 | 示例 |
|------|------|------|
| `touch <file>` | 创建空文件 | `touch notes.txt` |
| `create <file>` | 创建文件（可写入内容） | `create data.txt` |
| `cat <file>` | 显示文件内容 | `cat notes.txt` |
| `open <file>` | 打开文件 | `open data.txt` |
| `close <fd>` | 关闭文件 | `close 0` |
| `read <fd> <size>` | 读文件 | `read 0 100` |
| `write <fd>` | 写文件 | `write 0` |
| `copy <src> <dest>` | 复制文件 | `copy file1.txt file2.txt` |
| `rename <old> <new>` | 重命名文件 | `rename old.txt new.txt` |
| `delete <file>` | 删除文件 | `delete temp.txt` |
| `link <src> <dest>` | 创建硬链接 | `link file.txt link.txt` |

### 权限管理

| 命令 | 说明 | 示例 |
|------|------|------|
| `chmod <mode> <file>` | 改变文件权限 | `chmod 755 script.sh` |
| `stat <file>` | 显示文件详细信息 | `stat file.txt` |

### 系统管理

| 命令 | 说明 | 示例 |
|------|------|------|
| `format` | 格式化虚拟磁盘 | `format` |
| `help` | 显示帮助信息 | `help` |
| `quit` | 退出并保存 | `quit` |

## 使用示例

### 示例 1: 基本文件操作

```bash
VFS> login                    # 登录
Username: root
Password: root
Login successful!

VFS> touch myfile.txt         # 创建空文件
File created successfully!

VFS> mkdir docs               # 创建目录
Directory created successfully!

VFS> chdir docs               # 进入目录
Current directory changed!

VFS> create report.txt        # 创建文件并写入内容
Do you want to write content now? (y/n): y
Enter content (type 'EOF' on a new line to end):
This is my report.
EOF
File content written successfully!

VFS> cat report.txt           # 显示文件内容
This is my report.

VFS> pwd                      # 显示当前目录
Current directory: (internal inode 2)

VFS> dir                      # 列出目录内容
-rw------- 1 0 17 report.txt
```

### 示例 2: 多用户与权限

```bash
VFS> login                    # root 登录
Username: root
Password: root

VFS> useradd alice            # 添加用户
Password: alice123
User added successfully!

VFS> logout                   # 登出

VFS> login                    # alice 登录
Username: alice
Password: alice123
Login successful!

VFS> chmod 644 file.txt       # 改变权限
Permissions changed successfully!

VFS> stat file.txt            # 查看文件统计
=== File Statistics ===
Inode: 5
Mode: 0644 (-rw-r--r--)
Links: 1
Owner UID: 0
Group GID: 0
Size: 1234 bytes
```

### 示例 3: 目录导航与路径

```bash
VFS> mkdir -p a/b/c           # 创建多级目录
VFS> chdir a/b/c              # 进入深层目录
VFS> pwd                      # 显示当前路径
/a/b/c

VFS> create file.txt          # 在当前目录创建文件
VFS> chdir ../..              # 返回上级目录
VFS> dir a/b                  # 查看指定目录
```

## 编译与运行

### 编译

```bash
cd "d:\trae project\os"
make clean
make
```

### 运行

```bash
./vfs
# 或 ./vfs_new.exe (Windows)
```

首次运行时会提示是否格式化虚拟磁盘，选择"1"创建新的文件系统。

### 清理

```bash
make clean  # 删除编译文件和虚拟磁盘
```

## 设计特点

### 1. UNIX 风格的 Inode 结构
- 文件类型和权限集成在 mode 字段
- 支持硬链接（link count）
- 块地址采用直接 + 间接索引

### 2. 空闲资源管理
- 超级块中维护空闲块/Inode 栈
- 减少磁盘寻址时间
- 支持大文件（>8 块）的间接引用

### 3. 用户与权限
- 三层权限控制（owner/group/other）
- 权限位与文件类型分离存储
- root 用户特殊权限

### 4. 内存缓存
- Inode Hash 表快速查找
- 引用计数自动释放
- 双向链表便于管理

## 限制说明

| 限制项 | 值 | 说明 |
|--------|-----|------|
| 磁盘大小 | 4 MB | 虚拟磁盘总容量 |
| 最大 Inode 数 | 128 | 文件/目录总数 |
| 最大块数 | 512 | 数据块总数 |
| 块大小 | 512 字节 | 每个数据块 |
| 最大用户数 | 10 | 系统用户数 |
| 每用户打开文件数 | 20 | 用户进程打开文件限制 |
| 系统打开文件表项 | 40 | 全局打开文件表 |
| 文件名长度 | 14 字符 | 目录条目名字段 |
| 路径长度 | 256 字符 | 路径字符串最大长度 |

## 关键算法

### 路径解析 (namei)
1. 检查路径首字符确定起点（/表示根，否则为当前目录）
2. 逐级解析目录名，查找目录项
3. 返回目标 Inode 的内存缓存

### 块分配 (balloc)
1. 从超级块的空闲块栈栈顶取块号
2. 若栈指针 < 0，从栈底块读取下一组空闲块号
3. 更新超级块计数器

### Inode 查找 (iget)
1. 计算 Inode 号的 Hash 值
2. 在 Hash 链表中查找（已缓存则增加引用计数）
3. 未缓存则从磁盘读取，插入 Hash 链表

### 文件删除 (cmd_delete)
1. 递归释放所有数据块（支持间接块）
2. 更新目录项（标记为 0）
3. 回收 Inode 号到空闲栈

## 持久化机制

### 存储
- 所有 Inode 和数据块写入 virtual_disk.bin
- 超级块标志 s_modified 跟踪修改状态
- quit 命令触发完整保存

### 恢复
- 启动时检测 virtual_disk.bin 文件
- 自动加载系统状态
- 用户表需重新初始化（root 用户）

## 完整性检查清单

- ✅ 多用户系统（login/logout/useradd/userdel）
- ✅ 多级目录（mkdir/chdir/pwd）
- ✅ 文件操作（create/read/write/delete/copy）
- ✅ 硬链接（link）
- ✅ 权限管理（chmod）
- ✅ 文件统计（stat）
- ✅ 虚拟磁盘持久化
- ✅ Inode Hash 缓存
- ✅ 空闲块/Inode 栈管理
- ✅ 间接块支持
- ✅ 目录项查找
- ✅ 权限检查

## 参考资源

- 《UNIX V6 操作系统源代码分析》- Bell Labs
- 《现代操作系统》- Andrew S. Tanenbaum
- 《操作系统教程习题与解答 第 3、4 版》- 张尧学等著

## 作者

课程设计项目 - 操作系统文件系统模块

## 许可证

Educational Use Only

---

**最后更新**: 2024 年

**项目状态**: 完成并测试
