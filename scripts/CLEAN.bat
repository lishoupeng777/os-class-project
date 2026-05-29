@echo off
cd /d "%~dp0.."

REM 删除所有多余的文档和测试文件
del /q docs\BEFORE_AFTER_COMPARISON.md 2>nul
del /q docs\BUGS_AND_FIXES.md 2>nul
del /q docs\CHECKLIST.md 2>nul
del /q docs\CHOWN_COMPLETE.md 2>nul
del /q docs\CHOWN_IMPLEMENTATION.md 2>nul
del /q docs\CHOWN_SUMMARY.md 2>nul
del /q docs\CURRENT_STATUS_SUMMARY.md 2>nul
del /q docs\ENHANCEMENT_COMPLETION_SUMMARY.txt 2>nul
del /q docs\FINAL_COMPLETION_REPORT.md 2>nul
del /q docs\FINAL_GAP_ANALYSIS.md 2>nul
del /q docs\IMPROVEMENT_SUMMARY.md 2>nul
del /q docs\PERMISSION_ENHANCEMENT.md 2>nul
del /q docs\PHASE2_ENHANCEMENT_README.md 2>nul
del /q docs\PHASE2_INDEX.md 2>nul
del /q docs\PHASE2_SUMMARY.md 2>nul
del /q docs\PROBLEM_DESCRIPTION_VERIFICATION.md 2>nul
del /q docs\README_检查结果.txt 2>nul
del /q docs\REQUIREMENTS_GAP_ANALYSIS.md 2>nul
del /q docs\SYMLINK_IMPLEMENTATION.md 2>nul
del /q docs\SYMLINK_USAGE.md 2>nul
del /q docs\VERIFICATION_CHECKLIST.md 2>nul
del /q docs\VERIFICATION_REPORT.md 2>nul
del /q scripts\build.bat 2>nul
del /q scripts\cleanup.bat 2>nul
del /q docs\test_compile.c 2>nul
del /q docs\test_compile.py 2>nul
del /q docs\test_passwd.txt 2>nul
del /q docs\test_permissions.c 2>nul
del /q docs\test_vfs.txt 2>nul
del /q bin\vfs_new.exe 2>nul
del /q docs\完成报告.txt 2>nul
del /q docs\课设完整性检查报告.txt 2>nul
del /q scripts\delete_files.ps1 2>nul
del /q docs\隐患修复总结.txt 2>nul

echo.
echo ✅ 清理完成！
echo.
echo 保留的核心文件：
dir /b docs\*.md docs\*.txt 2>nul | findstr /v ".vscode"
echo.
echo 源代码文件：
dir /b src\core\*.c src\commands\*.c src\api\*.c include\*.h Makefile scripts\compile.sh 2>nul
