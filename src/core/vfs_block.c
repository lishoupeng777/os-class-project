#include "vfs.h"

// ============================================================
// 磁盘块分配与释放
// 负责：balloc() / bfree()
// ============================================================

// ----------------------------------------------------------
// 分配一个空闲数据块
// 返回值：数据区相对块号（0 ~ FILEBLK-1），失败返回 -1
// ----------------------------------------------------------
int balloc() {
    if (sb->ffree_num == 0) {
        printf("Disk space exhausted!\n");
        return -1;
    }

    int blkno = sb->ffree[sb->ffree_ptr--];
    sb->ffree_num--;

    // 隐患修复 #4: 多级寻址越界检查
    if (blkno < 0 || blkno >= FILEBLK) {
        printf("[ERROR] balloc: invalid block number %d (expected range [0, %d))\n",
               blkno, FILEBLK);
        return -1;
    }

    // 当前空闲栈用完时，从下一个数据块加载新的空闲栈
    if (sb->ffree_ptr < 0 && sb->ffree_num > 0) {
        int next_block = blkno;
        int *p = (int *)(virtual_disk + DATASTART + next_block * BLOCKSIZ);
        sb->ffree_ptr = NICFREE - 1;
        sb->ffree_num = NICFREE;
        for (int i = 0; i < NICFREE; i++) {
            sb->ffree[i] = p[i];
        }
        blkno = sb->ffree[sb->ffree_ptr--];
        sb->ffree_num--;
    }

    sb->s_modified = 1;
    return blkno;
}

// ----------------------------------------------------------
// 释放一个数据块，回收到空闲栈
// ----------------------------------------------------------
void bfree(int blkno) {
    if (blkno < 0 || blkno >= FILEBLK) return;

    // 当前栈满时，将栈内容写入该块，然后重置栈
    if (sb->ffree_ptr >= NICFREE - 1) {
        int *p = (int *)(virtual_disk + DATASTART + blkno * BLOCKSIZ);
        for (int i = 0; i <= sb->ffree_ptr; i++) {
            p[i] = sb->ffree[i];
        }
        p[sb->ffree_ptr + 1] = -1;
        sb->ffree[0] = blkno;
        sb->ffree_ptr = 0;
    } else {
        sb->ffree[++sb->ffree_ptr] = blkno;
    }
    sb->ffree_num++;
    sb->s_modified = 1;
}