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

    // root (uid=0) 绕过所有权限检查
    if (current_user->u_uid == 0) return 0;

    uint16_t perm = mip->dino.di_mode;
    // 三级权限模型：
    //   位 8-10 (0o0700): 所有者权限
    //   位 5-7  (0o0070): 同组权限
    //   位 2-4  (0o0007): 其他用户权限
    //
    // S_IREAD  = 0x0100 (位 8, 所有者读)
    // S_IWRITE = 0x0080 (位 7, 所有者写)
    // S_IEXEC  = 0x0040 (位 6, 所有者执行)

    // 所有者检查
    if (current_user->u_uid == mip->dino.di_uid) {
        if ((mode & O_RDONLY) && !(perm & S_IREAD)) return -1;
        if ((mode & O_WRONLY) && !(perm & S_IWRITE)) return -1;
        if ((mode & O_RDWR) && !((perm & S_IREAD) && (perm & S_IWRITE))) return -1;
        return 0;
    }

    // 同组检查（权限位右移 3）
    if (current_user->u_gid == mip->dino.di_gid) {
        if ((mode & O_RDONLY) && !(perm & (S_IREAD >> 3))) return -1;
        if ((mode & O_WRONLY) && !(perm & (S_IWRITE >> 3))) return -1;
        if ((mode & O_RDWR) && !((perm & (S_IREAD >> 3)) && (perm & (S_IWRITE >> 3)))) return -1;
        return 0;
    }

    // 其他用户检查（权限位右移 6）
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
            mip->dino.di_addr[addr_idx] = balloc();
            mip->m_flag = 1;
        }
        return mip->dino.di_addr[addr_idx];
    }

    // ---- 一级间接 (8 ~ 8+127) ----
    if (blkno < 8 + indirect_blocks) {
        blkno -= 8;
        addr_idx = 8;

        if (mip->dino.di_addr[addr_idx] == 0) {
            mip->dino.di_addr[addr_idx] = balloc();
            mip->m_flag = 1;
            int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
            memset(indirect, 0, BLOCKSIZ);
        }

        int *indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        if (indirect[blkno] == 0) {
            indirect[blkno] = balloc();
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
        mip->dino.di_addr[addr_idx] = balloc();
        mip->m_flag = 1;
        int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
        memset(dbl_indirect, 0, BLOCKSIZ);
    }

    int *dbl_indirect = (int *)(virtual_disk + DATASTART + mip->dino.di_addr[addr_idx] * BLOCKSIZ);
    if (dbl_indirect[first_idx] == 0) {
        dbl_indirect[first_idx] = balloc();
        mip->m_flag = 1;
        int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
        memset(indirect, 0, BLOCKSIZ);
    }

    int *indirect = (int *)(virtual_disk + DATASTART + dbl_indirect[first_idx] * BLOCKSIZ);
    if (indirect[second_idx] == 0) {
        indirect[second_idx] = balloc();
        mip->m_flag = 1;
    }
    return indirect[second_idx];
}