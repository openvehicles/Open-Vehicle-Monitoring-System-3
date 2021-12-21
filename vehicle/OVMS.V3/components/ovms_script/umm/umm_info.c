#ifdef UMM_INFO

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <math.h>

/* ----------------------------------------------------------------------------
 * One of the coolest things about this little library is that it's VERY
 * easy to get debug information about the memory heap by simply iterating
 * through all of the memory blocks.
 *
 * As you go through all the blocks, you can check to see if it's a free
 * block by looking at the high order bit of the next block index. You can
 * also see how big the block is by subtracting the next block index from
 * the current block number.
 *
 * The umm_info function does all of that and makes the results available
 * in the ummHeapInfo structure.
 * ----------------------------------------------------------------------------
 */

UMM_HEAP_INFO ummHeapInfo;

void compute_usage_metric(void)
{
    if (0 == ummHeapInfo.freeBlocks) {
        ummHeapInfo.usage_metric = -1;        // No free blocks!
    } else {
        ummHeapInfo.usage_metric = (int)((ummHeapInfo.usedBlocks * 100) / (ummHeapInfo.freeBlocks));
    }
}

void compute_fragmentation_metric(void)
{
    if (0 == ummHeapInfo.freeBlocks) {
        ummHeapInfo.fragmentation_metric = 0; // No free blocks ... so no fragmentation either!
    } else {
        ummHeapInfo.fragmentation_metric = 100 - (((uint32_t)(sqrtf(ummHeapInfo.freeBlocksSquared)) * 100) / (ummHeapInfo.freeBlocks));
    }
}

void *umm_info(void *ptr, bool force) {
    uint16_t blockNo = 0;

    UMM_CHECK_INITIALIZED();

    /* Protect the critical section... */
    UMM_CRITICAL_ENTRY();

    /*
     * Clear out all of the entries in the ummHeapInfo structure before doing
     * any calculations..
     */
    memset(&ummHeapInfo, 0, sizeof(ummHeapInfo));

    DBGLOG_FORCE(force, "\n");
    DBGLOG_FORCE(force, "+----------+-------+--------+--------+-------+--------+--------+\n");
    DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5i|NF %5i|PF %5i|\n",
        DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
        blockNo,
        UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
        UMM_PBLOCK(blockNo),
        (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) - blockNo,
        UMM_NFREE(blockNo),
        UMM_PFREE(blockNo));

    /*
     * Now loop through the block lists, and keep track of the number and size
     * of used and free blocks. The terminating condition is an nb pointer with
     * a value of zero...
     */

    blockNo = UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK;

    while (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) {
        size_t curBlocks = (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) - blockNo;

        ++ummHeapInfo.totalEntries;
        ummHeapInfo.totalBlocks += curBlocks;

        /* Is this a free block? */

        if (UMM_NBLOCK(blockNo) & UMM_FREELIST_MASK) {
            ++ummHeapInfo.freeEntries;
            ummHeapInfo.freeBlocks += curBlocks;
            ummHeapInfo.freeBlocksSquared += (curBlocks * curBlocks);

            if (ummHeapInfo.maxFreeContiguousBlocks < curBlocks) {
                ummHeapInfo.maxFreeContiguousBlocks = curBlocks;
            }

            DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5u|NF %5i|PF %5i|\n",
                DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
                blockNo,
                UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
                UMM_PBLOCK(blockNo),
                (uint16_t)curBlocks,
                UMM_NFREE(blockNo),
                UMM_PFREE(blockNo));

            /* Does this block address match the ptr we may be trying to free? */

            if (ptr == &UMM_BLOCK(blockNo)) {

                /* Release the critical section... */
                UMM_CRITICAL_EXIT();

                return ptr;
            }
        } else {
            ++ummHeapInfo.usedEntries;
            ummHeapInfo.usedBlocks += curBlocks;

            DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5u|                 |\n",
                DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
                blockNo,
                UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
                UMM_PBLOCK(blockNo),
                (uint16_t)curBlocks);
        }

        blockNo = UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK;
    }

    /*
     * The very last block is used as a placeholder to indicate that
     * there are no more blocks in the heap, so it cannot be used
     * for anything - at the same time, the size of this block must
     * ALWAYS be exactly 1 !
     */

    DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5i|NF %5i|PF %5i|\n",
        DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
        blockNo,
        UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
        UMM_PBLOCK(blockNo),
        UMM_NUMBLOCKS - blockNo,
        UMM_NFREE(blockNo),
        UMM_PFREE(blockNo));

    DBGLOG_FORCE(force, "+----------+-------+--------+--------+-------+--------+--------+\n");

    DBGLOG_FORCE(force, "Total Entries %5i    Used Entries %5i    Free Entries %5i\n",
        ummHeapInfo.totalEntries,
        ummHeapInfo.usedEntries,
        ummHeapInfo.freeEntries);

    DBGLOG_FORCE(force, "Total Blocks  %5i    Used Blocks  %5i    Free Blocks  %5i\n",
        ummHeapInfo.totalBlocks,
        ummHeapInfo.usedBlocks,
        ummHeapInfo.freeBlocks);

    DBGLOG_FORCE(force, "+--------------------------------------------------------------+\n");

    compute_usage_metric();
    DBGLOG_FORCE(force, "Usage Metric:               %5i\n", ummHeapInfo.usage_metric);

    compute_fragmentation_metric();
    DBGLOG_FORCE(force, "Fragmentation Metric:       %5i\n", ummHeapInfo.fragmentation_metric);

    DBGLOG_FORCE(force, "+--------------------------------------------------------------+\n");

    /* Release the critical section... */
    UMM_CRITICAL_EXIT();

    return NULL;
}

/* ------------------------------------------------------------------------ */

size_t umm_free_heap_size(void) {
    #ifndef UMM_INLINE_METRICS
    umm_info(NULL, false);
    #endif
    return (size_t)ummHeapInfo.freeBlocks * UMM_BLOCKSIZE;
}

size_t umm_max_free_block_size(void) {
    umm_info(NULL, false);
    return ummHeapInfo.maxFreeContiguousBlocks * sizeof(umm_block);
}

int umm_usage_metric(void) {
    #ifdef UMM_INLINE_METRICS
    compute_usage_metric();
    #else
    umm_info(NULL, false);
    #endif
    DBGLOG_DEBUG("usedBlocks %i totalBlocks %i\n", ummHeapInfo.usedBlocks, ummHeapInfo.totalBlocks);

    return ummHeapInfo.usage_metric;
}

int umm_fragmentation_metric(void) {
    #ifdef UMM_INLINE_METRICS
    compute_fragmentation_metric();
    #else
    umm_info(NULL, false);
    #endif
    DBGLOG_DEBUG("freeBlocks %i freeBlocksSquared %i\n", ummHeapInfo.freeBlocks, ummHeapInfo.freeBlocksSquared);

    return ummHeapInfo.fragmentation_metric;
}

#ifdef UMM_INLINE_METRICS
static void umm_fragmentation_metric_init(void) {
    ummHeapInfo.freeBlocks = UMM_NUMBLOCKS - 2;
    ummHeapInfo.freeBlocksSquared = ummHeapInfo.freeBlocks * ummHeapInfo.freeBlocks;
}

static void umm_fragmentation_metric_add(uint16_t c) {
    uint16_t blocks = (UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) - c;
    DBGLOG_DEBUG("Add block %i size %i to free metric\n", c, blocks);
    ummHeapInfo.freeBlocks += blocks;
    ummHeapInfo.freeBlocksSquared += (blocks * blocks);
}

static void umm_fragmentation_metric_remove(uint16_t c) {
    uint16_t blocks = (UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) - c;
    DBGLOG_DEBUG("Remove block %i size %i from free metric\n", c, blocks);
    ummHeapInfo.freeBlocks -= blocks;
    ummHeapInfo.freeBlocksSquared -= (blocks * blocks);
}
#endif // UMM_INLINE_METRICS

/* ------------------------------------------------------------------------ */
#endif
