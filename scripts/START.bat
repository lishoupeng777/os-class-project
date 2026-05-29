@echo off
REM VFS 虚拟文件系统启动脚本
cd /d "%~dp0.."

echo.
echo ╔════════════════════════════════════════════════════════╗
echo ║      VFS 虚拟文件系统 - 高端桌面应用启动器              ║
echo ║                  Virtual File System v1.0               ║
echo ╚════════════════════════════════════════════════════════╝
echo.

REM 检查编译是否存在
if not exist "bin\vfs_api.exe" (
    echo ⚠️  未找到 bin\vfs_api.exe，正在编译...
    gcc -Wall -Wextra -g -Iinclude -o bin\vfs_api.exe src\core\main.c src\core\vfs.c src\commands\commands_system.c src\commands\commands_dir.c src\commands\commands_file_io.c src\commands\commands_file_ops.c src\commands\commands_user.c src\api\api.c -lws2_32
    if errorlevel 1 (
        echo ❌ 编译失败！
        pause
        exit /b 1
    )
    echo ✅ 编译成功！
)

REM 清理旧磁盘文件（可选）
if exist "virtual_disk.bin" (
    echo 检测到已有虚拟磁盘，保持使用...
) else (
    echo 📝 将创建新的虚拟磁盘...
)

echo.
echo 🚀 正在启动 VFS 后端 API 服务...
echo    • 后端地址: http://localhost:8888
echo    • API 文档: http://localhost:8888/api/status
echo.
echo 📋 启动选项:
echo    1. 格式化新磁盘
echo    0. 恢复现有磁盘
echo    按 Ctrl+C 退出
echo.

REM 启动后端
start "VFS Backend" cmd /k "cd /d "%cd%" && bin\vfs_api.exe"

timeout /t 2

REM 尝试打开浏览器展示前端
if exist "ui\electron\src\index.html" (
    echo.
    echo 📌 前端文件位置: ui\electron\src\index.html
    echo.
    echo 启动选项:
    echo    • [1] 打开 Electron 桌面应用（推荐）
    echo    • [2] 在浏览器中打开
    echo    • [3] 仅运行后端
    echo    • [其他] 继续...
    echo.
    set /p choice="请选择 [1-3]: "
    
    if "%choice%"=="1" (
        if exist "ui\electron\node_modules" (
            echo ✅ 启动 Electron 应用...
            cd ui\electron
            npm start
        ) else (
            echo ⚠️  Electron 未安装，正在安装依赖...
            cd ui\electron
            call npm install
            if errorlevel 1 (
                echo ❌ npm 安装失败，请确保已安装 Node.js
                pause
            ) else (
                call npm start
            )
        )
    ) else if "%choice%"=="2" (
        echo 📱 在浏览器中打开前端...
        start http://localhost:8888
    ) else (
        echo ✅ 后端已启动，您可以手动打开浏览器访问 http://localhost:8888
        pause
    )
) else (
    echo ✅ 后端服务已启动！
    echo.
    echo 📝 后端命令行界面：
    echo    • format - 格式化磁盘
    echo    • login - 用户登录
    echo    • mkdir - 创建目录
    echo    • create - 创建文件
    echo    • help - 查看所有命令
    echo.
    pause
)
