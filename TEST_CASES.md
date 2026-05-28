# VFS 文件系统测试用例

## 测试执行指南

```bash
# 编译项目
make clean && make

# 首次运行（需要格式化）
./vfs_new.exe < test_vfs.txt

# 或者交互式使用
./vfs_new.exe
```

## 测试场景

### TC-001: 系统初始化与格式化

**目的**: 验证虚拟磁盘初始化

**操作步骤**:
1. 选择格式化 (1)
2. 验证根目录创建
3. 验证 root 用户创建

**预期结果**:
- ✅ virtual_disk.bin 创建成功
- ✅ "Format completed!" 提示
- ✅ 根 inode (inode 1) 存在

---

### TC-002: 用户登录与登出

**目的**: 测试用户认证机制

**操作步骤**:
```
login
root
root           # 密码
whoami
logout
```

**预期结果**:
- ✅ "Login successful!" 提示
- ✅ whoami 显示 "uid=0(root) gid=0"
- ✅ "Logout successful!" 提示
- ✅ 登出后无法执行文件操作

**故障处理**:
- 错误密码应显示 "Invalid username or password!"
- 未登录时操作应提示 "Please login first!"

---

### TC-003: 目录操作 - 创建与导航

**目的**: 测试多级目录支持

**操作步骤**:
```
login
root
root
mkdir projects
mkdir archives
dir
chdir projects
pwd
mkdir work
dir
chdir ..
dir
```

**预期结果**:
- ✅ projects 和 archives 目录创建
- ✅ dir 显示两个目录项
- ✅ chdir projects 成功
- ✅ pwd 显示当前目录信息
- ✅ work 子目录创建在 projects 内
- ✅ chdir .. 返回根目录

**验证点**:
```
d--- ------ 2 0 work        # 目录项格式
-rw- ------ 1 0 file.txt    # 文件项格式
```

---

### TC-004: 文件创建与读写

**目的**: 测试基本文件 I/O

**操作步骤**:
```
login
root
root
touch empty.txt
create document.txt
y
Hello World!
This is line 2.
EOF
cat document.txt
open document.txt
read 0 50
close 0
```

**预期结果**:
- ✅ touch 创建空文件（大小 0）
- ✅ create 创建文件并写入内容
- ✅ cat 显示完整文件内容
- ✅ open 返回文件描述符
- ✅ read 读取指定字节数
- ✅ close 成功关闭

**验证点**:
```
File created successfully!
Content read:
Hello World!
This is line 2.
```

---

### TC-005: 文件复制与重命名

**目的**: 测试文件复制和重命名

**操作步骤**:
```
login
root
root
create original.txt
y
Source content
EOF
copy original.txt backup.txt
rename backup.txt archive.txt
dir
stat archive.txt
```

**预期结果**:
- ✅ original.txt 创建并写入内容
- ✅ backup.txt 是 original.txt 的副本
- ✅ archive.txt 包含原始内容
- ✅ dir 显示三个文件
- ✅ stat 显示正确的大小和权限

---

### TC-006: 硬链接

**目的**: 测试硬链接和链接计数

**操作步骤**:
```
login
root
root
create linktest.txt
y
Original content
EOF
stat linktest.txt
link linktest.txt link1.txt
stat linktest.txt
link linktest.txt link2.txt
stat linktest.txt
cat link1.txt
cat link2.txt
```

**预期结果**:
- ✅ linktest.txt 创建（Links: 1）
- ✅ link1.txt 创建后 (Links: 2)
- ✅ link2.txt 创建后 (Links: 3)
- ✅ 所有链接显示相同内容
- ✅ 链接数正确递增

---

### TC-007: 文件删除

**目的**: 测试文件删除和资源回收

**操作步骤**:
```
login
root
root
create file1.txt
y
Content 1
EOF
create file2.txt
y
Content 2
EOF
dir
delete file1.txt
dir
stat file2.txt
```

**预期结果**:
- ✅ 初始 dir 显示 2 个文件
- ✅ delete 成功
- ✅ 删除后 dir 显示 1 个文件
- ✅ file2.txt 仍可访问

---

### TC-008: 权限管理

**目的**: 测试文件权限设置和检查

**操作步骤**:
```
login
root
root
create test.txt
y
Test content
EOF
stat test.txt
chmod 644 test.txt
stat test.txt
chmod 755 test.txt
stat test.txt
```

**预期结果**:
- ✅ 创建文件默认权限 0644 或 0755
- ✅ chmod 644 设置成功
- ✅ stat 显示 Mode: 0644
- ✅ chmod 755 设置成功
- ✅ stat 显示 Mode: 0755

**预期权限显示**:
```
Mode: 0644 (-rw-r--r--)
Mode: 0755 (-rwxr-xr-x)
```

---

### TC-009: 多用户管理

**目的**: 测试用户添加、删除和切换

**操作步骤**:
```
login
root
root
whoami
useradd alice
useradd bob
logout
login
alice
alice
whoami
create alice_file.txt
y
Alice's data
EOF
logout
login
bob
bob
whoami
cat alice_file.txt
logout
login
root
root
userdel alice
```

**预期结果**:
- ✅ root 显示 uid=0
- ✅ alice 和 bob 添加成功
- ✅ alice 登录显示正确的 UID
- ✅ alice_file.txt 创建成功
- ✅ bob 登录可见 alice_file.txt
- ✅ bob 可读 alice_file.txt
- ✅ alice 删除成功

**验证点**:
```
alice login:  uid=1(alice) gid=1
bob login:    uid=2(bob) gid=2
root login:   uid=0(root) gid=0
```

---

### TC-010: 权限检查

**目的**: 测试权限限制和访问控制

**操作步骤**:
```
login
root
root
create secret.txt
y
Secret data
EOF
chmod 600 secret.txt
logout
login
alice
alice
cat secret.txt      # 应失败
logout
login
root
root
chmod 644 secret.txt
logout
login
alice
alice
cat secret.txt      # 应成功
```

**预期结果**:
- ✅ Root 创建 secret.txt
- ✅ chmod 600 只允许所有者读写
- ✅ alice 无权读取（Permission denied）
- ✅ chmod 644 后 alice 可读
- ✅ cat 显示内容成功

---

## 测试结果汇总模板

| 测试用例 | 结果 | 备注 |
|---------|------|------|
| TC-001 | ✅/❌ | |
| TC-002 | ✅/❌ | |
| TC-003 | ✅/❌ | |
| TC-004 | ✅/❌ | |
| TC-005 | ✅/❌ | |
| TC-006 | ✅/❌ | |
| TC-007 | ✅/❌ | |
| TC-008 | ✅/❌ | |
| TC-009 | ✅/❌ | |
| TC-010 | ✅/❌ | |

---

**更新**: 2024 年
**维护者**: 课程设计团队
