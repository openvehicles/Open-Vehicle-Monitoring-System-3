#ifndef __UMM_MALLOC_CFGPORT_H__
#define __UMM_MALLOC_CFGPORT_H__

// umm_malloc intrinsic data structure limitation (see https://github.com/rhempel/umm_malloc)
#define UMM_MAX_BLOCKS 32767

// Block size 32 bytes = 1 MB managable space
#define UMM_BLOCK_BODY_SIZE CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_HEAP_UMM_BLOCKSIZE

// Enable heap info
#define UMM_INFO

#endif // __UMM_MALLOC_CFGPORT_H__
