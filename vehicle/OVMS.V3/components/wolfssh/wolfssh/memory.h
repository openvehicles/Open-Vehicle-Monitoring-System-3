/* memory.h
 *
 * Copyright (C) 2014-2016 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * The memory module contains the interface to the custom memory handling hooks.
 */


#pragma once

#include <wolfssh/settings.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void *(*wolfSSH_Malloc_cb)(size_t size);
typedef void (*wolfSSH_Free_cb)(void *ptr);
typedef void *(*wolfSSH_Realloc_cb)(void *ptr, size_t size);


/* Public set function */
WOLFSSH_API int wolfSSH_SetAllocators(wolfSSH_Malloc_cb  malloc_function,
                                      wolfSSH_Free_cb    free_function,
                                      wolfSSH_Realloc_cb realloc_function);

/* Public in case user app wants to use WMALLOC/WFREE */
WOLFSSH_API void* wolfSSH_Malloc(size_t size);
WOLFSSH_API void  wolfSSH_Free(void *ptr);
WOLFSSH_API void* wolfSSH_Realloc(void *ptr, size_t size);


#ifdef __cplusplus
}
#endif

