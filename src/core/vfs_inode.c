#include "vfs.h"

// ============================================================
// Inode 分配与缓存管理
// 负责：ialloc() / ifree() / iget() / iput()
//       + 缓存健康检查
// ============================================================

// ----------------------------------------------------------
// 分配一个新的 Inode（从磁盘 inode 表）
// 返回值：inode 号（1-based），失败返回 -1
// ----------------------------------------------------------
int ialloc() {
    if (sb->ifree_num == 0) {
        printf("No more i-nodes available!\n");
        return -1;
    }

    int ino = sb->ifree[sb->ifree_ptr--];
    sb->ifree_num--;
    sb->s_modified = 1;

    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memset(dip, 0, DINODESIZ);
    dip->di_mode = IALLOC;

    return ino;
}

// ----------------------------------------------------------
// 释放一个 Inode（回收到空闲栈）
// ----------------------------------------------------------
void ifree(int ino) {
    if (ino < 1 || ino > DINODEBLK * BLOCKSIZ / DINODESIZ) return;

    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memset(dip, 0, DINODESIZ);
    dip->di_mode = IFREE;

    if (sb->ifree_ptr < NICINOD - 1) {
        sb->ifree[++sb->ifree_ptr] = ino;
        sb->ifree_num++;
        sb->s_modified = 1;
    }
}

// ----------------------------------------------------------
// 获取 Inode（从磁盘加载到内存 hash 缓存）
// 如果缓存中已有，则引用计数 +1 返回
// ----------------------------------------------------------
minode *iget(int ino) {
    if (ino < 1) return NULL;

    // 先在 hash 表中查找
    minode **hash_head = ihash(ino);
    minode *mip = *hash_head;
    while (mip != NULL) {
        if (mip->ino == ino) {
            mip->ref_count++;
            return mip;
        }
        mip = mip->next;
    }

    // 未命中，创建新的 minode 并从磁盘加载
    mip = (minode *)malloc(sizeof(minode));
    if (mip == NULL) return NULL;

    dinode *dip = (dinode *)(virtual_disk + DINODESTART + (ino - 1) * DINODESIZ);
    memcpy(&mip->dino, dip, DINODESIZ);
    mip->dev = 0;
    mip->ino = ino;
    mip->ref_count = 1;
    mip->m_flag = 0;

    // 插入 hash 表头部
    mip->next = *hash_head;
    mip->prev = NULL;
    if (*hash_head != NULL) {
        (*hash_head)->prev = mip;
    }
    *hash_head = mip;

    return mip;
}

// ----------------------------------------------------------
// 释放 Inode 引用（引用计数减 1，归零时写回磁盘并释放内存）
// ----------------------------------------------------------
void iput(minode *mip) {
    if (mip == NULL) return;

    // 隐患修复 #2: 引用计数泄漏检测
    if (mip->ref_count <= 0) {
        printf("[WARNING] iput() called on inode with ref_count=%d (possible leak)\n",
               mip->ref_count);
        return;
    }

    mip->ref_count--;
    if (mip->ref_count > 0) return;

    // 如果被修改过，写回磁盘
    if (mip->m_flag) {
        dinode *dip = (dinode *)(virtual_disk + DINODESTART + (mip->ino - 1) * DINODESIZ);
        memcpy(dip, &mip->dino, DINODESIZ);
        sb->s_modified = 1;
    }

    // 从 hash 表中移除
    minode **hash_head = ihash(mip->ino);
    if (mip->prev != NULL) {
        mip->prev->next = mip->next;
    } else {
        *hash_head = mip->next;
    }
    if (mip->next != NULL) {
        mip->next->prev = mip->prev;
    }

    free(mip);
}

// ----------------------------------------------------------
// 隐患修复 #2: 缓存健康检查（遍历所有 hash 桶，报告泄漏）
// ----------------------------------------------------------
void check_inode_cache_health() {
    int total_ref_count = 0;
    int leaked_count = 0;

    for (int i = 0; i < NHINO; i++) {
        minode *mip = hinode[i];
        while (mip != NULL) {
            total_ref_count += mip->ref_count;
            if (mip->ref_count > 5) {
                printf("[WARNING] Inode %d has high ref_count=%d (possible leak)\n",
                       mip->ino, mip->ref_count);
                leaked_count++;
            }
            mip = mip->next;
        }
    }

    if (total_ref_count > 0 || leaked_count > 0) {
        printf("[CACHE STATUS] Total ref_count=%d, Suspected leaks=%d\n",
               total_ref_count, leaked_count);
    }
}