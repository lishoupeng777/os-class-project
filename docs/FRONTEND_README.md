# VFS 虚拟文件系统 - 高端桌面应用

## 项目结构

```
├── src\core\                # VFS 核心实现
│   ├── main.c
│   └── vfs.c
├── src\commands\            # 命令处理模块
├── src\api\api.c            # HTTP API 服务（支持前端通信）
├── include\vfs.h            # 头文件
├── bin\                     # 可执行文件
└── ui\electron\             # Electron 桌面应用
    ├── main.js              # 主进程
    ├── preload.js           # 预加载脚本
    ├── package.json
    └── src\index.html       # 前端 UI（深色主题+毛玻璃效果）
```

## 功能特性

### 🎨 前端设计
- **现代深色主题** - 渐变背景 + 毛玻璃效果
- **文件树形导航** - 实时显示文件系统结构
- **仪表板** - 磁盘/Inode 使用率图表
- **模态窗口** - 登录、创建文件、格式化等操作
- **响应式布局** - 侧边栏 + 主内容区 + 详情面板

### 🔧 后端服务
- **HTTP API** (`api.c`)
  - `GET /api/status` - 系统状态（磁盘/Inode 使用）
  - `GET /api/dir` - 文件列表
  - `GET /api/pwd` - 当前目录信息
- **端口**: `8888`
- **CORS** 支持跨域请求

### 👥 用户管理
- 多用户登录/登出
- 用户权限控制 (rwx)
- 密码管理

## 快速开始

### 方式 1：现代 Web 界面（推荐）

#### 步骤 1：启动后端 API
```bash
cd d:\trae project\os
gcc -Wall -Wextra -g -Iinclude -o bin\vfs_api.exe src\core\main.c src\core\vfs.c src\commands\commands_system.c src\commands\commands_dir.c src\commands\commands_file_io.c src\commands\commands_file_ops.c src\commands\commands_user.c src\api\api.c -lws2_32
bin\vfs_api.exe
```

#### 步骤 2：在浏览器打开前端
```
打开: ui\electron\src\index.html
```

### 方式 2：Electron 桌面应用

#### 前置要求
- Node.js 14+ 
- npm 

#### 安装与运行
```bash
cd d:\trae project\os\ui\electron
npm install
npm start
```

#### 或开发模式（支持热重载）
```bash
npm run dev
```

## API 文档

### 获取系统状态
```bash
GET /api/status

响应:
{
  "total_blocks": 512,
  "used_blocks": 45,
  "total_inodes": 1024,
  "used_inodes": 128,
  "current_user": "root"
}
```

### 获取目录列表
```bash
GET /api/dir

响应:
{
  "entries": [
    {
      "name": "dir1",
      "type": "dir",
      "size": 1024,
      "owner": 0,
      "perm": "rwx"
    },
    {
      "name": "file1.txt",
      "type": "file",
      "size": 512,
      "owner": 0,
      "perm": "rw-"
    }
  ]
}
```

### 获取当前路径
```bash
GET /api/pwd

响应:
{
  "cwd": "/",
  "user": "root",
  "uid": 0
}
```

## 使用场景示例

### 演示流程
1. **格式化虚拟磁盘**
   - 启动程序后输入 `1` 格式化
   - 或在 GUI 中点击 "⚡ 格式化"

2. **用户登录**
   - 默认用户：`root` / `root`
   - 在左侧菜单点击 "🔐 用户登录"

3. **创建目录和文件**
   - 点击 "📂 新建文件夹" 或 "📄 新建文件"
   - 输入名称后确认

4. **监控系统状态**
   - 右侧面板实时显示磁盘使用率
   - Inode 节点使用情况

## 技术栈

### 后端
- **C 语言** - 核心文件系统实现
- **HTTP 服务** - 基于 Socket 的 API 服务
- **多线程** - 支持并发请求（可选优化）

### 前端
- **Electron** - 跨平台桌面应用框架
- **HTML5 / CSS3** - 现代 Web 标准
- **JavaScript (原生)** - 无框架依赖，轻量级

## 界面设计说明

### 色彩方案
- **主色** 紫色梯度: `#667eea` → `#764ba2`
- **背景** 深蓝黑色: `#0f0c29`
- **文本** 浅灰色: `#e0e0e0`
- **强调** 紫色: `#667eea`

### 布局
```
┌─────────────┬────────────────────────────────────┐
│             │  工具栏                              │
│   边栏      ├────────────────────────────────────┤
│   导航      │                                    │
│   操作      │  主内容区                           │
│   快捷      │                                    │
│             │                                    │
└─────────────┴────────────────────────────────────┘
```

### 交互特性
- ✨ 悬停效果 - 菜单项、按钮有渐变动画
- 🎯 活跃状态 - 当前选中项高亮显示
- 📊 实时更新 - 每 5 秒自动刷新数据
- 🔒 模态窗口 - 毛玻璃背景弹窗

## 编译和调试

### 编译后端（包含 API）
```bash
gcc -Wall -Wextra -g -o vfs main.c vfs.c commands_*.c api.c -lws2_32
```

### 开启调试
```bash
# 在 Electron 中
win.webContents.openDevTools();  # main.js 已启用

# 在终端监控 API
curl http://localhost:8888/api/status
```

## 常见问题

### Q: 前端无法连接后端？
**A:** 确保：
1. VFS 后端程序正在运行
2. 防火墙未阻止 8888 端口
3. 检查浏览器控制台错误 (F12)

### Q: Electron 启动失败？
**A:** 
```bash
npm install electron --save-dev
npm start
```

### Q: 如何修改界面主题？
**A:** 编辑 `ui\electron\src\index.html`，搜索 `:root` 修改 CSS 变量。

## 下一步优化

- [ ] 后端多线程支持
- [ ] WebSocket 实时推送
- [ ] 文件上传/下载
- [ ] 权限配置面板
- [ ] 系统日志查看器
- [ ] 磁盘快照导出

---

**作者**: VFS 课程设计  
**版本**: 1.0.0  
**许可**: MIT
