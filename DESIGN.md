# VFS 文件系统设计文档

## 目录
1. [总体架构](#总体架构)
2. [数据结构](#数据结构)
3. [关键算法](#关键算法)
4. [多用户管理](#多用户管理)
5. [权限控制](#权限控制)
6. [缓存策略](#缓存策略)
7. [磁盘布局](#磁盘布局)

---

## 总体架构

### 系统组成

```
┌─────────────────────────────────────┐
│     用户命令接口 (main.c)           │
├─────────────────────────────────────┤
│     命令处理层 (commands.c)         │
├─────────────────────────────────────┤
│  文件系统核心 (vfs.c) + 数据结构    │
│  • Inode 管理                       │
│  • 块分配器                         │
│  • 路径解析                         │
├─────────────────────────────────────┤
│     虚拟磁盘 (virtual_disk.bin)     │
└─────────────────────────────────────┘
```

### 设计目标

1. **多用户支持**：用户隔离、权限控制
2. **多级目录**：递归目录结构、路径解析
3. **持久化**：虚拟磁盘持久存储
4. **简洁高效**：UNIX 风格设计，易于理解

---

## 数据结构

### 1. 超级块 (Superblock)

超级块存储文件系统全局元数据和空闲资源栈。

```c
struct super_block {
    int isize;           // Inode 表大小（32 块，共 128 个 inode）
    int ifree_num;       // 当前空闲 inode 数量
    int ifree[50];       // 空闲 inode 号栈
    int ifree_ptr;       // 栈指针
    int fsize;           // 数据块总数（512）
    int ffree_num;       // 当前空闲块数量
    int ffree[50];       // 空闲块号栈（FAT 风格）
    int ffree_ptr;       // 栈指针
    int s_modified;      // 修改标志
}
```

**设计原理**：
- 空闲资源采用**栈**结构（类似 FAT 的空闲簇栈）
- 栈满 (ptr >= 49) 时，栈顶块作为"链接块"，存储下一组 50 个资源号
- 减少了磁盘寻址次数，提高分配效率

### 2. Inode (Index Node)

**磁盘 Inode** (dinode)：
```c
struct dinode {
    uint16_t di_mode;       // 文件类型（高 4 位） + 权限（低 12 位）
    uint8_t  di_nlink;      // 硬链接计数
    uint8_t  di_uid;        // 所有者 UID
    uint16_t di_gid;        // 所有者 GID
    uint32_t di_size;       // 文件大小
    uint32_t di_addr[10];   // 块地址（8 直接 + 1 一级间接 + 1 二级间接）
    uint16_t di_atime;      // 访问时间
    uint16_t di_mtime;      // 修改时间
}
```

**内存 Inode** (minode)：
```c
struct minode {
    dinode dino;            // 磁盘 inode 副本
    int ino;                // Inode 号
    int ref_count;          // 引用计数（缓存管理）
    int m_flag;             // 修改标志
    minode *next, *prev;    // Hash 链表
}
```

**设计特点**：
- **模式字段**：文件类型和权限合并，节省空间
  ```
  di_mode = (S_IFDIR | S_IREAD | S_IWRITE | S_IEXEC)
           = (0x4000 | 0x0100 | 0x0080 | 0x0040)
           = 0x41C0
  ```
- **块地址**：直接索引 + 两级间接索引
  - `di_addr[0..7]`：直接块（8 * 512 = 4 KB）
  - `di_addr[8]`：一级间接块（128 * 512 = 64 KB）
  - `di_addr[9]`：二级间接块（128 * 128 * 512 = 8 MB）
- **引用计数**：当 ref_count 归零时自动释放内存

### 3. 目录项 (Directory Entry)

```c
struct dir_entry {
    char de_name[14];       // 文件名（最多 14 字符）
    uint16_t de_ino;        // 对应的 inode 号
}
```

**特点**：
- 目录是包含目录项的特殊文件
- 每个目录项 16 字节，一个 512 字节块可容纳 32 项
- 第一个条目总是 "."（指向自己）
- 第二个条目总是 ".."（指向父目录）

### 4. 打开文件表

**系统层** (sys_open_file)：
```c
struct sys_open_file {
    int of_mode;            // 打开模式（读/写/读写）
    int of_count;           // 打开计数（共享用）
    minode *of_minode;      // 关联的内存 inode
    int of_offset;          // 当前读写位置
}
```

**用户层** (user.u_ofile)：
- 每个用户有 20 个文件描述符（0-19）
- 文件描述符是系统打开文件表的索引（范围 0-39）

### 5. 用户表

```c
struct user {
    char u_name[14];        // 用户名
    char u_passwd[12];      // 密码
    uint8_t u_uid;          // 用户 ID（0=root）
    uint8_t u_gid;          // 组 ID
    int u_ofile[20];        // 打开文件表（-1 表示未使用）
    minode *u_cwd;          // 当前工作目录 inode
}
```

---

## 关键算法

### 1. 块分配算法 (balloc)

**算法伪码**：
```
balloc():
    if ffree_num == 0:
        return -1  # 空间耗尽
    
    blkno = ffree[ffree_ptr--]
    ffree_num--
    
    if ffree_ptr < 0 and ffree_num > 0:
        # 栈空，从链接块读取下一组
        next_block = blkno
        p = DATASTART + next_block * BLOCKSIZ
        ffree_ptr = 50 - 1
        ffree_num = 50
        for i in 0..49:
            ffree[i] = p[i]
        blkno = ffree[ffree_ptr--]
        ffree_num--
    
    return blkno
```

**空间复杂度**：O(1)  
**时间复杂度**：O(1) 平均情况，O(50) 链接块读取

**FAT 与栈的对比**：
| 特性 | FAT | 栈 |
|------|------|------|
| 空闲链查找 | 逐块遍历 | O(1) 栈操作 |
| 分配时间 | O(n) | O(1) |
| 实现复杂度 | 低 | 中 |
| 缓存友好性 | 低 | 高 |

### 2. Inode 分配算法 (ialloc)

与块分配类似，使用空闲 inode 栈。

```c
ialloc():
    if ifree_num == 0:
        return -1
    
    ino = ifree[ifree_ptr--]
    ifree_num--
    
    # 初始化 dinode
    dip = DINODESTART + (ino - 1) * DINODESIZ
    memset(dip, 0, DINODESIZ)
    dip->di_mode = IALLOC
    
    return ino
```

### 3. 路径解析算法 (namei)

**功能**：将路径字符串转换为 inode  
**输入**：路径（如 "/a/b/c" 或 "dir/file"）  
**输出**：目标 inode

**算法步骤**：

```c
namei(path):
    if path[0] == '/':
        cur = iget(ROOT_INODE)  # 绝对路径从根开始
        p = path + 1
    else:
        cur = iget(current_user->u_cwd->ino)  # 相对路径
        p = path
    
    while *p != '\0':
        # 提取下一个路径分量
        q = p
        while *q != '\0' and *q != '/': q++
        
        name = substring(p, q)
        
        # 在当前目录中查找 name
        de = cur->dino.di_addr[0] 的数据块
        found = false
        for each de[i]:
            if strcmp(de[i].de_name, name) == 0:
                cur = iget(de[i].de_ino)
                found = true
                break
        
        if not found:
            return -1  # 路径不存在
        
        p = (*q == '/') ? q + 1 : q
    
    return cur
```

**时间复杂度**：O(路径深度 * 目录大小)

### 4. 读文件算法 (cmd_read)

```c
read(fd, size):
    of = sys_ofile[current_user->u_ofile[fd]]
    mip = of->of_minode
    
    while total_read < size:
        # 根据偏移获取数据块
        blk = alloc_block(mip, offset)
        off_in_blk = offset % BLOCKSIZ
        
        # 计算读取长度
        read_len = min(
            BLOCKSIZ - off_in_blk,
            size - total_read,
            file_size - offset
        )
        
        # 从块中读数据
        src = DATASTART + blk * BLOCKSIZ + off_in_blk
        memcpy(buf, src, read_len)
        
        total_read += read_len
        offset += read_len
    
    of->of_offset = offset
```

### 5. Inode 缓存管理 (iget/iput)

**Hash 表实现**：

```c
#define ihash(x) (&hinode[(int)(x) & 127])  // x % 128

minode *iget(int ino):
    # 计算 Hash 值
    hash_head = ihash(ino)
    
    # 在 Hash 链中查找
    mip = *hash_head
    while mip:
        if mip->ino == ino:
            mip->ref_count++     # 找到，增加引用
            return mip
        mip = mip->next
    
    # 未找到，从磁盘读取
    mip = malloc(sizeof(minode))
    dip = DINODESTART + (ino - 1) * DINODESIZ
    memcpy(&mip->dino, dip, DINODESIZ)
    mip->ino = ino
    mip->ref_count = 1
    
    # 插入 Hash 链头
    mip->next = *hash_head
    if *hash_head:
        (*hash_head)->prev = mip
    *hash_head = mip
    
    return mip

void iput(minode *mip):
    mip->ref_count--
    
    if mip->ref_count > 0:
        return  # 还有其他引用
    
    # ref_count == 0，可以释放
    if mip->m_flag:
        # 写回磁盘
        dip = DINODESTART + (mip->ino - 1) * DINODESIZ
        memcpy(dip, &mip->dino, DINODESIZ)
    
    # 从 Hash 链中移除
    if mip->prev:
        mip->prev->next = mip->next
    else:
        *ihash(mip->ino) = mip->next
    if mip->next:
        mip->next->prev = mip->prev
    
    free(mip)
```

**缓存优势**：
- 快速查找：O(1) 平均时间
- 减少磁盘 I/O：频繁访问的 inode 驻留内存
- 自动管理：引用计数确保正确释放

---

## 多用户管理

### 用户认证

**登录流程**：
```
1. 用户输入用户名和密码
2. 遍历 users[] 表查找匹配项
3. 比对密码（明文，非加密）
4. 设置 current_user
5. 初始化用户上下文（打开文件表、工作目录等）
```

### 用户隔离

- **工作目录隔离**：每个用户有独立的 u_cwd
- **打开文件隔离**：每个用户有独立的 u_ofile[NOFILE]
- **权限检查**：每个操作都基于 current_user 进行权限验证

---

## 权限控制

### 权限模型

采用 UNIX 标准的 9 位权限模型：

```
di_mode 低 12 位布局：
[111][111][111][000]
 |||  |||  |||
 |||  |||  +--- 其他用户权限 (other)
 |||  +------- 同组用户权限 (group)
 +----------- 所有者权限 (owner)

每组 3 位：[执行][写][读]
即：[x][w][r]
```

**权限位定义**：
```c
#define S_IREAD  0x0100   // 所有者读权限
#define S_IWRITE 0x0080   // 所有者写权限
#define S_IEXEC  0x0040   // 所有者执行权限

// 组和其他通过移位获得
group_read  = (S_IREAD >> 3)    // 0x0020
group_write = (S_IWRITE >> 3)   // 0x0010
other_read  = (S_IREAD >> 6)    // 0x0004
```

### 权限检查函数

```c
int access(minode *mip, int mode):
    if current_user->u_uid == 0:
        return 0  # root 用户无限权限
    
    perm = mip->dino.di_mode
    
    if current_user->u_uid == mip->dino.di_uid:
        # 所有者权限检查
        if (mode & O_RDONLY) and !(perm & S_IREAD):
            return -1
        if (mode & O_WRONLY) and !(perm & S_IWRITE):
            return -1
    elif current_user->u_gid == mip->dino.di_gid:
        # 同组权限检查（右移 3 位）
        ...
    else:
        # 其他用户权限检查（右移 6 位）
        ...
    
    return 0  # 允许访问
```

---

## 缓存策略

### 四层缓存架构

```
层级 1：超级块 (sb) - 全局唯一
  |
层级 2：Inode Hash 表 (hinode[]) - 最多 128 个 inode
  |
层级 3：数据块缓存 - 虚拟磁盘内存
  |
层级 4：磁盘 (virtual_disk.bin)
```

### 写回策略

- **Dirty 标志**：m_flag 标记 inode 是否修改
- **延迟写**：修改仅更新内存，在 iput 或 quit 时写回
- **一致性**：quit 命令前保存所有修改

---

## 磁盘布局

### 物理布局

```
Block 0: Boot Block (未使用)
│
Block 1: Superblock
│        ├─ inode 空闲栈
│        └─ 块空闲栈
│
Blocks 2-33: Inode 表 (32 块，128 个 inode)
│            每个 inode 32 字节 => 512/32 = 16 个 inode/块
│
Blocks 34-545: 数据区 (512 块，512 KB 容量)
│              ├─ 文件数据
│              ├─ 目录数据
│              ├─ 间接块
│              └─ 链接块
│
总计: 4 MB 虚拟磁盘
```

### 寻址示例

**获取第 5 个 inode**：
```
inode 地址 = DINODESTART + (5-1) * DINODESIZ
          = (2 * 512) + 4 * 32
          = 1024 + 128
          = 1152 字节
```

**获取块 100 的数据**：
```
块地址 = DATASTART + 100 * BLOCKSIZ
       = (34 * 512) + 100 * 512
       = 17408 + 51200
       = 68608 字节
```

---

## 设计权衡

### 取舍分析

| 设计决策 | 优点 | 缺点 | 理由 |
|---------|------|------|------|
| 固定块大小 512B | 简单，兼容 | 碎片化 | 教学项目首选 |
| 空闲栈 | 快速分配 | 需链接块 | FAT 风格 + UNIX 风格混合 |
| Inode Hash | 快速查找 | 碰撞处理 | 128 个 inode 足够 |
| 延迟写 | 性能高 | 崩溃丢数据 | 教学项目可接受 |
| 明文密码 | 实现简单 | 不安全 | 教学项目无需加密 |

---

## 参考文献

1. UNIX V6 Kernel Source Code Analysis
2. "Operating Systems: Three Easy Pieces" - Arpaci-Dusseau
3. 张尧学《操作系统教程习题与解答》第 3、4 版
4. "The Design and Implementation of the FreeBSD Operating System"

---

**文档版本**：1.0  
**最后更新**：2024 年  
**作者**：操作系统课程设计小组
