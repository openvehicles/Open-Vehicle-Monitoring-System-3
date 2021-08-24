/* wolfsftp.h
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


#ifndef _WOLFSSH_WOLFSFTP_H_
#define _WOLFSSH_WOLFSFTP_H_


#ifdef WOLFSSL_USER_SETTINGS
    #include <wolfssl/wolfcrypt/settings.h>
#else
    #include <wolfssl/options.h>
#endif

#include <wolfssh/ssh.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Packet Types */
enum WS_PacketTypes {
    WOLFSSH_FTP_INIT     = 1,
    WOLFSSH_FTP_VERSION  = 2,
    WOLFSSH_FTP_OPEN     = 3,
    WOLFSSH_FTP_CLOSE    = 4,
    WOLFSSH_FTP_READ     = 5,
    WOLFSSH_FTP_WRITE    = 6,
    WOLFSSH_FTP_LSTAT    = 7,
    WOLFSSH_FTP_FSTAT    = 8,
    WOLFSSH_FTP_SETSTAT  = 9,
    WOLFSSH_FTP_FSETSTAT = 10,
    WOLFSSH_FTP_OPENDIR  = 11,
    WOLFSSH_FTP_READDIR  = 12,
    WOLFSSH_FTP_REMOVE   = 13,
    WOLFSSH_FTP_MKDIR    = 14,
    WOLFSSH_FTP_RMDIR    = 15,
    WOLFSSH_FTP_REALPATH = 16,
    WOLFSSH_FTP_STAT     = 17,
    WOLFSSH_FTP_RENAME   = 18,
    WOLFSSH_FTP_READLINK = 19,
    WOLFSSH_FTP_LINK     = 21,
    WOLFSSH_FTP_BLOCK    = 22,
    WOLFSSH_FTP_UNBLOCK  = 23,

    WOLFSSH_FTP_STATUS  = 101,
    WOLFSSH_FTP_HANDLE  = 102,
    WOLFSSH_FTP_DATA    = 103,
    WOLFSSH_FTP_NAME    = 104,
    WOLFSSH_FTP_ATTRS   = 105,

    WOLFSSH_FTP_EXTENDED       = 200,
    WOLFSSH_FTP_EXTENDED_REPLY = 201
};

enum WS_SFTPStatus {
    WOLFSSH_FTP_OK = 0,
    WOLFSSH_FTP_EOF,
    WOLFSSH_FTP_NOFILE,
    WOLFSSH_FTP_PERMISSION,
    WOLFSSH_FTP_FAILURE,
    WOLFSSH_FTP_BADMSG,
    WOLFSSH_FTP_NOCONN,
    WOLFSSH_FTP_CONNLOST,
    WOLFSSH_FTP_UNSUPPORTED
};

enum WS_SFTPConnectStates {
    SFTP_BEGIN = 20,
    SFTP_RECV,
    SFTP_EXT,
    SFTP_DONE
};


/* implementation version */
enum {
    WOLFSSH_SFTP_VERSION = 3,
    WOLFSSH_SFTP_HEADER = 9,
    WOLFSSH_MAX_HANDLE = 256
};

/* file attribute flags */
#define WOLFSSH_FILEATRB_SIZE   0x00000001
#define WOLFSSH_FILEATRB_UIDGID 0x00000002
#define WOLFSSH_FILEATRB_PERM   0x00000004
#define WOLFSSH_FILEATRB_TIME   0x00000008
#define WOLFSSH_FILEATRB_EXT    0x80000000


/* open flags */
#define WOLFSSH_FXF_READ    0x00000001
#define WOLFSSH_FXF_WRITE   0x00000002
#define WOLFSSH_FXF_APPEND  0x00000004
#define WOLFSSH_FXF_CREAT   0x00000008
#define WOLFSSH_FXF_TRUNC   0x00000010
#define WOLFSSH_FXF_EXCL    0x00000020

#ifndef WS_DRIVE_SIZE
    #define WS_DRIVE_SIZE 1
#endif

typedef struct WS_SFTP_FILEATRB_EX WS_SFTP_FILEATRB_EX;
struct WS_SFTP_FILEATRB_EX {
    char* type;
    char* data;
    word32 typeSz;
    word32 dataSz;
    WS_SFTP_FILEATRB_EX* next;
};

typedef struct WS_SFTP_FILEATRB {
    word32 flags;
    word32 sz[2]; /* sz[0] being the lower and sz[1] being the upper */
    word32 uid; /* user ID */
    word32 gid; /* group ID */
    word32 per; /* permissions */
    word32 atime;
    word32 mtime;
    word32 extCount;
    WS_SFTP_FILEATRB_EX* exts; /* extensions */
} WS_SFTP_FILEATRB;

typedef struct WS_SFTPNAME WS_SFTPNAME;
struct WS_SFTPNAME {
    void* heap;
    char* fName; /* file name */
    char* lName; /* long name */
    word32 fSz;
    word32 lSz;
    WS_SFTP_FILEATRB atrb;
    WS_SFTPNAME* next;
};

#ifndef WOLFSSH_MAX_SFTP_RW
    #define WOLFSSH_MAX_SFTP_RW 1024
#endif

/* functions for establishing a connection */
WOLFSSH_API int wolfSSH_SFTP_accept(WOLFSSH* ssh);
WOLFSSH_API int wolfSSH_SFTP_connect(WOLFSSH* ssh);
WOLFSSH_API int wolfSSH_SFTP_negotiate(WOLFSSH* ssh);

/* protocol level functions */
WOLFSSH_LOCAL WS_SFTPNAME* wolfSSH_SFTP_ReadDir(WOLFSSH* ssh, byte* handle,
        word32 handleSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_OpenDir(WOLFSSH* ssh, byte* buf, word32 bufSz);

WOLFSSH_API int wolfSSH_SFTP_SetDefaultPath(WOLFSSH* ssh, const char* path);
WOLFSSH_API WS_SFTPNAME* wolfSSH_SFTP_RealPath(WOLFSSH* ssh, char* dir);
WOLFSSH_API int wolfSSH_SFTP_Close(WOLFSSH* ssh, byte* handle, word32 handleSz);
WOLFSSH_API int wolfSSH_SFTP_Open(WOLFSSH* ssh, char* dir, word32 reason,
        WS_SFTP_FILEATRB* atr, byte* handle, word32* handleSz);
WOLFSSH_API int wolfSSH_SFTP_SendReadPacket(WOLFSSH* ssh, byte* handle,
        word32 handleSz, const word32* ofst, byte* out, word32 outSz);
WOLFSSH_API int wolfSSH_SFTP_SendWritePacket(WOLFSSH* ssh, byte* handle,
        word32 handleSz, const word32* ofst, byte* out, word32 outSz);
WOLFSSH_API int wolfSSH_SFTP_STAT(WOLFSSH* ssh, char* dir,
        WS_SFTP_FILEATRB* atr);
WOLFSSH_API int wolfSSH_SFTP_LSTAT(WOLFSSH* ssh, char* dir,
        WS_SFTP_FILEATRB* atr);
WOLFSSH_API int wolfSSH_SFTP_SetSTAT(WOLFSSH* ssh, char* dir,
        WS_SFTP_FILEATRB* atr);

WOLFSSH_API void wolfSSH_SFTPNAME_free(WS_SFTPNAME* n);
WOLFSSH_API void wolfSSH_SFTPNAME_list_free(WS_SFTPNAME* n);


/* handling reget / reput */
WOLFSSH_API int wolfSSH_SFTP_SaveOfst(WOLFSSH* ssh, char* frm, char* to,
        const word32* ofst);
WOLFSSH_API int wolfSSH_SFTP_GetOfst(WOLFSSH* ssh, char* frm, char* to,
        word32* ofst);
WOLFSSH_API int wolfSSH_SFTP_ClearOfst(WOLFSSH* ssh);
WOLFSSH_API void wolfSSH_SFTP_Interrupt(WOLFSSH* ssh);


/* functions used for commands */
WOLFSSH_API int wolfSSH_SFTP_Remove(WOLFSSH* ssh, char* f);
WOLFSSH_API int wolfSSH_SFTP_MKDIR(WOLFSSH* ssh, char* dir,
        WS_SFTP_FILEATRB* atr);
WOLFSSH_API int wolfSSH_SFTP_RMDIR(WOLFSSH* ssh, char* dir);
WOLFSSH_API int wolfSSH_SFTP_Rename(WOLFSSH* ssh, const char* old,
        const char* nw);
WOLFSSH_API WS_SFTPNAME* wolfSSH_SFTP_LS(WOLFSSH* ssh, char* dir);
WOLFSSH_API int wolfSSH_SFTP_CHMOD(WOLFSSH* ssh, char* n, char* oct);

typedef void(WS_STATUS_CB)(WOLFSSH*, word32*, char*);
WOLFSSH_API int wolfSSH_SFTP_Get(WOLFSSH* ssh, char* from, char* to,
        byte resume, WS_STATUS_CB* statusCb);
WOLFSSH_API int wolfSSH_SFTP_Put(WOLFSSH* ssh, char* from, char* to,
        byte resume, WS_STATUS_CB* statusCb);



/* SFTP server functions */
WOLFSSH_API int wolfSSH_SFTP_read(WOLFSSH* ssh);


WOLFSSH_LOCAL int wolfSSH_SFTP_CreateStatus(WOLFSSH* ssh, word32 status,
        word32 reqId, const char* reason, const char* lang, byte* out,
        word32* outSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvRMDIR(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvMKDIR(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvOpen(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvRead(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvWrite(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvClose(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvRemove(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvRename(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvSTAT(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvLSTAT(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvSetSTAT(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvFSTAT(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);

#ifndef NO_WOLFSSH_DIR
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvOpenDir(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvReadDir(WOLFSSH* ssh, int reqId, byte* data,
        word32 maxSz);
WOLFSSH_LOCAL int wolfSSH_SFTP_RecvCloseDir(WOLFSSH* ssh, byte* handle,
        word32 handleSz);
#endif /* NO_WOLFSSH_DIR */

WOLFSSL_LOCAL int wolfSSH_SFTP_free(WOLFSSH* ssh);
WOLFSSL_LOCAL int SFTP_AddHandleNode(WOLFSSH* ssh, byte* handle,
        word32 handleSz, char* name);
WOLFSSL_LOCAL int SFTP_RemoveHandleNode(WOLFSSH* ssh, byte* handle,
        word32 handleSz);

WOLFSSH_LOCAL void wolfSSH_SFTP_ShowSizes(void);

#ifdef __cplusplus
}
#endif

#endif /* _WOLFSSH_WOLFSFTP_H_ */

