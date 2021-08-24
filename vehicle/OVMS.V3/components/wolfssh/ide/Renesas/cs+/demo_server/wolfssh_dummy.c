/* wolfssh_dummy.c
 *
 * Copyright (C) 2014-2020 wolfSSL Inc.
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
#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/error-crypt.h>
#include <stdio.h>

/* used for checking bytes on wire for window adjust packet read */
void ws_Ioctl(int fd, int flag, int *ret)
{
  /* This needs to implement when using scp or sftp */
}

#define YEAR 2019
#define APR  4

static int tick = 0;

time_t time(time_t *t)
{
    return ((YEAR-1970)*365+30*APR)*24*60*60 + tick++;
}

#include <ctype.h>
int strncasecmp(const char *s1, const char * s2, unsigned int sz)
{
    for( ; sz>0; sz--, s1++, s2++){
            if(toupper(*s1) < toupper(*s2)){
            return -1;
        }
        if(toupper(*s1) > toupper(*s2)){
            return 1;
        }
    }
    return 0;	
}