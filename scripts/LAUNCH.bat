@echo off
chcp 65001 >nul 2>&1
color 0B
cls
cd /d "%~dp0.."

echo.
echo ================================================
echo   VFS Virtual File System Launcher
echo ================================================
echo.

REM 检查文件是否存在
if not exist "bin\vfs_api.exe" (
    echo [ERROR] Missing bin\vfs_api.exe
    echo Build first: gcc -o bin\\vfs_api.exe src\\core\\main.c ...
    pause
    exit /b 1
)

echo [1] Start backend API
echo [2] Start Electron app
echo [3] Open browser (http://localhost:8888)
echo [4] Start backend + open browser
echo [5] Start backend + Electron
echo.
set /p choice="Select [1-5]: "

if "%choice%"=="1" (
    echo.
    echo Starting backend API...
    echo Port: 8888
    echo.
    bin\vfs_api.exe
) else if "%choice%"=="2" (
    echo.
    if not exist "ui\electron\node_modules" (
        echo Installing Electron dependencies...
        cd ui\electron
        call npm install --legacy-peer-deps
        if errorlevel 1 goto error
        cd ..
    )
    echo.
    echo Starting Electron app...
    cd ui\electron
    call npm start
    cd ..
) else if "%choice%"=="3" (
    echo.
    echo Make sure backend is running. Press Enter to open browser...
    pause
    start http://localhost:8888
) else if "%choice%"=="4" (
    echo.
    echo Starting backend API...
    start "VFS Backend" cmd /k "bin\\vfs_api.exe"
    timeout /t 2
    echo.
    echo Opening browser...
    start http://localhost:8888
    pause
) else if "%choice%"=="5" (
    echo.
    echo Starting backend API...
    start "VFS Backend" cmd /k "bin\\vfs_api.exe"
    timeout /t 2
    echo.
    if not exist "ui\electron\node_modules" (
        echo Installing Electron dependencies...
        cd ui\electron
        call npm install --legacy-peer-deps
        cd ..
    )
    echo Starting Electron app...
    cd ui\electron
    call npm start
    cd ..
) else (
    echo.
    echo Invalid selection.
    pause
    exit /b 1
)

exit /b 0

:error
echo.
echo npm install failed.
echo Check:
echo   1. Node.js is installed
echo   2. npm works
echo   3. Network is available
pause
exit /b 1
