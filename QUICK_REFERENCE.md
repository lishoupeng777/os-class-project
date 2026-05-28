# VFS 快速参考卡片

## 系统常数速查

| 常数 | 值 | 说明 |
|------|-----|------|
| BLOCKSIZ | 512 B | 块大小 |
| DISK_SIZE | 4 MB | 虚拟磁盘大小 |
| NINODE | 128 | 最大文件数 |
| FILEBLK | 512 | 最大块数 |
| USERNUM | 10 | 最大用户数 |
| NOFILE | 20 | 每用户最大打开文件数 |
| DIRSIZ | 14 | 文件名最大长度 |

## 命令快速参考

### 用户命令
```
login           登录用户
logout          登出用户
whoami          显示当前用户
useradd <name>  添加用户 (root 专用)
userdel <name>  删除用户 (root 专用)
```

### 目录命令
```
mkdir <dir>     创建目录
chdir <dir>     改变目录 (支持 .. 返回)
pwd             显示当前目录
dir             列出目录内容
```

### 文件操作
```
touch <file>    创建空文件
create <file>   创建文件并写入
cat <file>      显示文件内容
delete <file>   删除文件
rename <old> <new>  重命名文件
copy <src> <dst>    复制文件
link <src> <dst>    创建硬链接
```

### 文件 I/O
```
open <file>     打开文件 (返回 fd)
close <fd>      关闭文件
read <fd> <size>    读文件
write <fd>      写文件
```

### 权限与信息
```
chmod <mode> <file>  改变权限 (8进制)
stat <file>         显示文件信息
```

### 系统命令
```
format          格式化虚拟磁盘
help            显示帮助
quit            退出并保存
```

## 权限速查

### 权限位
```
4 = 读 (r)
2 = 写 (w)
1 = 执行 (x)

755 = rwxr-xr-x  (所有者rwx, 组rx, 其他rx)
644 = rw-r--r--  (所有者rw, 组r, 其他r)
600 = rw-------  (仅所有者读写)
700 = rwx------  (仅所有者读写执行)
```

### 权限字段
```
Mode = [文件类型][权限]
       [4位]     [12位]

文件类型：
  0x4000 = 目录 (d)
  0x8000 = 普通文件 (-)

权限位 (12 位)：
  [bit11-9]   所有者 (owner)
  [bit8-6]    同组 (group)
  [bit5-3]    其他 (other)
```

## inode 结构速查

```c
struct dinode {
    di_mode         // 文件类型 + 权限
    di_nlink        // 硬链接计数
    di_uid          // 所有者 UID
    di_gid          // 所有者 GID
    di_size         // 文件大小
    di_addr[10]     // 块地址：
                    //   [0-7] 直接块
                    //   [8]   一级间接
                    //   [9]   二级间接
    di_atime        // 访问时间
    di_mtime        // 修改时间
}
```

## 文件寻址

```
直接块：8 块 × 512B = 4 KB
  di_addr[0] → 块 0
  di_addr[1] → 块 1
  ...
  di_addr[7] → 块 7

一级间接：128 块 × 512B = 64 KB
  di_addr[8] → 间接块
           ├→ 块 0
           ├→ 块 1
           └→ 块 127

二级间接：128×128 块 × 512B = 8 MB
  di_addr[9] → 二级间接块
           ├→ 一级间接块 0
           │  ├→ 块 0
           │  └→ 块 127
           └→ 一级间接块 127
              ├→ 块 0
              └→ 块 127

最大文件大小 = 8 + 128 + (128×128) = 16,648 块 = 8+ MB
```

## 目录项结构

```c
struct dir_entry {
    de_name[14]     // 文件名 (最多 14 字符)
    de_ino          // inode 号
}

一个 512B 块可容纳：
512 / 16 = 32 个目录项
```

## 快速操作示例

### 创建并使用文件
```
login
root
root

touch empty.txt           # 创建空文件

create myfile.txt         # 创建并编辑
y                         # 确认写入
Hello, World!             # 文件内容
EOF                       # 结束

cat myfile.txt            # 查看内容

copy myfile.txt backup.txt   # 备份

stat myfile.txt           # 查看详细信息
```

### 目录操作
```
mkdir projects            # 创建目录

cd projects               # 进入目录

pwd                      # 显示当前目录

mkdir work               # 创建子目录

cd work                  # 进入子目录

cd ..                    # 返回上级

cd /                     # 返回根目录

dir                      # 列出内容
```

### 权限管理
```
chmod 755 script.sh      # 可执行文件

chmod 644 data.txt       # 普通数据文件

chmod 600 secret.txt     # 私密文件

stat file.txt            # 查看权限
```

### 硬链接
```
create original.txt
y
Original data
EOF

link original.txt link1.txt     # 创建硬链接

stat original.txt              # 查看链接数

delete original.txt            # 删除不影响链接

cat link1.txt                  # 仍可访问原数据
```

## 常见错误及解决

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| "Please login first!" | 未登录 | `login` 先登录 |
| "File not found!" | 文件不存在 | 检查路径和文件名 |
| "Permission denied!" | 权限不足 | 检查文件权限或用 root |
| "Directory already exists!" | 目录已存在 | 使用不同名字 |
| "Disk space exhausted!" | 磁盘已满 | 删除不需要的文件 |
| "No more i-nodes available!" | inode 用完 | 文件数超过 128 个 |

## 磁盘布局速查

```
0          Boot Block (未使用)
1          Superblock
2-33       Inode 表 (32 块)
34-545     数据区 (512 块)
```

**物理地址计算**：
```
Inode i 的位置 = DINODESTART + (i-1) * 32
              = 1024 + (i-1) * 32

块 b 的位置 = DATASTART + b * 512
           = 17408 + b * 512
```

## 缓存机制

### inode Hash 表
```
ihash(ino) = &hinode[ino & 127]

查找时间：O(1) 平均情况
最多缓存：128 个 inode
管理：引用计数自动释放

iget(ino)   - 获取 inode (ref_count++)
iput(mip)   - 释放 inode (ref_count--)
```

## 文件描述符

### 文件描述符范围
```
用户进程 fd：0-19 (NOFILE=20)
系统打开文件表索引：0-39 (SYSOPENFILE=40)

用户可最多打开 20 个文件
但不同用户可共享同一打开文件表项
```

### 打开模式
```
O_RDONLY (0x01)   - 只读
O_WRONLY (0x02)   - 只写
O_RDWR   (0x03)   - 读写
O_APPEND (0x08)   - 追加
```

## 用户信息

### 默认用户
```
UID=0, 用户名=root, 密码=root
```

### 添加用户示例
```
login           # 用 root 登录
root
root

useradd alice   # 添加用户
password        # 输入密码

logout
login           # 以 alice 登录
alice
password
```

## 性能特性

| 操作 | 时间复杂度 | 说明 |
|------|----------|------|
| 块分配 (balloc) | O(1) | 栈操作 |
| Inode 查找 (iget) | O(1) 平均 | Hash 表 |
| 路径解析 (namei) | O(深度×目录大小) | 逐级查找 |
| 读文件 | O(文件大小/512) | 块数次 I/O |
| 权限检查 (access) | O(1) | 位操作 |

## 故障排查

### 文件系统损坏恢复
```bash
# 备份现有虚拟磁盘
cp virtual_disk.bin virtual_disk.bin.bak

# 删除虚拟磁盘
rm virtual_disk.bin

# 重新启动程序
./vfs

# 选择格式化
1
```

### 调试技巧
```
stat <file>        # 检查文件信息
dir                # 检查目录内容
whoami             # 检查当前用户
help               # 查看所有命令
```

## 设计理念要点

1. **UNIX 哲学**：一切皆文件（甚至目录）
2. **分层设计**：磁盘→缓存→内存→应用
3. **资源管理**：引用计数、延迟写
4. **安全性**：权限检查、用户隔离
5. **性能**：Hash 表、栈分配、缓存

## 学习路径

1. 理解 inode 结构
2. 学习块分配算法
3. 掌握路径解析
4. 理解权限模型
5. 学习缓存机制
6. 实践多用户系统

## 推荐阅读

- vfs.h：数据结构定义
- vfs.c：核心算法实现
- commands.c：命令处理
- README.md：使用指南
- DESIGN.md：设计深入
- TEST_CASES.md：测试用例

---

**快速参考 v1.0** | 最后更新：2024 年

