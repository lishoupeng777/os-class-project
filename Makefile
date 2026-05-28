CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = vfs
SRCS = main.c vfs.c commands.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c vfs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) virtual_disk.bin

.PHONY: all clean