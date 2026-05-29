CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

TARGET = bin\\vfs.exe

# 按功能拆分的核心模块
CORE_SRCS = \
    src\\core\\main.c \
    src\\core\\vfs_globals.c \
    src\\core\\vfs_disk.c \
    src\\core\\vfs_block.c \
    src\\core\\vfs_inode.c \
    src\\core\\vfs_path.c \
    src\\core\\vfs_perm.c

CMD_SRCS = \
    src\\commands\\commands_system.c \
    src\\commands\\commands_dir.c \
    src\\commands\\commands_file_io.c \
    src\\commands\\commands_file_ops.c \
    src\\commands\\commands_user.c

SRCS = $(CORE_SRCS) $(CMD_SRCS)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c include\\vfs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(subst /,\,$(OBJS)) $(TARGET) virtual_disk.bin 2>nul || echo Clean done

.PHONY: all clean