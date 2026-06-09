#include "vfs.h"

// ============================================================
// 文件权限检查 与 数据块寻址分配
// 负责：access() / alloc_block()
// ============================================================

// ----------------------------------------------------------
// 权限检查
// 检查当前用户是否可以对指定 inode 进行 mode 操作
// mode: O_RDONLY / O_WRONLY / O_RDWR
// 返回值: 0=允许, -1=拒绝
// ----------------------------------------------------------
int access(minode *mip, int mode) {
    if (current_user == NULL) return -1;

    uint16_t perm = mip->dino.di_mode;
    // 三级权限模型：
    //   位 8-10 (0o0700): 所有者权限
    //   位 5-7  (0o0070): 同组权限
    //   位 2-4  (0o0007): 其他用户权限

    // 所有者检查
    if (current_user->u_uid == mip->dino.di_uid) {
        if ((mode & O_RDONLY) && !(perm & S_IREAD)) return -1;
        if ((mode & O_WRONLY) && !(perm & S_IWRITE)) return -1;
        if ((mode & O_RDWR) && !((perm & S_IREAD) && (perm & S_IWRITE))) return -1;
        return 0;
    }

    // 同组检查
    if (current_user->u_gid == mip->dino.di_gid) {
        if ((mode & O_RDONLY) && !(perm & (S_IREAD >> 3))) return -1;
        if ((mode & O_WRONLY) && !(perm & (S_IWRITE >> 3))) return -1;
        if ((mode & O_RDWR) && !((perm & (S_IREAD >> 3)) && (perm & (S_IWRITE >> 3)))) return -1;
        return 0;
    }

    // 其他用户检查
    if ((mode & O_RDONLY) && !(perm & (S_IREAD >> 6))) return -1;
    if ((mode & O_WRONLY) && !(perm & (S_IWRITE >> 6))) return -1;
    if ((mode & O_RDWR) && !((perm & (S_IREAD >> 6)) && (perm & (S_IWRITE >> 6)))) return -1;

    return 0;
}

// ----------------------------------------------------------
// 数据块寻址与分配
// 根据文件内偏移 offset 找到或分配对应的磁盘块号
// 支持：直接块(0-7)、一级间接(8)、二级间接(9)
// 返回值：块号，失败返回 -1
// ----------------------------------------------------------
int alloc_block(minode *mip, int offset) {
    int blkno = offset / BLOCKSIZ;
    int addr_idx;
    int indirect_blocks = (int)(BLOCKSIZ / sizeof(int));  // 128 个块指针

    // ---- 直接块 (0-7) ----
    if (blkno < 8) {
        addr_idx = blkno;
        if (mip->dino.di_addr[addr_idx] == 0) {
            int new_blk = balloc();
            if (new_blk < 0) {
                return -1;
            }
            mip->dino.di_addr[addr_idx] = new_blk;
            mip->m_flag = 1;
        }
        return mip->dino.di_addr[addr_idx];
    }

    // ---- 一级间接 (8 ~ 8+127) ----
    if (blkno < 8 + indirect_blocks) {
        blkno -= 8;
        addr_idx = 8;

        if (mip->dino.di_addr[addr_idx] == 0) {
            int indirect_blk = balloc();
            if (indirect_blk < 0) {
                return -1;
            }
            mip->dino.di_addr[addr_idx] = indirect_blk;
            mip->m_flag = 1;
            int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
            memset(indirect, 0, BLOCKSIZ);
        }

        int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        if (indirect[blkno] == 0) {
            int new_blk = balloc();
            if (new_blk < 0) {
                return -1;
            }
            indirect[blkno] = new_blk;
            mip->m_flag = 1;
        }
        return indirect[blkno];
    }

    // ---- 二级间接 (8+128 ~ 8+128+128*128) ----
    blkno -= 8 + indirect_blocks;
    int first_idx  = blkno / indirect_blocks;
    int second_idx = blkno % indirect_blocks;
    addr_idx = 9;

    if (mip->dino.di_addr[addr_idx] == 0) {
        int dbl_blk = balloc();
        if (dbl_blk < 0) {
            return -1;
        }
        mip->dino.di_addr[addr_idx] = dbl_blk;
        mip->m_flag = 1;
        int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        memset(dbl_indirect, 0, BLOCKSIZ);
    }

    int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
    if (dbl_indirect[first_idx] == 0) {
        int indirect_blk = balloc();
        if (indirect_blk < 0) {
            return -1;
        }
        dbl_indirect[first_idx] = indirect_blk;
        mip->m_flag = 1;
        int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
        memset(indirect, 0, BLOCKSIZ);
    }

    int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
    if (indirect[second_idx] == 0) {
        int new_blk = balloc();
        if (new_blk < 0) {
            return -1;
        }
        indirect[second_idx] = new_blk;
        mip->m_flag = 1;
    }
    return indirect[second_idx];
}