#!/bin/bash
cd "d:\trae project\os"
gcc -Wall -Wextra -g -Iinclude -c src\\core\\main.c -o src\\core\\main.o
gcc -Wall -Wextra -g -Iinclude -c src\\core\\vfs.c -o src\\core\\vfs.o
gcc -Wall -Wextra -g -Iinclude -c src\\commands\\commands_system.c -o src\\commands\\commands_system.o
gcc -Wall -Wextra -g -Iinclude -c src\\commands\\commands_dir.c -o src\\commands\\commands_dir.o
gcc -Wall -Wextra -g -Iinclude -c src\\commands\\commands_file_io.c -o src\\commands\\commands_file_io.o
gcc -Wall -Wextra -g -Iinclude -c src\\commands\\commands_file_ops.c -o src\\commands\\commands_file_ops.o
gcc -Wall -Wextra -g -Iinclude -c src\\commands\\commands_user.c -o src\\commands\\commands_user.o
gcc -Wall -Wextra -g -o bin\\vfs.exe src\\core\\main.o src\\core\\vfs.o src\\commands\\commands_system.o src\\commands\\commands_dir.o src\\commands\\commands_file_io.o src\\commands\\commands_file_ops.o src\\commands\\commands_user.o
echo "Compilation complete"
