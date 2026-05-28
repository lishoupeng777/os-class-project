#!/bin/bash
cd "d:\trae project\os"
gcc -Wall -Wextra -g -c main.c -o main.o
gcc -Wall -Wextra -g -c vfs.c -o vfs.o
gcc -Wall -Wextra -g -c commands.c -o commands.o
gcc -Wall -Wextra -g -o vfs main.o vfs.o commands.o
echo "Compilation complete"
