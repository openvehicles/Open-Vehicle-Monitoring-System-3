/* error.h
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
 * The error header file provides the error codes used in the library.
 */


#pragma once

#include <wolfssh/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

/* add new error strings to error.c */


/* main public return values */
enum WS_ErrorCodes {
    WS_SUCCESS         =  0,    /* function success */
    WS_FATAL_ERROR     = -1,    /* general function failure */
    WS_BAD_ARGUMENT    = -2,    /* bad function argument */
    WS_MEMORY_E        = -3,    /* memory allocation failure */
    WS_BUFFER_E        = -4,    /* input/output buffer size error */
    WS_PARSE_E         = -5,    /* general parsing error */
    WS_NOT_COMPILED    = -6,    /* feature not compiled in */
    WS_OVERFLOW_E      = -7,    /* would overflow if continued */
    WS_BAD_USAGE       = -8,    /* bad example usage */
    WS_SOCKET_ERROR_E  = -9,
    WS_WANT_READ       = -10,
    WS_WANT_WRITE      = -11,
    WS_RECV_OVERFLOW_E = -12,
    WS_VERSION_E       = -13,   /* Peer using wrong version of SSH */
    WS_SEND_OOB_READ_E = -14,
    WS_INPUT_CASE_E    = -15,
    WS_BAD_FILETYPE_E  = -16,
    WS_UNIMPLEMENTED_E = -17,
    WS_RSA_E           = -18,
    WS_BAD_FILE_E      = -19,
    WS_INVALID_ALGO_ID = -20,
    WS_DECRYPT_E       = -21,
    WS_ENCRYPT_E       = -22,
    WS_VERIFY_MAC_E    = -23,
    WS_CREATE_MAC_E    = -24,
    WS_RESOURCE_E      = -25,   /* insufficient resources for new channel */
    WS_INVALID_CHANTYPE = -26,  /* invalid channel type */
    WS_INVALID_CHANID   = -27,
    WS_INVALID_USERNAME = -28,
    WS_CRYPTO_FAILED   = -29,   /* crypto action failed */
    WS_INVALID_STATE_E = -30,
    WS_REKEYING        = -31
};


/* I/O Callback default errors */
enum WS_IOerrors {
    WS_CBIO_ERR_GENERAL    = -1,     /* general unexpected err */
    WS_CBIO_ERR_WANT_READ  = -2,     /* need to call read  again */
    WS_CBIO_ERR_WANT_WRITE = -2,     /* need to call write again */
    WS_CBIO_ERR_CONN_RST   = -3,     /* connection reset */
    WS_CBIO_ERR_ISR        = -4,     /* interrupt */
    WS_CBIO_ERR_CONN_CLOSE = -5,     /* connection closed or epipe */
    WS_CBIO_ERR_TIMEOUT    = -6      /* socket timeout */
};


#ifdef __cplusplus
}
#endif

