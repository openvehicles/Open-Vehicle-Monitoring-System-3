/* port.h
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


/*
 * The port module wraps standard C library functions with macros to
 * cover portablility issues when building in environments that rename
 * those functions. This module also provides local versions of some
 * standard C library functions that are missing on some platforms.
 */


#pragma once

#include <wolfssh/settings.h>
#include <wolfssh/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PORT_DYNTYPE_STRING 500
/* This value needs to stay in sync with the actual value of DYNTYPE_STRING
 * from internal.h. */

/* setup memory handling */
#ifndef WMALLOC_USER
    #ifdef WOLFSSL_USER_SETTINGS
    #include <wolfssl/wolfcrypt/settings.h>
    #else
    #include <wolfssl/options.h>
    #endif
    #include <wolfssl/wolfcrypt/types.h>
    #define WMALLOC(s, h, t)      XMALLOC(s, h, t)
    #define WFREE(p, h, t)        XFREE(p, h, t)
    #define WREALLOC(p, n, h, t)  XREALLOC(p, n, h, t)
    #define wolfSSH_SetAllocators wolfSSL_SetAllocators
#endif /* WMALLOC_USER */

#ifndef WFGETS
    #define WFGETS fgets
#endif
#ifndef WFPUTS
    #define WFPUTS fputs
#endif

#ifndef WOLFSSH_HANDLE
    /* handle for console to use during Linux console code translations */
    #ifdef USE_WINDOWS_API
        #define WOLFSSH_HANDLE HANDLE
    #else
        #define WOLFSSH_HANDLE int
    #endif
#endif /* !WOLFSSH_HANDLE */

#ifdef NO_FILESYSTEM
    #define WS_DELIM '/'
#elif defined(WOLFSSL_NUCLEUS)
    #include "storage/nu_storage.h"

    #define WFILE int
    WOLFSSH_API int wfopen(WFILE**, const char*, const char*);

    #define WFOPEN(f,fn,m)    wfopen((f),(fn),(m))
    #define WFCLOSE(f)        NU_Close(*(f))
    #define WFWRITE(b,x,s,f)  ((s) != 0)? NU_Write(*(f),(const CHAR*)(b),(s)): 0
    #define WFREAD(b,x,s,f)   NU_Read(*(f),(CHAR*)(b),(s))
    #define WFSEEK(s,o,w)     NU_Seek(*(s),(o),(w))
    #define WFTELL(s)         NU_Seek(*(s), 0, PSEEK_CUR)
    #define WREWIND(s)        NU_Seek(*(s), 0, PSEEK_SET)
    #define WSEEK_END         PSEEK_END

    #define WS_DELIM '\\'
    #define WOLFSSH_O_RDWR   PO_RDWR
    #define WOLFSSH_O_RDONLY PO_RDONLY
    #define WOLFSSH_O_WRONLY PO_WRONLY
    #define WOLFSSH_O_APPEND PO_APPEND
    #define WOLFSSH_O_CREAT  PO_CREAT
    #define WOLFSSH_O_TRUNC  PO_TRUNC
    #define WOLFSSH_O_EXCL   PO_EXCL

#ifndef WOPEN
    static inline int wOpen(const char* f, short flag, short mode)
    {
        /* @TODO could use PS_IWRITE only or PS_IREAD only? */
        return NU_Open(f, PO_TEXT | flag, (PS_IWRITE | PS_IREAD));
    }

    #define WOPEN(f,m,p) wOpen((f),(m),(p))
#endif

    #define WCLOSE(fd) NU_Close((fd))

    static inline int wChmod(const char* f, int mode) {
        unsigned char atr = 0;

        if (f == NULL) {
            return -1;
        }

        if (NU_Get_Attributes(&atr, f) != NU_SUCCESS) {
            return -1;
        }

        /* Set attribute value */
        atr = atr & 0xF0; /* clear first byte */
        if (mode == 0x124) {
            atr |= ARDONLY;   /* set read only value */
        }
        else {
            /* if not setting read only set to normal */
            atr |= ANORMAL;
        }

        if (NU_Set_Attributes(f, atr) != NU_SUCCESS) {
            return -1;
        }
        else {
            return 0;
        }
    }
    #define WCHMOD(fs,f,m) wChmod((f),(m))

#elif defined(FREESCALE_MQX)
    /* Freescale MQX 4.2 (Classic) */
    #include "mfs.h"
    #include "mqx.h"
    #include "fio.h"
    #include "rtcs.h"

    #define WFILE         MQX_FILE
    #define WOLFSSH_MAX_FILENAME PATHNAME_SIZE

    /* default to "a:" drive, user can override */
    #ifndef MQX_MFS_DEVICE
        #define MQX_MFS_DEVICE  "a:"
    #endif

    WOLFSSH_API int wfopen(WFILE**, const char*, const char*);

    #define WFOPEN(f,fn,m)      wfopen((f),(fn),(m))
    #define WFCLOSE(f)          fclose((f))
    #define WFREAD(b,s,a,f)     fread((b),(s),(a),(f))
    #define WFWRITE(b,x,s,f)    fwrite((b),(x),(s),(f))
    #define WFSEEK(s,o,w)       fseek((s),(o),(w))
    #define WFTELL(s)           ftell((s))
    #define WREWIND(s)          fseek((s), 0, IO_SEEK_SET)
    #define WSEEK_END           IO_SEEK_END

    static inline int wChmod(void* fs, const char* f, int mode)
    {
        int err;
        MQX_FILE_PTR mfs_ptr;
        MFS_FILE_ATTR_PARAM attr;
        unsigned char attribute = 0;

        if (fs == NULL || f == NULL) {
            return -1;
        }
        mfs_ptr = (MQX_FILE_PTR)fs;

        attr.ATTRIBUTE_PTR = &attribute;
        attr.PATHNAME = (char*)f;

        /* check that filesystem handle is valid */
        if (_io_is_fs_valid(mfs_ptr) == 0) {
            WLOG(WS_LOG_SFTP, "Invalid file system pointer");
            return -1;
        }

        /* get file attributes */
        err = ioctl(mfs_ptr, IO_IOCTL_GET_FILE_ATTR, (uint32_t*)&attr);
        if (err != MFS_NO_ERROR) {
            WLOG(WS_LOG_SFTP, "Unable to get file attributes");
            return -1;
        }

        /* set file attributes */
        if (mode == 0x124) {
            /* set read only value */
            attribute |= MFS_ATTR_READ_ONLY;
            err = ioctl(mfs_ptr, IO_IOCTL_SET_FILE_ATTR, (uint32_t*)&attr);
            if (err != MFS_NO_ERROR) {
                WLOG(WS_LOG_SFTP, "Unable to set file attributes");
                return -1;
            }
        }

        return 0;
    }
    #define WCHMOD(fs,f,m)         wChmod((fs),(f),(m))

#else
    #include <stdlib.h>
    #if !defined(_WIN32_WCE) && !defined(FREESCALE_MQX)
        #include <stdio.h>
    #endif
    #define WFILE FILE
    WOLFSSH_API int wfopen(WFILE**, const char*, const char*);

    #define WFOPEN(f,fn,m)    wfopen((f),(fn),(m))
    #define WFCLOSE(f)        fclose(f)
    #define WFREAD(b,s,a,f)   fread((b),(s),(a),(f))
    #define WFWRITE(b,s,a,f)  fwrite((b),(s),(a),(f))
    #define WFSEEK(s,o,w)     fseek((s),(o),(w))
    #define WFTELL(s)         ftell((s))
    #define WREWIND(s)        rewind((s))
    #define WSEEK_END         SEEK_END
    #define WBADFILE          NULL
    #ifdef WOLFSSL_VXWORKS
        #define WUTIMES(f,t)      (WS_SUCCESS)
    #else
        #define WUTIMES(f,t)      utimes((f),(t))
    #endif

    #ifndef USE_WINDOWS_API
        #define WCHMOD(fs,f,m)   chmod((f),(m))
    #else
        #define WCHMOD(fs,f,m)   _chmod((f),(m))
    #endif

    #if (defined(WOLFSSH_SCP) || defined(WOLFSSH_SFTP)) && \
        !defined(WOLFSSH_SCP_USER_CALLBACKS)

        #ifdef USE_WINDOWS_API
            WOLFSSH_LOCAL void* WS_CreateFileA(const char* fileName,
                    unsigned long desiredAccess, unsigned long shareMode,
                    unsigned long creationDisposition, unsigned long flags,
                    void* heap);
            WOLFSSH_LOCAL void* WS_FindFirstFileA(const char* fileName,
                    char* realFileName, size_t realFileNameSz, int* isDir,
                    void* heap);
            WOLFSSH_LOCAL int WS_FindNextFileA(void* findHandle,
                    char* realFileName, size_t realFileNameSz);
            WOLFSSH_LOCAL int WS_GetFileAttributesExA(const char* fileName,
                    void* fileInfo, void* heap);
            WOLFSSH_LOCAL int WS_RemoveDirectoryA(const char* dirName,
                    void* heap);
            WOLFSSH_LOCAL int WS_CreateDirectoryA(const char* dirName,
                    void* heap);
            WOLFSSH_LOCAL int WS_MoveFileA(const char* oldName,
                    const char* newName, void* heap);
            WOLFSSH_LOCAL int WS_DeleteFileA(const char* fileName,
                    void* heap);

            #ifndef _WIN32_WCE
                #include <direct.h>
                #define WCHDIR(p)      _chdir((p))
                #define WMKDIR(fs,p,m) _mkdir((p))
            #endif
        #else
            #include <unistd.h>
            #include <sys/stat.h>
            #define WCHDIR(p)     chdir((p))
            #ifdef WOLFSSL_VXWORKS
                #define WMKDIR(fs,p,m) mkdir((p))
            #else
                #define WMKDIR(fs,p,m) mkdir((p),(m))
            #endif
        #endif
    #endif
#endif

/* setup string handling */
#ifndef WSTRING_USER
    #ifdef WOLFSSL_VXWORKS
        #include <strings.h>
    #else
        #include <string.h>
    #endif

    WOLFSSH_API char* wstrnstr(const char*, const char*, unsigned int);
    WOLFSSH_API char* wstrncat(char*, const char*, size_t);

    #define WMEMCPY(d,s,l)    memcpy((d),(s),(l))
    #define WMEMSET(b,c,l)    memset((b),(c),(l))
    #define WMEMCMP(s1,s2,n)  memcmp((s1),(s2),(n))
    #define WMEMMOVE(d,s,l)   memmove((d),(s),(l))

    #define WSTRLEN(s1)       strlen((s1))
    #define WSTRSTR(s1,s2)    strstr((s1),(s2))
    #define WSTRNSTR(s1,s2,n) wstrnstr((s1),(s2),(n))
    #define WSTRNCMP(s1,s2,n) strncmp((s1),(s2),(n))
    #define WSTRNCAT(s1,s2,n) wstrncat((s1),(s2),(n))
    #define WSTRSPN(s1,s2)    strspn((s1),(s2))
    #define WSTRCSPN(s1,s2)   strcspn((s1),(s2))

    #ifdef USE_WINDOWS_API
        #define WSTRNCPY(s1,s2,n) strncpy_s((s1),(n),(s2),(n))
        #define WSTRNCASECMP(s1,s2,n) _strnicmp((s1),(s2),(n))
        #define WSNPRINTF(s,n,f,...) _snprintf_s((s),(n),(n),(f),##__VA_ARGS__)
        #define WVSNPRINTF(s,n,f,...) _vsnprintf_s((s),(n),(n),(f),##__VA_ARGS__)
        #define WSTRTOK(s1,s2,s3) strtok_s((s1),(s2),(s3))
        #define WSTRDUP(s,h) _strdup((s))
    #elif defined(MICROCHIP_MPLAB_HARMONY) || defined(MICROCHIP_PIC32)
        #include <stdio.h>
        #define WSTRNCPY(s1,s2,n) strncpy((s1),(s2),(n))
        #define WSTRNCASECMP(s1, s2, n) strncmp((s1), (s2), (n))
        #define WSNPRINTF(s,n,f,...) snprintf((s),(n),(f),##__VA_ARGS__)
        #define WVSNPRINTF(s,n,f,...) vsnprintf((s),(n),(f),##__VA_ARGS__)
        #define WSTRTOK(s1,s2,s3) strtok_r((s1),(s2),(s3))
        #define WSTRDUP(s,h) strdup((s))
    #elif defined(RENESAS_CSPLUS)
        #include <stdio.h>
        #define WSTRNCPY(s1,s2,n) strncpy((s1),(s2),(n))
        #define WSTRNCASECMP(s1,s2,n) strncasecmp((s1),(s2),(n))
        #define WSNPRINTF(s,n,f,...) snprintf((s),(n),(f),__VA_ARGS__)
        #define WVSNPRINTF(s,n,f,...) vsnprintf((s),(n),(f),__VA_ARGS__)
        #define WSTRTOK(s1,s2,s3) strtok_r((s1),(s2),(s3))
        #define WSTRDUP(s,h) strdup((s))
    #else
        #ifndef FREESCALE_MQX
            #include <stdio.h>
        #endif
        #define WSTRNCPY(s1,s2,n) strncpy((s1),(s2),(n))
        #define WSTRNCASECMP(s1,s2,n) strncasecmp((s1),(s2),(n))
        #define WSNPRINTF(s,n,f,...) snprintf((s),(n),(f),##__VA_ARGS__)
        #define WVSNPRINTF(s,n,f,...) vsnprintf((s),(n),(f),##__VA_ARGS__)
        #define WSTRTOK(s1,s2,s3) strtok_r((s1),(s2),(s3))
        #define WSTRDUP(s,h) strdup((s))
    #endif
#endif /* WSTRING_USER */

/* get local time for debug print out */
#ifdef USE_WINDOWS_API
    #define WTIME time
    #define WLOCALTIME(c,r) (localtime_s((r),(c))==0)
#elif defined(MICROCHIP_MPLAB_HARMONY)
    #define WTIME XTIME
    #define WLOCALTIME(c,r) (localtime((c)) != NULL && \
                             WMEMCPY((r), localtime((c)), sizeof(struct tm)))
#elif defined(FREESCALE_MQX)
    static inline time_t mqx_time(time_t* timer)
    {
        time_t localTime;
        TIME_STRUCT time_s;

        if (timer == NULL)
            timer = &localTime;

        _time_get(&time_s);
        *timer = (time_t)time_s.SECONDS;

        return *timer;
    }
    #define WTIME(t1)        mqx_time((t1))
    #define WLOCALTIME(c,r) (localtime_r((c),(r))!=NULL)
    #define WOLFSSH_NO_TIMESTAMP /* no strftime() */
#else
    #define WTIME time
    #define WLOCALTIME(c,r) (localtime_r((c),(r))!=NULL)
#endif

#if (defined(WOLFSSH_SFTP) || defined(WOLFSSH_SCP)) && \
        !defined(NO_WOLFSSH_SERVER) && !defined(NO_FILESYSTEM)

    #ifndef SIZEOF_OFF_T
        /* if not using autoconf then make a guess on off_t size based on sizeof
         * long long */
        #if defined(SIZEOF_LONG) && SIZEOF_LONG == 8
            #define SIZEOF_OFF_T 8
        #else
            #define SIZEOF_OFF_T 4
        #endif
    #endif

#ifdef WOLFSSL_NUCLEUS
    #define WSTAT_T         struct stat
    #define WRMDIR(fs,d)   (NU_Remove_Dir((d)) == NU_SUCCESS)?0:1
    #define WMKDIR(fs,d,m) (NU_Make_Dir((d)) == NU_SUCCESS)?0:1
    #define WSTAT(p,b)     NU_Get_First((b),(p))
    #define WLSTAT(p,b)    NU_Get_First((b),(p))
    #define WREMOVE(fs,d)  NU_Delete((d))

#ifndef WS_MAX_RENAME_BUF
#define WS_MAX_RENAME_BUF 256
#endif

    static inline int wRename(char* o, char* n)
    {
        int ret;

        if (o == NULL || n == NULL) {
            return NUF_BADPARM;
        }

        ret = NU_Rename(o, n);

        /* try to handle case of from one drive to another */
        if (ret == NUF_BADPARM && o[0] != n[0]) {
            WFILE* fOld;
            WFILE* fNew;
            unsigned char buf[WS_MAX_RENAME_BUF];

            if ((ret = WFOPEN(&fOld, o, "rb")) != 0) {
                return ret;
            }

            if ((ret = WFOPEN(&fNew, n, "rwb")) != 0) {
                WFCLOSE(fOld);
                return ret;
            }

            /* read from the file in chunks and write chunks to new file */
            do {
                ret = WFREAD(buf, 1, WS_MAX_RENAME_BUF, fOld);
                if (ret > 0) {
                    if ((WFWRITE(buf, 1, ret, fNew)) != ret) {
                        WFCLOSE(fOld);
                        WFCLOSE(fNew);
                        WREMOVE(NULL, n);
                        return NUF_BADPARM;
                    }
                }
            } while (ret > 0);

            if (WFTELL(fOld) == WFSEEK(fOld, 0, WSEEK_END)) {
                /* wrote everything from file */
                WFCLOSE(fOld);
                WREMOVE(NULL, o);
                WFCLOSE(fNew);
            }
            else {
                /* unable to write everything to file */
                WFCLOSE(fNew);
                WREMOVE(NULL, n);
                WFCLOSE(fOld);
                return NUF_BADPARM;
            }

            return 0;
        }

        /* not special case so just return value from NU_Rename */
        return ret;
    }

    #define WRENAME(fs,o,n) wRename((o),(n))
    #define WFD int

#ifndef WGETCWD
    static inline char* wGetCwd(char* buf, unsigned int bufSz)
    {
        int ret;
        MNT_LIST_S* list = NU_NULL;

        if (buf == NULL || bufSz < 3) {
            return NULL;
        }

        ret = NU_List_Mount(&list);
        if (ret != NU_SUCCESS) {
            return NULL;
        }

        buf[0] = list->mnt_name[0];
        buf[1] = ':';
        buf[2] = '\0';

        return buf;
    }
    #define WGETCWD(fs,r,rSz) wGetCwd((r),(rSz))
#endif

#ifndef WPWRITE
    static inline int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
            const unsigned int* shortOffset)
    {
        INT32 ofst = shortOffset[0];
        if (ofst > 0) {
            NU_Seek(fd, ofst, 0);
        }

        return NU_Write(fd, (const CHAR*)buf, sz);
    }
    #define WPWRITE(fd,b,s,o) wPwrite((fd),(b),(s),(o))
#endif

#ifndef WPREAD
    static inline int wPread(WFD fd, unsigned char* buf, unsigned int sz,
            const unsigned int* shortOffset)
    {
        INT32 ofst = shortOffset[0];
        if (ofst > 0) {
            NU_Seek(fd, ofst, 0);
        }

        return NU_Read(fd, (CHAR*)buf, sz);
    }
    #define WPREAD(fd,b,s,o)  wPread((fd),(b),(s),(o))
#endif

    static inline int wUtimes(const char* f, struct timeval t[2])
    {
        DSTAT stat;
        int ret = -1;

        if (NU_Get_First(&stat, f) == NU_SUCCESS) {
             ret = NU_Utime(&stat, 0xFFFF, t[0].tv_sec, 0xFFFF, t[1].tv_sec,
                0xFFFF, 0xFFFF);
             NU_Done(&stat);
             if (ret == NU_SUCCESS) {
                 ret = 0;
             }
             else {
                 ret = -1;
             }
        }
        return ret;
    }
    #define WUTIMES(f,t) wUtimes((f), (t))

    #ifndef NO_WOLFSSH_DIR
    #define WDIR DSTAT

#ifndef WOPENDIR
    static inline int wOpenDir(WDIR* d, const char* dir)
    {
        int ret;
        int idx = WSTRLEN(dir);
        char tmp[256]; /* default max file name size */

        /* handle special case at "/" to list all drives */
        if (idx < 3 && dir[0] == WS_DELIM) {
            d->fsize = 0; /* used to count number of drives */
            return NU_SUCCESS;
        }

        if (idx < 3) {
            return -1;
        }

        memcpy(tmp, dir, idx);
        if (tmp[idx - 1] == '.') {
            tmp[idx - 1] = '*';
        }
        else {
            /* if opening a directory then make sure pattern '/' '*' is used */
            unsigned char atrib = 0;
            if (NU_Get_Attributes(&atrib, dir) == NU_SUCCESS) {
                if (atrib & ADIRENT) {
                    if (tmp[idx-1] != WS_DELIM) {
                        if (idx + 2 > sizeof(tmp)) {
                            /* not enough space */
                            return -1;
                        }
                        tmp[idx++] = WS_DELIM;
                        tmp[idx++] = '*';
                    }
                }
            }
        }

        if (tmp[idx - 1] == WS_DELIM) {
            if (idx + 1 > sizeof(tmp)) {
                /* not enough space */
                return -1;
            }
            tmp[idx++] = '*';
        }
        tmp[idx] = '\0';
        ret = NU_Get_First(d, tmp);

        /* if back to root directory i.e. A:/ then handle case
         * where file system has nothing in it. */
        if (dir[idx - 3] == ':' && ret == NUF_NOFILE) {
             memset(d, 0, sizeof(WDIR));
             ret = NU_SUCCESS;
        }

        if (ret == NU_SUCCESS) {
            return 0;
        }

        return -1;
    }
    #define WOPENDIR(fs,h,c,d)  wOpenDir((c),(d))
#endif

    #define WCLOSEDIR(d) NU_Done((d))
    #define WREADDIR(d)  (NU_Get_Next((d)) == NU_SUCCESS)?(d):NULL
    #endif /* NO_WOLFSSH_DIR */

#elif defined(FREESCALE_MQX)
    /* Freescale MQX 4.2 (Classic)
     * TODO for SCP support:
     * - implement WUTIMES / wUtimes
     * - implement WREADDIR() */

    #define WFD         MQX_FILE_PTR
    #define WS_DELIM    '/'

    #ifndef MQX_DEFAULT_SFTP_NAME_SZ
        #define MQX_DEFAULT_SFTP_NAME_SZ 256
    #endif

    /* flags can be accessed through MQX_FILE_PTR->FLAGS & IO_O_RDONLY */
    /* these are from io.h */
    #define WOLFSSH_O_RDWR      IO_O_RDWR       /* not used? */
    #define WOLFSSH_O_RDONLY    IO_O_RDONLY
    #define WOLFSSH_O_WRONLY    IO_O_WRONLY     /* not used? */
    #define WOLFSSH_O_APPEND    IO_O_APPEND     /* not used? */
    #define WOLFSSH_O_CREAT     IO_O_CREAT      /* not used? */
    #define WOLFSSH_O_TRUNC     IO_O_TRUNC      /* not used? */
    #define WOLFSSH_O_EXCL      IO_O_EXCL       /* not used? */

    #ifndef WOPEN
        static inline WFD wOpen(const char* f, int flag, unsigned int per)
        {
            char* mode;

            /* translate open() flags to fopen() mode */
            if (flag & WOLFSSH_O_RDONLY) {
                mode = "r";

            } else if (flag & WOLFSSH_O_RDWR) {
                if ((flag & WOLFSSH_O_CREAT) &&
                    (flag & WOLFSSH_O_TRUNC)) {
                  mode = "w+";

                } else if ((flag & WOLFSSH_O_CREAT) &&
                           (flag & WOLFSSH_O_APPEND)) {
                  mode = "a+";

                } else {
                  mode = "r+";
                }
            } else if (flag & WOLFSSH_O_WRONLY) {
                if ((flag & WOLFSSH_O_CREAT) &&
                    (flag & WOLFSSH_O_TRUNC)) {
                  mode = "w";
                } else if ((flag & WOLFSSH_O_CREAT) &&
                           (flag & WOLFSSH_O_APPEND)) {
                  mode = "a";
                }
            } else {
                return NULL;
            }

            return fopen(f, mode);
        }

        #define WOPEN(f,m,p)    wOpen((f),(m),(p))
    #endif

    #ifndef WRMDIR
        static inline int wRmDir(void* fs, const char* path)
        {
            MQX_FILE_PTR mfs_ptr;
            uint32_t err;

            if (fs == NULL || path == NULL) {
                return -1;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return -1;
            }

            /* try to delete subdirectory */
            err = ioctl(mfs_ptr, IO_IOCTL_REMOVE_SUBDIR, (uint32_t*)path);
            if (err != MFS_NO_ERROR) {
                return -1;
            }

            return 0;
        }

        #define WRMDIR(fs,d)  wRmDir((fs),(d))
    #endif /* WRMDIR */

    #ifndef WMKDIR

        static inline int wMkDir(void* fs, const char* path)
        {
            MQX_FILE_PTR mfs_ptr;
            uint32_t err;

            if (fs == NULL || path == NULL) {
                return -1;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return -1;
            }

            /* try to create subdirectory */
            err = ioctl(mfs_ptr, IO_IOCTL_CREATE_SUBDIR, (uint32_t*)path);
            if (err != MFS_NO_ERROR) {
                return -1;
            }

            return 0;
        }

        #define WMKDIR(fs,d,m)     wMkDir((fs),(d))
    #endif /* WMKDIR */

    #ifndef WREMOVE
        static inline int wRemove(void* fs, const char* path)
        {
            MQX_FILE_PTR mfs_ptr;
            uint32_t err;

            if (fs == NULL || path == NULL) {
                return -1;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return -1;
            }

            /* try to create subdirectory */
            err = ioctl(mfs_ptr, IO_IOCTL_DELETE_FILE, (uint32_t*)path);
            if (err != MFS_NO_ERROR) {
                return -1;
            }

            return 0;
        }
        #define WREMOVE(fs,d)      wRemove((fs),(d))
    #endif /* WREMOVE */

    #ifndef WRENAME
        static inline int wRename(void* fs, const char* oldpath,
                                  const char* newpath)
        {
            int err;
            MQX_FILE_PTR mfs_ptr;
            MFS_RENAME_PARAM rename;

            if (fs == NULL || oldpath == NULL || newpath == NULL) {
                return -1;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return -1;
            }

            rename.OLD_PATHNAME = (char*)oldpath;
            rename.NEW_PATHNAME = (char*)newpath;

            /* try to create subdirectory */
            err = ioctl(mfs_ptr, IO_IOCTL_RENAME_FILE, (uint32_t*)&rename);
            if (err != MFS_NO_ERROR) {
                return -1;
            }

            return 0;
        }
        #define WRENAME(fs,o,n)    wRename((fs),(o),(n))
    #endif /* WRENAME */

    #ifndef WGETCWD
        static inline char* wGetCwd(void* fs, char* buf, unsigned int bufSz)
        {
            MQX_FILE_PTR mfs_ptr;
            int err, dirSz, labelSz;

            if (buf == NULL || bufSz == 0) {
                return NULL;
            }

            if (fs == NULL) {
                WLOG(WS_LOG_SFTP, "Must set MFS file system handle with "
                     "wolfSSH_SetFilesystemHandle()");
                return NULL;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return NULL;
            }

            /* check bufSz against MFS path size */
            if (bufSz < PATHNAME_SIZE) {
                WLOG(WS_LOG_SFTP, "Buf size smaller than max MFS path size");
                return NULL;
            }

            /* try to get current directory */
            err = ioctl(mfs_ptr, IO_IOCTL_GET_CURRENT_DIR, (uint32_t*)buf);
            if (err != MFS_NO_ERROR) {
                return NULL;
            }

            /* add drive to beginning of path */
            dirSz = WSTRLEN(buf);
            labelSz = WSTRLEN(MQX_MFS_DEVICE);
            if (dirSz + labelSz + 1 > bufSz) {
                WLOG(WS_LOG_SFTP, "Buffer too small to prepend drive label");
                return NULL;
            }
            WMEMMOVE(buf + labelSz, buf, dirSz);
            WMEMCPY(buf, MQX_MFS_DEVICE, labelSz);
            buf[labelSz + dirSz] = '\0';

            /* remove trailing '\' if present */
            if (buf[labelSz + dirSz - 1] == '\\') {
                buf[labelSz + dirSz - 1] = '\0';
            }

            return buf;
        }
        #define WGETCWD(fs,r,rSz)  wGetCwd((fs),(r),(rSz))
    #endif /* WGETCWD */

    #define WCLOSE  fclose

    #ifndef WPWRITE
        static inline int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
            const unsigned int* shortOffset)
        {
            unsigned int ofst;

            ofst = shortOffset[0];
            if (ofst > 0) {
                fseek(fd, ofst, 0);
            }

            return fwrite(buf, sz, 1, fd);
        }
        #define WPWRITE(fd,b,s,o) wPwrite((fd),(b),(s),(o))
    #endif

#ifndef WPREAD
    static inline int wPread(WFD fd, unsigned char* buf, unsigned int sz,
            const unsigned int* shortOffset)
    {
        unsigned int ofst;

        ofst = shortOffset[0];
        if (ofst > 0) {
            fseek(fd, ofst, 0);
        }

        return fread(buf, 1, sz, fd);
    }
    #define WPREAD(fd,b,s,o)  wPread((fd),(b),(s),(o))
#endif

#ifndef NO_WOLFSSH_DIR

    typedef struct {
        void* dirStruct;    /* MFS DIR_STRUCT_PTR */
        char pathname[PATHNAME_SIZE];
        void* heap;
    } MQXDIR;

    #define WDIR MQXDIR*

    #ifndef WOPENDIR
        static inline int wOpenDir(void* fs, void* heap, WDIR* d, char* dir)
        {
            int pathSz;
            MQXDIR* mqxdir;
            MQX_FILE_PTR mfs_ptr;
            void* dir_ptr;
            char* mode_ptr = "f";

            if (fs == NULL || d == NULL || dir == NULL) {
                return -1;
            }
            mfs_ptr = (MQX_FILE_PTR)fs;

            /* check that filesystem handle is valid */
            if (_io_is_fs_valid(mfs_ptr) == 0) {
                WLOG(WS_LOG_SFTP, "Invalid file system pointer");
                return -1;
            }

            /* allocate MQXDIR struct to hold handle and modified pathname */
            mqxdir = (MQXDIR*)WMALLOC(sizeof(MQXDIR), mqxdir->heap,
                                      DYNTYPE_SFTP);
            if (mqxdir == NULL) {
                WLOG(WS_LOG_SFTP, "Unable to allocate MQXDIR in wOpenDir");
                return -1;
            }
            WMEMSET(mqxdir, 0, sizeof(MQXDIR));
            mqxdir->heap = heap;

            /* copy dir to pathname */
            pathSz = WSTRLEN(dir);

            if (pathSz > sizeof(mqxdir->pathname)) {
                WLOG(WS_LOG_SFTP, "dir name too large for buffer");
                WFREE(mqxdir, mqxdir->heap, DYNTYPE_SFTP);
                return -1;
            }
            WMEMCPY(mqxdir->pathname, dir, pathSz);

            /* remove trailing '.' */
            if (mqxdir->pathname[pathSz-1] == '.' &&
                mqxdir->pathname[pathSz-2] != '.') {
                mqxdir->pathname[pathSz-1] = '\0';
                pathSz--;
            }

            /* add trailing delimiter if not present */
            if (mqxdir->pathname[pathSz-1] != WS_DELIM) {
                if (pathSz + 2 > sizeof(mqxdir->pathname)) {
                    WFREE(mqxdir, mqxdir->heap, DYNTYPE_SFTP);
                    return -1;
                }
                mqxdir->pathname[pathSz] = WS_DELIM;
                mqxdir->pathname[pathSz + 1] = '\0';
            }

            dir_ptr = _io_mfs_dir_open(mfs_ptr, mqxdir->pathname, mode_ptr);
            if (dir_ptr == NULL) {
                WLOG(WS_LOG_SFTP, "MFS file not found");
                WFREE(mqxdir, mqxdir->heap, DYNTYPE_SFTP);
                return -1;
            }
            mqxdir->dirStruct = dir_ptr;
            *d = mqxdir;

            return 0;
        }
        #define WOPENDIR(fs,h,c,d)  wOpenDir((fs),(h),(c),(d))
    #endif

    #ifndef WCLOSEDIR
        static inline int wCloseDir(WDIR* d)
        {
            MQXDIR* mqxdir = NULL;

            if (d != NULL) {
                mqxdir = *d;

                _io_mfs_dir_close(mqxdir->dirStruct);
                WFREE(mqxdir, mqxdir->heap, DYNTYPE_SFTP);
            }

            return 0;
        }
        #define WCLOSEDIR(d)  wCloseDir((d))
    #endif /* WCLOSEDIR */
#endif /* NO_WOLFSSH_DIR */

#elif defined(USE_WINDOWS_API)

    #include <windows.h>
    #ifndef _WIN32_WCE
        #include <sys/types.h>
        #include <sys/stat.h>
        #include <io.h>
        #include <direct.h>
        #include <fcntl.h>
    #endif

    #define WSTAT_T           struct _stat
    #define WRMDIR(fs,d)      _rmdir((d))
    #define WSTAT(p,b)        _stat((p),(b))
    #define WLSTAT(p,b)       _stat((p),(b))
    #define WREMOVE(fs,d)     remove((d))
    #define WRENAME(fs,o,n)   rename((o),(n))
    #define WGETCWD(fs,r,rSz)    _getcwd((r),(rSz))
    #define WOPEN(f,m,p)      _open((f),(m),(p))
    #define WCLOSE(fd)        _close((fd))

    #define WFD int
    int wPwrite(WFD, unsigned char*, unsigned int, const unsigned int*);
    int wPread(WFD, unsigned char*, unsigned int, const unsigned int*);
    #define WPWRITE(fd,b,s,o) wPwrite((fd),(b),(s),(o))
    #define WPREAD(fd,b,s,o)  wPread((fd),(b),(s),(o))
    #define WS_DELIM          '\\'

    #define WOLFSSH_O_RDWR    _O_RDWR
    #define WOLFSSH_O_RDONLY  _O_RDONLY
    #define WOLFSSH_O_WRONLY  _O_WRONLY
    #define WOLFSSH_O_APPEND  _O_APPEND
    #define WOLFSSH_O_CREAT   _O_CREAT
    #define WOLFSSH_O_TRUNC   _O_TRUNC
    #define WOLFSSH_O_EXCL    _O_EXCL

    #ifndef NO_WOLFSSH_DIR
        #define WDIR HANDLE
    #endif /* NO_WOLFSSH_DIR */

#else
    #include <unistd.h>   /* used for rmdir */
    #include <sys/stat.h> /* used for mkdir, stat, and lstat */
    #include <stdio.h>    /* used for remove and rename */
    #include <dirent.h>   /* used for opening directory and reading */

    #define WSTAT_T      struct stat
    #define WRMDIR(fs,d) rmdir((d))
    #define WSTAT(p,b)   stat((p),(b))
    #ifndef USE_OSE_API
        #define WLSTAT(p,b) lstat((p),(b))
    #else
        #define WLSTAT(p,b) stat((p),(b))
    #endif
    #define WREMOVE(fs,d)     remove((d))
    #define WRENAME(fs,o,n)   rename((o),(n))
    #define WGETCWD(fs,r,rSz) getcwd((r),(rSz))
    #define WS_DELIM '/'

    #include <fcntl.h> /* used for open, close, pwrite, and pread */
    #define WFD int
    #define WOLFSSH_O_RDWR   O_RDWR
    #define WOLFSSH_O_RDONLY O_RDONLY
    #define WOLFSSH_O_WRONLY O_WRONLY
    #define WOLFSSH_O_APPEND O_APPEND
    #define WOLFSSH_O_CREAT  O_CREAT
    #define WOLFSSH_O_TRUNC  O_TRUNC
    #define WOLFSSH_O_EXCL   O_EXCL

    #define WOPEN(f,m,p) open((f),(m),(p))
    #define WCLOSE(fd) close((fd))
    int wPwrite(WFD, unsigned char*, unsigned int, const unsigned int*);
    int wPread(WFD, unsigned char*, unsigned int, const unsigned int*);
    #define WPWRITE(fd,b,s,o) wPwrite((fd),(b),(s),(o))
    #define WPREAD(fd,b,s,o)  wPread((fd),(b),(s),(o))

#ifndef NO_WOLFSSH_DIR
    #include <dirent.h> /* used for opendir, readdir, and closedir */
    #define WDIR DIR*

    /* returns 0 on success */
    #define WOPENDIR(fs,h,c,d)  ((*(c) = opendir((d))) == NULL)
    #define WCLOSEDIR(d) closedir(*(d))
    #define WREADDIR(d)  readdir(*(d))
#endif /* NO_WOLFSSH_DIR */
#endif
#endif /* WOLFSSH_SFTP or WOLFSSH_SCP */

/* setup compiler inlining */
#ifndef INLINE
#ifndef NO_INLINE
    #ifdef _MSC_VER
        #define INLINE __inline
    #elif defined(__GNUC__)
        #define INLINE inline
    #elif defined(__IAR_SYSTEMS_ICC__)
        #define INLINE inline
    #elif defined(THREADX)
        #define INLINE _Inline
    #else
        #define INLINE
    #endif
#else
    #define INLINE
#endif
#endif /* INLINE */


/* GCC 7 has new switch() fall-through detection */
#if defined(__GNUC__) && !defined(FALL_THROUGH)
    #if ((__GNUC__ > 7) || ((__GNUC__ == 7) && (__GNUC_MINOR__ >= 1)))
        #define FALL_THROUGH __attribute__ ((fallthrough))
    #endif
#endif
#ifndef FALL_THROUGH
    #define FALL_THROUGH
#endif


#if defined(USE_WINDOWS_API)
    #define WS_SOCKET_T SOCKET
#else
    #define WS_SOCKET_T int
#endif


#if !defined(NO_TERMIOS) && defined(WOLFSSH_TERM)
    #if !defined(USE_WINDOWS_API) && !defined(MICROCHIP_PIC32)
        #include <termios.h>
        #define WOLFSSH_TERMIOS struct termios
    #elif defined(USE_WINDOWS_API)
        #define WOLFSSH_TERMIOS DWORD
    #else
        #define NO_TERMIOS
    #endif
#endif

#ifdef __cplusplus
}
#endif

