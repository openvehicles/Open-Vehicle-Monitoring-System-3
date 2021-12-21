/* error.h
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
 * The error header file provides the error codes used in the library.
 */


#ifndef _WOLFSSH_ERROR_H_
#define _WOLFSSH_ERROR_H_

#include <wolfssh/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

/* add new error strings to error.c */


/* main public return values */
enum WS_ErrorCodes {
    WS_SUCCESS              =  0,    /* function success */
    WS_FATAL_ERROR          = -1001, /* name deprecated */
    WS_ERROR                = -1001, /* general function failure */
    WS_BAD_ARGUMENT         = -1002, /* bad function argument */
    WS_MEMORY_E             = -1003, /* memory allocation failure */
    WS_BUFFER_E             = -1004, /* input/output buffer size error */
    WS_PARSE_E              = -1005, /* general parsing error */
    WS_NOT_COMPILED         = -1006, /* feature not compiled in */
    WS_OVERFLOW_E           = -1007, /* would overflow if continued */
    WS_BAD_USAGE            = -1008, /* bad example usage */
    WS_SOCKET_ERROR_E       = -1009,
    WS_WANT_READ            = -1010,
    WS_WANT_WRITE           = -1011,
    WS_RECV_OVERFLOW_E      = -1012,
    WS_VERSION_E            = -1013, /* Peer using wrong version of SSH */
    WS_SEND_OOB_READ_E      = -1014,
    WS_INPUT_CASE_E         = -1015,
    WS_BAD_FILETYPE_E       = -1016,
    WS_UNIMPLEMENTED_E      = -1017,
    WS_RSA_E                = -1018,
    WS_BAD_FILE_E           = -1019,
    WS_INVALID_ALGO_ID      = -1020,
    WS_DECRYPT_E            = -1021,
    WS_ENCRYPT_E            = -1022,
    WS_VERIFY_MAC_E         = -1023,
    WS_CREATE_MAC_E         = -1024,
    WS_RESOURCE_E           = -1025, /* not enough resources for new channel */
    WS_INVALID_CHANTYPE     = -1026, /* invalid channel type */
    WS_INVALID_CHANID       = -1027,
    WS_INVALID_USERNAME     = -1028,
    WS_CRYPTO_FAILED        = -1029, /* crypto action failed */
    WS_INVALID_STATE_E      = -1030,
    WS_EOF                  = -1031,
    WS_INVALID_PRIME_CURVE  = -1032,
    WS_ECC_E                = -1033,
    WS_CHANOPEN_FAILED      = -1034,
    WS_REKEYING             = -1035, /* Status: rekey in progress */
    WS_CHANNEL_CLOSED       = -1036, /* Status: channel closed */
    WS_INVALID_PATH_E       = -1037,
    WS_SCP_CMD_E            = -1038,
    WS_SCP_BAD_MSG_E        = -1039,
    WS_SCP_PATH_LEN_E       = -1040,
    WS_SCP_TIMESTAMP_E      = -1041,
    WS_SCP_DIR_STACK_EMPTY_E = -1042,
    WS_SCP_CONTINUE         = -1043,
    WS_SCP_ABORT            = -1044,
    WS_SCP_ENTER_DIR        = -1045,
    WS_SCP_EXIT_DIR         = -1046,
    WS_SCP_EXIT_DIR_FINAL   = -1047,
    WS_SCP_COMPLETE         = -1048, /* SCP transfer complete */
    WS_SCP_INIT             = -1049, /* SCP transfer verified */
    WS_MATCH_KEX_ALGO_E     = -1050, /* cannot match KEX algo with peer */
    WS_MATCH_KEY_ALGO_E     = -1051, /* cannot match key algo with peer */
    WS_MATCH_ENC_ALGO_E     = -1052, /* cannot match encrypt algo with peer */
    WS_MATCH_MAC_ALGO_E     = -1053, /* cannot match MAC algo with peer */
    WS_PERMISSIONS          = -1054,
    WS_SFTP_COMPLETE        = -1055, /* SFTP connection established */
    WS_NEXT_ERROR           = -1056, /* Getting next value/state is error */
    WS_CHAN_RXD             = -1057, /* Status that channel data received. */
    WS_INVALID_EXTDATA      = -1058, /* invalid Channel Extended Data Type */
    WS_SFTP_BAD_REQ_ID      = -1060, /* SFTP Bad request ID */
    WS_SFTP_BAD_REQ_TYPE    = -1061, /* SFTP Bad request ID */
    WS_SFTP_STATUS_NOT_OK   = -1062, /* SFTP Status not OK */
    WS_SFTP_FILE_DNE        = -1063, /* SFTP File Does Not Exist */
    WS_SIZE_ONLY            = -1064, /* Only getting size of buffer needed */
    WS_CLOSE_FILE_E         = -1065, /* Unable to close local file */
    WS_PUBKEY_REJECTED_E    = -1066, /* Server public key rejected */
    WS_EXTDATA              = -1067, /* Extended Data available to be read */
    WS_USER_AUTH_E          = -1068, /* User authentication error */
    WS_SSH_NULL_E           = -1069, /* SSH was null */
    WS_SSH_CTX_NULL_E       = -1070, /* SSH_CTX was null */
    WS_CHANNEL_NOT_CONF     = -1071, /* Channel open not confirmed. */
    WS_CHANGE_AUTH_E        = -1072, /* Changing auth type attempt */
    WS_WINDOW_FULL          = -1073,
    WS_MISSING_CALLBACK     = -1074, /* Callback is missing */
    WS_DH_SIZE_E            = -1075, /* DH prime larger than expected */
    WS_PUBKEY_SIG_MIN_E     = -1076, /* Signature too small */
    WS_AGENT_NULL_E         = -1077, /* AGENT was null */
    WS_AGENT_NO_KEY_E       = -1078, /* AGENT doesn't have requested key */
    WS_AGENT_CXN_FAIL       = -1079, /* Couldn't connect to agent. */

    WS_LAST_E               = -1079  /* Update this to indicate last error */
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
#endif /* _WOLFSSH_ERROR_H_ */

