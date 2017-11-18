/* memory.c 
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


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/settings.h>


#ifdef USE_WOLFSSH_MEMORY

#include <wolfssh/memory.h>
#include <wolfssh/error.h>

/* Set these to default values initially. */
static wolfSSH_Malloc_cb  malloc_function  = NULL;
static wolfSSH_Free_cb    free_function    = NULL;
static wolfSSH_Realloc_cb realloc_function = NULL;


/* set wolfSSH allocator callbacks, 0 on success */
int wolfSSH_SetAllocators(wolfSSH_Malloc_cb  mf,
                           wolfSSH_Free_cb    ff,
                           wolfSSH_Realloc_cb rf)
{
    int res = 0;

    if (mf)
        malloc_function = mf;
	else
        res = WS_BAD_ARGUMENT;

    if (ff)
        free_function = ff;
    else
        res = WS_BAD_ARGUMENT;

    if (rf)
        realloc_function = rf;
    else
        res = WS_BAD_ARGUMENT;

    return res;
}


/* our malloc handler */
void* wolfSSH_Malloc(size_t size)
{
    void* res = 0;

    if (malloc_function)
        res = malloc_function(size);
    else
        res = malloc(size);

    return res;
}


/* our free handler */
void wolfSSH_Free(void *ptr)
{
    if (free_function)
        free_function(ptr);
    else
        free(ptr);
}


/* our realloc handler */
void* wolfSSH_Realloc(void *ptr, size_t size)
{
    void* res = 0;

    if (realloc_function)
        res = realloc_function(ptr, size);
    else
        res = realloc(ptr, size);

    return res;
}

#endif /* USE_WOLFSSH_MEMORY */

