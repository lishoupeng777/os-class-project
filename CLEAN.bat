@echo off
cd /d "d:\trae project\os"

REM 删除所有多余的文档和测试文件
del /q BEFORE_AFTER_COMPARISON.md 2>nul
del /q BUGS_AND_FIXES.md 2>nul
del /q CHECKLIST.md 2>nul
del /q CHOWN_COMPLETE.md 2>nul
del /q CHOWN_IMPLEMENTATION.md 2>nul
del /q CHOWN_SUMMARY.md 2>nul
del /q CURRENT_STATUS_SUMMARY.md 2>nul
del /q ENHANCEMENT_COMPLETION_SUMMARY.txt 2>nul
del /q FINAL_COMPLETION_REPORT.md 2>nul
del /q FINAL_GAP_ANALYSIS.md 2>nul
del /q IMPROVEMENT_SUMMARY.md 2>nul
del /q PERMISSION_ENHANCEMENT.md 2>nul
del /q PHASE2_ENHANCEMENT_README.md 2>nul
del /q PHASE2_INDEX.md 2>nul
del /q PHASE2_SUMMARY.md 2>nul
del /q PROBLEM_DESCRIPTION_VERIFICATION.md 2>nul
del /q README_检查结果.txt 2>nul
del /q REQUIREMENTS_GAP_ANALYSIS.md 2>nul
del /q SYMLINK_IMPLEMENTATION.md 2>nul
del /q SYMLINK_USAGE.md 2>nul
del /q VERIFICATION_CHECKLIST.md 2>nul
del /q VERIFICATION_REPORT.md 2>nul
del /q build.bat 2>nul
del /q cleanup.bat 2>nul
del /q test_compile.c 2>nul
del /q test_compile.py 2>nul
del /q test_passwd.txt 2>nul
del /q test_permissions.c 2>nul
del /q test_vfs.txt 2>nul
del /q vfs_new.exe 2>nul
del /q 完成报告.txt 2>nul
del /q 课设完整性检查报告.txt 2>nul
del /q delete_files.ps1 2>nul
del /q 隐患修复总结.txt 2>nul

echo.
echo ✅ 清理完成！
echo.
echo 保留的核心文件：
dir /b *.md *.txt 2>nul | findstr /v ".vscode"
echo.
echo 源代码文件：
dir /b *.c *.h Makefile compile.sh 2>nul
