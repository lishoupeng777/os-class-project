@echo off
chcp 65001 >nul 2>&1
color 0B
cls
cd /d "%~dp0"

echo ================================================
echo   VFS 虚拟文件系统
echo ================================================
echo.

if not exist "bin\vfs.exe" (
    echo [正在编译] 未找到 vfs.exe，开始编译...
    gcc -Wall -Wextra -g -Iinclude -o bin\\vfs.exe src\\core\\main.c src\\core\\vfs_globals.c src\\core\\vfs_disk.c src\\core\\vfs_block.c src\\core\\vfs_inode.c src\\core\\vfs_path.c src\\core\\vfs_perm.c src\\commands\\commands_system.c src\\commands\\commands_dir.c src\\commands\\commands_file_io.c src\\commands\\commands_file_ops.c src\\commands\\commands_user.c
    if errorlevel 1 (
        echo 编译失败，请确保已安装 MinGW GCC
        pause
        exit /b 1
    )
    echo 编译成功！
    echo.
)

echo 正在启动 VFS 终端...
echo 提示：输入 help 查看可用命令
echo.
bin\vfs.exe

echo.
echo 程序已退出。
pause