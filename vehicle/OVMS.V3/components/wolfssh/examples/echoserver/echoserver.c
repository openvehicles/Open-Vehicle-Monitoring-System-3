/* echoserver.c
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

#define WOLFSSH_TEST_SERVER
#define WOLFSSH_TEST_ECHOSERVER

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#ifdef WOLFSSL_USER_SETTINGS
    #include <wolfssl/wolfcrypt/settings.h>
#else
    #include <wolfssl/options.h>
#endif

#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssh/ssh.h>
#include <wolfssh/wolfsftp.h>
#include <wolfssh/agent.h>
#include <wolfssh/test.h>
#include <wolfssl/wolfcrypt/ecc.h>

#include "examples/echoserver/echoserver.h"

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    #include <pthread.h>
#endif

#ifdef WOLFSSL_NUCLEUS
    /* use buffers for keys with server */
    #define NO_FILESYSTEM
    #define WOLFSSH_NO_EXIT
#endif

#ifdef NO_FILESYSTEM
    #include <wolfssh/certs_test.h>
#endif

#ifdef WOLFSSH_SHELL
    #ifdef HAVE_PTY_H
        #include <pty.h>
    #endif
    #ifdef HAVE_UTIL_H
        #include <util.h>
    #endif
    #ifdef HAVE_TERMIOS_H
        #include <termios.h>
    #endif
    #include <pwd.h>
    #include <errno.h>
    #include <signal.h>
#endif /* WOLFSSH_SHELL */

#ifdef WOLFSSH_AGENT
    #include <stddef.h>
    #include <sys/socket.h>
    #include <sys/un.h>
#endif /* WOLFSSH_AGENT */

#ifdef HAVE_SYS_SELECT_H
    #include <sys/select.h>
#endif


#ifndef NO_WOLFSSH_SERVER

#define TEST_SFTP_TIMEOUT 1

static const char echoserverBanner[] = "wolfSSH Example Echo Server\n";

static int quit = 0;
wolfSSL_Mutex doneLock;
#define MAX_PASSWD_RETRY 3
static int passwdRetry = MAX_PASSWD_RETRY;



#ifdef WOLFSSH_AGENT
typedef struct WS_AgentCbActionCtx {
    struct sockaddr_un name;
    int listenFd;
    int fd;
    pid_t pid;
    int state;
} WS_AgentCbActionCtx;
#endif


typedef struct {
    WOLFSSH* ssh;
    WS_SOCKET_T fd;
    word32 id;
    int echo;
    char   nonBlock;
#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    WOLFSSH_CTX *ctx;
#endif
#ifdef WOLFSSH_AGENT
    WS_AgentCbActionCtx agentCbCtx;
#endif
} thread_ctx_t;


#ifndef EXAMPLE_HIGHWATER_MARK
    #define EXAMPLE_HIGHWATER_MARK 0x3FFF8000 /* 1GB - 32kB */
#endif
#ifndef EXAMPLE_BUFFER_SZ
    #define EXAMPLE_BUFFER_SZ 4096
#endif
#define SCRATCH_BUFFER_SZ 1200


static byte find_char(const byte* str, const byte* buf, word32 bufSz)
{
    const byte* cur;

    while (bufSz) {
        cur = str;
        while (*cur != '\0') {
            if (*cur == *buf)
                return *cur;
            cur++;
        }
        buf++;
        bufSz--;
    }

    return 0;
}


static int dump_stats(thread_ctx_t* ctx)
{
    char stats[1024];
    word32 statsSz;
    word32 txCount, rxCount, seq, peerSeq;

    wolfSSH_GetStats(ctx->ssh, &txCount, &rxCount, &seq, &peerSeq);

    WSNPRINTF(stats, sizeof(stats),
            "Statistics for Thread #%u:\r\n"
            "  txCount = %u\r\n  rxCount = %u\r\n"
            "  seq = %u\r\n  peerSeq = %u\r\n",
            ctx->id, txCount, rxCount, seq, peerSeq);
    statsSz = (word32)strlen(stats);

    fprintf(stderr, "%s", stats);
    return wolfSSH_stream_send(ctx->ssh, (byte*)stats, statsSz);
}

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)

#define SSH_TIMEOUT 10

static int callbackReqSuccess(WOLFSSH *ssh, void *buf, word32 sz, void *ctx)
{
    if ((WOLFSSH *)ssh != *(WOLFSSH **)ctx){
        printf("ssh(%x) != ctx(%x)\n", (unsigned int)ssh,
                (unsigned int)*(WOLFSSH **)ctx);
        return WS_FATAL_ERROR;
    }
    printf("Global Request Success[%d]: %s\n", sz, sz>0?buf:"No payload");
    return WS_SUCCESS;
}

static int callbackReqFailure(WOLFSSH *ssh, void *buf, word32 sz, void *ctx)
{
    if ((WOLFSSH *)ssh != *(WOLFSSH **)ctx)
    {
        printf("ssh(%x) != ctx(%x)\n", (unsigned int)ssh,
                (unsigned int)*(WOLFSSH **)ctx);
        return WS_FATAL_ERROR;
    }
    printf("Global Request Failure[%d]: %s\n", sz, sz > 0 ? buf : "No payload");
    return WS_SUCCESS;
}

static void *global_req(void *ctx)
{
    int ret;
    const char str[] = "SampleRequest";
    thread_ctx_t *threadCtx = (thread_ctx_t *)ctx;
    byte buf[0];

    wolfSSH_SetReqSuccess(threadCtx->ctx, callbackReqSuccess);
    wolfSSH_SetReqSuccessCtx(threadCtx->ssh, &threadCtx->ssh); /* dummy ctx */
    wolfSSH_SetReqFailure(threadCtx->ctx, callbackReqFailure);
    wolfSSH_SetReqFailureCtx(threadCtx->ssh, &threadCtx->ssh); /* dummy ctx */

    while(1){

        sleep(SSH_TIMEOUT);

        ret = wolfSSH_global_request(threadCtx->ssh, (const unsigned char *)str,
                strlen(str), 1);
        if (ret != WS_SUCCESS)
        {
            printf("Global Request Failed.\n");
            wolfSSH_shutdown(threadCtx->ssh);
            return NULL;
        }

        wolfSSH_stream_read(threadCtx->ssh, buf, 0);
        if (ret != WS_SUCCESS)
        {
            printf("wolfSSH_stream_read Failed.\n");
            wolfSSH_shutdown(threadCtx->ssh);
            return NULL;
        }
    }
    return NULL;
}

#endif


/* handle SSH echo operations
 * returns 0 on success
 */
static int ssh_worker(thread_ctx_t* threadCtx) {
    byte* buf = NULL;
    byte* tmpBuf;
    int bufSz, backlogSz = 0, rxSz, txSz, stop = 0, txSum;

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    pthread_t globalReq_th;
    int ret = 0;
    /* submit Global Request for keep-alive */
    ret = pthread_create(&globalReq_th, NULL, global_req, threadCtx);
    if (ret != 0)
    {
        printf("pthread_create() failed.\n");
    }
#endif

    do {
        bufSz = EXAMPLE_BUFFER_SZ + backlogSz;

        tmpBuf = (byte*)realloc(buf, bufSz);
        if (tmpBuf == NULL)
            stop = 1;
        else
            buf = tmpBuf;

        if (!stop) {
            if (threadCtx->nonBlock) {
                WS_SOCKET_T sockfd;
                int select_ret = 0;

                sockfd = (WS_SOCKET_T)wolfSSH_get_fd(threadCtx->ssh);

                select_ret = tcp_select(sockfd, 1);
                if (select_ret != WS_SELECT_RECV_READY &&
                    select_ret != WS_SELECT_ERROR_READY &&
                    select_ret != WS_SELECT_TIMEOUT) {
                    break;
                }
            }

            rxSz = wolfSSH_stream_read(threadCtx->ssh,
                                       buf + backlogSz,
                                       EXAMPLE_BUFFER_SZ);
            if (rxSz > 0) {
                backlogSz += rxSz;
                txSum = 0;
                txSz = 0;

                while (backlogSz != txSum && txSz >= 0 && !stop) {
                    txSz = wolfSSH_stream_send(threadCtx->ssh,
                                               buf + txSum,
                                               backlogSz - txSum);

                    if (txSz > 0) {
                        byte c;
                        const byte matches[] = { 0x03, 0x05, 0x06, 0x00 };

                        c = find_char(matches, buf + txSum, txSz);
                        switch (c) {
                            case 0x03:
                                stop = 1;
                                break;
                            case 0x06:
                                if (wolfSSH_TriggerKeyExchange(threadCtx->ssh)
                                        != WS_SUCCESS)
                                    stop = 1;
                                break;
                            case 0x05:
                                if (dump_stats(threadCtx) <= 0)
                                    stop = 1;
                                break;
                        }
                        txSum += txSz;
                    }
                    else if (txSz != WS_REKEYING) {
                        int error = wolfSSH_get_error(threadCtx->ssh);
                        if (error != WS_WANT_WRITE) {
                            stop = 1;
                        }
                        else {
                            txSz = 0;
                        }
                    }
                }

                if (txSum < backlogSz)
                    memmove(buf, buf + txSum, backlogSz - txSum);
                backlogSz -= txSum;
            }
            else {
                int error = wolfSSH_get_error(threadCtx->ssh);
                if (error != WS_WANT_READ)
                    stop = 1;
            }
        }
    } while (!stop);

    free(buf);

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    pthread_join(globalReq_th, NULL);
#endif

    return 0;
}


#ifdef WOLFSSH_AGENT

static const char EnvNameAuthPort[] = "SSH_AUTH_SOCK";

static int wolfSSH_AGENT_DefaultActions(WS_AgentCbAction action, void* vCtx)
{
    WS_AgentCbActionCtx* ctx = (WS_AgentCbActionCtx*)vCtx;
    int ret = 0;

    if (action == WOLFSSH_AGENT_LOCAL_SETUP) {
        struct sockaddr_un* name = &ctx->name;
        size_t size;

        memset(name, 0, sizeof(struct sockaddr_un));
        ctx->pid = getpid();
        name->sun_family = AF_LOCAL;

        ret = snprintf(name->sun_path, sizeof(name->sun_path),
                "/tmp/wolfserver.%d", ctx->pid);

        if (ret == 0) {
            name->sun_path[sizeof(name->sun_path) - 1] = '\0';
            size = strlen(name->sun_path) +
                    offsetof(struct sockaddr_un, sun_path);
            ctx->listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (ctx->listenFd == -1) {
                ret = -1;
            }
        }

        if (ret == 0) {
            ret = bind(ctx->listenFd,
                    (struct sockaddr *)name, (socklen_t)size);
        }

        if (ret == 0) {
            ret = setenv(EnvNameAuthPort, name->sun_path, 1);
        }

        if (ret == 0) {
            ret = listen(ctx->listenFd, 5);
        }

        if (ret == 0) {
            ctx->state = AGENT_STATE_LISTEN;
        }
        else {
            ret = WS_AGENT_SETUP_E;
        }
    }
    else if (action == WOLFSSH_AGENT_LOCAL_CLEANUP) {
        close(ctx->listenFd);
        unlink(ctx->name.sun_path);
        unsetenv(EnvNameAuthPort);
    }
    else
        ret = WS_AGENT_INVALID_ACTION;

    return ret;
}

#endif


#ifdef WOLFSSH_SHELL

#define SE_BUF_SIZE         4096

typedef struct
{
    char *buf;
    int rdidx;
    int wridx;
    int size;
} BUF_T;


#ifdef SHELL_DEBUG

static void display_ascii(char *p_buf,
                          int count)
{
  int i;

  printf("  *");
  for (i = 0; i < count; i++) {
    char      tmp_char    = p_buf[i];

    if ((isalnum(tmp_char) || ispunct(tmp_char)) && (tmp_char > 0))
        printf("%c", tmp_char);
    else
        printf(".");
  }
  printf("*\n");
}


static void buf_dump(char *buf, int len)
{
    int i;

    printf("\n");
    for (i = 0; i<len; i++) {
        if ((i%16) == 0) {
            printf("%04x :", i);
        }
        printf("%02x ", (unsigned char)buf[i]);

        if (((i + 1)%16) == 0) {
            display_ascii((buf+i - 15), 16);
        }
    }
    if ((len % 16) != 0) {
        display_ascii((buf +len -len%16), (len%16));
    }
    return;
}


static int termios_show(int fd)
{
    struct termios tios;
    int i;
    int rc;

    memset((void *) &tios, 0, sizeof(tios));
    rc = tcgetattr(fd, &tios);
    printf("tcgetattr returns=%x\n", rc);

    printf("iflag/oflag/cflag/lflag = %x/%x/%x/%x\n",
            (unsigned int)tios.c_iflag, (unsigned int)tios.c_oflag,
            (unsigned int)tios.c_cflag, (unsigned int)tios.c_lflag);
    printf("c_ispeed/c_ospeed = %x/%x\n",
            (unsigned int)tios.c_ispeed, (unsigned int)tios.c_ospeed);
    for (i = 0; i < NCCS; i++) {
        printf("c_cc[%d] = %hhx\n", i, tios.c_cc[i]);
    }
    return 0;
}

#endif /* SHELL_DEBUG */


int ChildRunning = 0;

static void ChildSig(int sig)
{
    (void)sig;
    ChildRunning = 0;
}


static int shell_worker(thread_ctx_t* threadCtx)
{
    WOLFSSH* ssh;
    int master;
    int sock_fd;
    int max_fd = 0;
    pid_t pid;
    int rc = 0;
    const char *userName;
    struct passwd *p_passwd;

    if (threadCtx == NULL)
        return 1;

    ssh = threadCtx->ssh;
    if (ssh == NULL)
        return WS_FATAL_ERROR;

    sock_fd = wolfSSH_get_fd(ssh);
    userName = wolfSSH_GetUsername(ssh);
    p_passwd = getpwnam((const char *)userName);
    if (p_passwd == NULL) {
        /* Not actually a user on the system. */
        return WS_FATAL_ERROR;
    }

    ChildRunning = 1;
    pid = forkpty(&master, NULL, NULL, NULL);

    if (pid < 0) {
        /* forkpty failed, so return */
        ChildRunning = 0;
        return WS_FATAL_ERROR;
    }
    else if (pid == 0) {
        /* Child process */
        const char *args[] = {"-sh", NULL};

        signal(SIGINT, SIG_DFL);

        #ifdef SHELL_DEBUG
            printf("userName is %s\n", userName);
            system("env");
        #endif

        setenv("HOME", p_passwd->pw_dir, 1);
        setenv("LOGNAME", p_passwd->pw_name, 1);
        rc = chdir(p_passwd->pw_dir);
        if (rc != 0) {
            return WS_FATAL_ERROR;
        }

        execv("/bin/sh", (char **)args);
    }
    else {
        /* Parent process */
        BUF_T buf_rx;
        BUF_T buf_tx;
        struct termios tios;
        fd_set read_fd;
        fd_set write_fd;
        int flags;
#ifdef WOLFSSH_AGENT
        BUF_T agent_buf;
        int agentFd = -1;
        int listenFd = threadCtx->agentCbCtx.listenFd;
        word32 agentChannelId = 0;
#endif

        #ifdef SHELL_DEBUG
            printf("In pid > 0; getpid=%d\n", (int)getpid());
        #endif
        signal(SIGCHLD, ChildSig);

        rc = tcgetattr(master, &tios);
        if (rc != 0) {
            printf("tcgetattr failed: rc =%d,errno=%x\n", rc, errno);
            return WS_FATAL_ERROR;
        }
        rc = tcsetattr(master, TCSAFLUSH, &tios);
        if (rc != 0) {
            printf("tcsetattr failed: rc =%d,errno=%x\n", rc, errno);
            return WS_FATAL_ERROR;
        }

        #ifdef SHELL_DEBUG
            termios_show(master);
        #endif

        memset((void *)&buf_rx, 0, sizeof(buf_rx));
        memset((void *)&buf_tx, 0, sizeof(buf_tx));

        buf_rx.buf = (char*)malloc(SE_BUF_SIZE);
        if (buf_rx.buf == NULL) {
            return WS_FATAL_ERROR;
        }

        buf_tx.buf = (char*)malloc(SE_BUF_SIZE);
        if (buf_tx.buf == NULL) {
            free(buf_rx.buf);
            return WS_FATAL_ERROR;
        }

#ifdef WOLFSSH_AGENT
        memset((void *)&agent_buf, 0, sizeof(agent_buf));
        agent_buf.buf = (char*)malloc(SE_BUF_SIZE);
        if (agent_buf.buf == NULL) {
            free(buf_rx.buf);
            free(buf_tx.buf);
            return WS_FATAL_ERROR;
        }
#endif

        /*set sock_fd to non-blocking;
          select() blocks even if socket is set to non-blocking*/
        flags = fcntl(sock_fd, F_GETFL, 0);
        #ifdef SHELL_DEBUG
            printf("flags read = 0x%x\n", flags);
        #endif

        if (flags == -1) {
            printf("fcntl F_GETFL failed: errno = 0x%x\n", errno);
        }
        else {
            flags = flags | O_NONBLOCK;
            rc = fcntl(sock_fd, F_SETFL, flags);
            if (rc == -1) {
                printf("fcntl F_SETFL: flags = 0x%x, rc = 0x%x, errno=0x%x\n",
                        flags, rc, errno);
            }
        }

        while (ChildRunning) {
            FD_ZERO(&read_fd);
            FD_ZERO(&write_fd);
            int count;
            int cnt_r;
            int cnt_w;

            if (buf_rx.size > 0) {
                FD_SET(master, &write_fd);
            }
            if (buf_tx.size < SE_BUF_SIZE) {
                FD_SET(master, &read_fd);
            }
            if (buf_rx.size < SE_BUF_SIZE) {
                FD_SET(sock_fd, &read_fd);
            }
            if (buf_tx.size > 0) {
                FD_SET(sock_fd, &write_fd);
            }
            max_fd = master;
            if (sock_fd > max_fd) {
                max_fd = sock_fd;
            }

#ifdef WOLFSSH_AGENT
            if (threadCtx->agentCbCtx.state == AGENT_STATE_LISTEN) {
                FD_SET(threadCtx->agentCbCtx.listenFd, &read_fd);
                if (threadCtx->agentCbCtx.listenFd > max_fd)
                    max_fd = threadCtx->agentCbCtx.listenFd;
            }

            if (threadCtx->agentCbCtx.state == AGENT_STATE_CONNECTED) {
                FD_SET(agentFd, &read_fd);
                if (threadCtx->agentCbCtx.fd > max_fd)
                    max_fd = agentFd;
            }
#endif

            #ifdef SHELL_DEBUG
                printf("Initial buf_rx(r/w/s) = %x/%x/%x:\n",
                        buf_rx.rdidx, buf_rx.wridx, buf_rx.size);
                printf("Initial buf_tx(r/w/s) = %x/%x/%x:\n",
                        buf_tx.rdidx, buf_tx.wridx, buf_tx.size);
            #endif /* SHELL_DEBUG */


            rc = select(max_fd + 1, &read_fd, &write_fd, NULL, NULL);
            #ifdef SHELL_DEBUG
                printf("select return 0x%x\n", rc);
            #endif
            if (rc == -1)
                break;

            if (FD_ISSET(master, &write_fd)) {
                #ifdef SHELL_DEBUG
                    printf("master set in writefd\n");
                #endif
                count = MIN(SE_BUF_SIZE - buf_rx.wridx, buf_rx.size);

                cnt_w = (int)write(master, buf_rx.buf+buf_rx.wridx, count);
                if (cnt_w < 0) {
                    if (errno != EAGAIN) {
                        #ifdef SHELL_DEBUG
                            printf("Break:write master returns %d: errno =%x\n",
                                    cnt_w, errno);
                        #endif
                        break;
                    }
                }
                else {
                    buf_rx.wridx += cnt_w;
                    if (buf_rx.wridx >= SE_BUF_SIZE) {
                        buf_rx.wridx = 0;
                    }
                    buf_rx.size -= cnt_w;
                    if (buf_rx.size == 0) {
                        buf_rx.rdidx = 0;
                        buf_rx.wridx = 0;
                    }
                }
            }

            if (FD_ISSET(sock_fd, &write_fd)) {
                #ifdef SHELL_DEBUG
                    printf("sock_fd set in writefd\n");
                #endif
                count = MIN(SE_BUF_SIZE - buf_tx.wridx, buf_tx.size);
                cnt_w = wolfSSH_stream_send(ssh,
                        (byte *)(buf_tx.buf + buf_tx.wridx), count);
                if (cnt_w <= 0) {
                    #ifdef SHELL_DEBUG
                        printf("Break:write sock_fd returns %d: errno =%x\n",
                                cnt_w, errno);
                    #endif
                    break;
                }
                else {
                    buf_tx.wridx += cnt_w;
                    if (buf_tx.wridx >= SE_BUF_SIZE) {
                        buf_tx.wridx = 0;
                    }
                    buf_tx.size -= cnt_w;
                    if (buf_tx.size == 0) {
                        buf_tx.rdidx = 0;
                        buf_tx.wridx = 0;
                    }
                }
            }

            if (FD_ISSET(master, &read_fd)) {
                #ifdef SHELL_DEBUG
                    printf("master set in readfd\n");
                #endif
                count = MIN(SE_BUF_SIZE - buf_tx.rdidx,
                        SE_BUF_SIZE - buf_tx.size);
                cnt_r = (int)read(master, buf_tx.buf+buf_tx.rdidx, count);
                if (cnt_r < 0) {
                    if (errno != EAGAIN) {
                        #ifdef SHELL_DEBUG
                            printf("Break:read master returns %d: errno =%x\n",
                                    cnt_r, errno);
                        #endif
                        break;
                    }
                }
                else {
                    #ifdef SHELL_DEBUG
                        buf_dump(buf_tx.buf+buf_tx.rdidx, cnt_r);
                    #endif
                    buf_tx.rdidx += cnt_r;
                    if (buf_tx.rdidx >= SE_BUF_SIZE) {
                        buf_tx.rdidx = 0;
                    }
                    buf_tx.size += cnt_r;
                }
            }

            if (FD_ISSET(sock_fd, &read_fd)) {
                #ifdef SHELL_DEBUG
                    printf("sock_fd set in readfd\n");
                #endif
                count = MIN(SE_BUF_SIZE - buf_rx.rdidx,
                        SE_BUF_SIZE - buf_rx.size);
                /* The following tries to read from the first channel inside
                   the stream. If the pending data in the socket is for
                   another channel, this will return an error with id
                   WS_CHAN_RXD. That means the agent has pending data in its
                   channel. The additional channel is only used with the
                   agent. */
                cnt_r = wolfSSH_stream_read(ssh,
                        (byte *)buf_rx.buf+buf_rx.rdidx, count);
                if (cnt_r <= 0) {
                    rc = wolfSSH_get_error(ssh);
                    if (rc != WS_WANT_READ) {
                        #ifdef SHELL_DEBUG
                            printf("Break:read sock_fd returns %d: errno =%x\n",
                                    cnt_r, errno);
                        #endif
                        break;
                    }
                    #ifdef WOLFSSH_AGENT
                        if (rc == WS_CHAN_RXD) {
                            wolfSSH_GetLastRxId(ssh, &agentChannelId);
                            cnt_r = wolfSSH_ChannelIdRead(ssh, agentChannelId,
                                    (byte*)agent_buf.buf, cnt_r);
                            if (cnt_r <= 0)
                                break;
                            cnt_w = (int)write(agentFd, (byte*)agent_buf.buf,
                                    cnt_r);
                            if (cnt_w <= 0)
                                break;
                        }
                    #endif
                }
                else {
                    #ifdef SHELL_DEBUG
                        buf_dump(buf_rx.buf+buf_rx.rdidx, cnt_r);
                    #endif
                    buf_rx.rdidx += cnt_r;
                    if (buf_rx.rdidx >= SE_BUF_SIZE) {
                        buf_rx.rdidx = 0;
                    }
                    buf_rx.size += cnt_r;
                }
            }

#ifdef WOLFSSH_AGENT
            if (threadCtx->agentCbCtx.state == AGENT_STATE_CONNECTED) {
                if (FD_ISSET(agentFd, &read_fd)) {
                    #ifdef SHELL_DEBUG
                        printf("agentFd set in readfd\n");
                    #endif
                    count = MIN(SE_BUF_SIZE - agent_buf.rdidx,
                            SE_BUF_SIZE - agent_buf.size);
                    cnt_r = (int)read(agentFd,
                            (byte *)agent_buf.buf+agent_buf.rdidx, count);
                    if (cnt_r == 0) {
                        /* Read zero-returned. Socket is closed. Go back
                           to listening. */
                        threadCtx->agentCbCtx.state = AGENT_STATE_LISTEN;
                        continue;
                    }
                    else if (cnt_r < 0) {
                        #ifdef SHELL_DEBUG
                            printf("Break:read agentFd returns %d: "
                                   "errno = %d\n", cnt_r, errno);
                        #endif
                        break;
                    }
                    else {
                        #ifdef SHELL_DEBUG
                            buf_dump(agent_buf.buf+agent_buf.rdidx, cnt_r);
                        #endif
                        cnt_w = wolfSSH_ChannelIdSend(ssh, agentChannelId,
                                (byte*)agent_buf.buf + agent_buf.rdidx, cnt_r);
                        if (cnt_w > 0) {
                            agent_buf.rdidx += cnt_r;
                            if (agent_buf.rdidx >= SE_BUF_SIZE) {
                                agent_buf.rdidx = 0;
                            }
                            agent_buf.size += cnt_r - cnt_w;
                        }
                        else {
                            break;
                        }
                    }
                }
            }

            if (threadCtx->agentCbCtx.state == AGENT_STATE_LISTEN) {
                if (FD_ISSET(listenFd, &read_fd)) {
                    #ifdef SHELL_DEBUG
                        printf("accepting agent connection\n");
                    #endif
                    agentFd = accept(listenFd, NULL, NULL);
                    if (agentFd == -1) {
                        rc = errno;
                        if (rc != EWOULDBLOCK) {
                            break;
                        }
                    }
                    else {
                        threadCtx->agentCbCtx.state = AGENT_STATE_CONNECTED;
                        threadCtx->agentCbCtx.fd = agentFd;
                    }
                }
            }
#endif
        }
        free(buf_rx.buf);
        free(buf_tx.buf);
#ifdef WOLFSSH_AGENT
        free(agent_buf.buf);
#endif
        close(master);
    }

    return 0;
}

#endif /* WOLFSSH_SHELL */


#ifdef WOLFSSH_SFTP
/* handle SFTP operations
 * returns 0 on success
 */
static int sftp_worker(thread_ctx_t* threadCtx) {
    byte tmp[1];
    int ret   = WS_SUCCESS;
    int error = WS_SUCCESS;
    WS_SOCKET_T sockfd;
    int select_ret = 0;

    sockfd = (WS_SOCKET_T)wolfSSH_get_fd(threadCtx->ssh);
    do {
        if (threadCtx->nonBlock) {
            if (error == WS_WANT_READ)
                printf("... sftp server would read block\n");
            else if (error == WS_WANT_WRITE)
                printf("... sftp server would write block\n");
        }

        if (wolfSSH_stream_peek(threadCtx->ssh, tmp, 1) > 0) {
            select_ret = WS_SELECT_RECV_READY;
        }
        else {
            select_ret = tcp_select(sockfd, TEST_SFTP_TIMEOUT);
        }

        if (select_ret == WS_SELECT_RECV_READY ||
            select_ret == WS_SELECT_ERROR_READY ||
            error == WS_WANT_WRITE)
        {
            ret = wolfSSH_SFTP_read(threadCtx->ssh);
            error = wolfSSH_get_error(threadCtx->ssh);
        }
        else if (select_ret == WS_SELECT_TIMEOUT)
            error = WS_WANT_READ;
        else
            error = WS_FATAL_ERROR;

        if (error == WS_WANT_READ || error == WS_WANT_WRITE)
            ret = WS_WANT_READ;

        if (ret == WS_FATAL_ERROR && error == 0) {
            WOLFSSH_CHANNEL* channel =
                wolfSSH_ChannelNext(threadCtx->ssh, NULL);
            if (channel && wolfSSH_ChannelGetEof(channel)) {
                ret = 0;
                break;
            }
        }

    } while (ret != WS_FATAL_ERROR);

    return ret;
}
#endif

static int NonBlockSSH_accept(WOLFSSH* ssh)
{
    int ret;
    int error;
    WS_SOCKET_T sockfd;
    int select_ret = 0;

    ret = wolfSSH_accept(ssh);
    error = wolfSSH_get_error(ssh);
    sockfd = (WS_SOCKET_T)wolfSSH_get_fd(ssh);

    while ((ret != WS_SUCCESS
                && ret != WS_SCP_COMPLETE && ret != WS_SFTP_COMPLETE)
            && (error == WS_WANT_READ || error == WS_WANT_WRITE)) {

        if (error == WS_WANT_READ)
            printf("... server would read block\n");
        else if (error == WS_WANT_WRITE)
            printf("... server would write block\n");

        select_ret = tcp_select(sockfd, 1);
        if (select_ret == WS_SELECT_RECV_READY  ||
            select_ret == WS_SELECT_ERROR_READY ||
            error      == WS_WANT_WRITE)
        {
            ret = wolfSSH_accept(ssh);
            error = wolfSSH_get_error(ssh);
        }
        else if (select_ret == WS_SELECT_TIMEOUT)
            error = WS_WANT_READ;
        else
            error = WS_FATAL_ERROR;
    }

    return ret;
}


static THREAD_RETURN WOLFSSH_THREAD server_worker(void* vArgs)
{
    int ret = 0, error = 0;
    thread_ctx_t* threadCtx = (thread_ctx_t*)vArgs;

    passwdRetry = MAX_PASSWD_RETRY;

    if (!threadCtx->nonBlock)
        ret = wolfSSH_accept(threadCtx->ssh);
    else
        ret = NonBlockSSH_accept(threadCtx->ssh);

    switch (ret) {
        case WS_SCP_COMPLETE:
            printf("scp file transfer completed\n");
            ret = 0;
            break;

        case WS_SFTP_COMPLETE:
        #ifdef WOLFSSH_SFTP
            ret = sftp_worker(threadCtx);
        #else
            err_sys("SFTP not compiled in. Please use --enable-sftp");
        #endif
            break;

        case WS_SUCCESS:
            #ifdef WOLFSSH_SHELL
                if (wolfSSH_GetSessionType(threadCtx->ssh) ==
                        WOLFSSH_SESSION_SHELL && !threadCtx->echo) {
                    ret = shell_worker(threadCtx);
                }
                else {
                    ret = ssh_worker(threadCtx);
                }
            #else
                ret = ssh_worker(threadCtx);
            #endif
            break;
    }

    if (ret == WS_FATAL_ERROR) {
        const char* errorStr;
        error = wolfSSH_get_error(threadCtx->ssh);

        errorStr = wolfSSH_ErrorToName(error);

        if (error == WS_VERSION_E) {
            ret = 0; /* don't break out of loop with version miss match */
            printf("%s\n", errorStr);
        }
        else if (error == WS_USER_AUTH_E) {
            wolfSSH_SendDisconnect(threadCtx->ssh,
                    WOLFSSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE);
            ret = 0; /* don't break out of loop with user auth error */
            printf("%s\n", errorStr);
        }
        else if (error == WS_SOCKET_ERROR_E) {
            ret = 0;
            printf("%s\n", errorStr);
        }
    }

    if (error != WS_SOCKET_ERROR_E && error != WS_FATAL_ERROR) {
        if (wolfSSH_shutdown(threadCtx->ssh) != WS_SUCCESS) {
            fprintf(stderr, "Error with SSH shutdown.\n");
        }
    }

    WCLOSESOCKET(threadCtx->fd);
    wolfSSH_free(threadCtx->ssh);

    if (ret != 0) {
        fprintf(stderr, "Error [%d] \"%s\" with handling connection.\n", ret,
                wolfSSH_ErrorToName(error));
    #ifndef WOLFSSH_NO_EXIT
        wc_LockMutex(&doneLock);
        quit = 1;
        wc_UnLockMutex(&doneLock);
    #endif
    }

    free(threadCtx);

    return 0;
}

#ifndef NO_FILESYSTEM
static int load_file(const char* fileName, byte* buf, word32 bufSz)
{
    FILE* file;
    word32 fileSz;
    word32 readSz;

    if (fileName == NULL) return 0;

    if (WFOPEN(&file, fileName, "rb") != 0)
        return 0;
    fseek(file, 0, XSEEK_END);
    fileSz = (word32)ftell(file);
    rewind(file);

    if (fileSz > bufSz) {
        fclose(file);
        return 0;
    }

    readSz = (word32)fread(buf, 1, fileSz, file);
    if (readSz < fileSz) {
        fclose(file);
        return 0;
    }

    fclose(file);

    return fileSz;
}
#endif /* NO_FILESYSTEM */

#ifdef HAVE_ECC521
    #define ECC_PATH "./keys/server-key-ecc-521.der"
#else
    #define ECC_PATH "./keys/server-key-ecc.der"
#endif

/* returns buffer size on success */
static int load_key(byte isEcc, byte* buf, word32 bufSz)
{
    word32 sz = 0;

#ifndef NO_FILESYSTEM
    const char* bufName;
    bufName = isEcc ? ECC_PATH : "./keys/server-key-rsa.der" ;
    sz = load_file(bufName, buf, bufSz);
#else
    /* using buffers instead */
    if (isEcc) {
        if (sizeof_ecc_key_der_256 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, ecc_key_der_256, sizeof_ecc_key_der_256);
        sz = sizeof_ecc_key_der_256;
    }
    else {
        if (sizeof_rsa_key_der_2048 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, (byte*)rsa_key_der_2048, sizeof_rsa_key_der_2048);
        sz = sizeof_rsa_key_der_2048;
    }
#endif

    return sz;
}


/* Map user names to passwords */
/* Use arrays for username and p. The password or public key can
 * be hashed and the hash stored here. Then I won't need the type. */
typedef struct PwMap {
    byte type;
    byte username[32];
    word32 usernameSz;
    byte p[WC_SHA256_DIGEST_SIZE];
    struct PwMap* next;
} PwMap;


typedef struct PwMapList {
    PwMap* head;
} PwMapList;


static PwMap* PwMapNew(PwMapList* list, byte type, const byte* username,
                       word32 usernameSz, const byte* p, word32 pSz)
{
    PwMap* map;

    map = (PwMap*)malloc(sizeof(PwMap));
    if (map != NULL) {
        map->type = type;
        if (usernameSz >= sizeof(map->username))
            usernameSz = sizeof(map->username) - 1;
        memcpy(map->username, username, usernameSz + 1);
        map->username[usernameSz] = 0;
        map->usernameSz = usernameSz;

        if (type != WOLFSSH_USERAUTH_NONE) {
            wc_Sha256Hash(p, pSz, map->p);
        }

        map->next = list->head;
        list->head = map;
    }

    return map;
}


static void PwMapListDelete(PwMapList* list)
{
    if (list != NULL) {
        PwMap* head = list->head;

        while (head != NULL) {
            PwMap* cur = head;
            head = head->next;
            memset(cur, 0, sizeof(PwMap));
            free(cur);
        }
    }
}


static const char samplePasswordBuffer[] =
    "jill:upthehill\n"
    "jack:fetchapail\n";


#ifdef HAVE_ECC
#ifndef NO_ECC256
static const char samplePublicKeyEccBuffer[] =
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBNkI5JTP6D0lF42tbxX19cE87hztUS6FSDoGvPfiU0CgeNSbI+aFdKIzTP5CQEJSvm25"
    "qUzgDtH7oyaQROUnNvk= hansel\n"
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBKAtH8cqaDbtJFjtviLobHBmjCtG56DMkP6A4M2H9zX2/YCg1h9bYS7WHd9UQDwXO1Hh"
    "IZzRYecXh7SG9P4GhRY= gretel\n";
#elif defined(HAVE_ECC521)
static const char samplePublicKeyEccBuffer[] =
    "ecdsa-sha2-nistp521 AAAAE2VjZHNhLXNoYTItbmlzdHA1MjEAAAAIbmlzdHA1MjEAAA"
    "CFBAET/BOzBb9Jx9b52VIHFP4g/uk5KceDpz2M+/Ln9WiDjsMfb4NgNCAB+EMNJUX/TNBL"
    "FFmqr7c6+zUH+QAo2qstvQDsReyFkETRB2vZD//nCZfcAe0RMtKZmgtQLKXzSlimUjXBM4"
    "/zE5lwE05aXADp88h8nuaT/X4bll9cWJlH0fUykA== hansel\n"
    "ecdsa-sha2-nistp521 AAAAE2VjZHNhLXNoYTItbmlzdHA1MjEAAAAIbmlzdHA1MjEAAA"
    "CFBAD3gANmzvkxOBN8MYwRBYO6B//7TTCtA2vwG/W5bqiVVxznXWj0xiFrgayApvH7FDpL"
    "HiJ8+c1vUsRVEa8PY5QPsgFow+xv0P2WSrRkn4/UUquftPs1ZHPhdr06LjS19ObvWM8xFZ"
    "YU6n0i28UWCUR5qE+BCTzZDWYT8V24YD8UhpaYIw== gretel\n";
#else
    #error "Enable an ECC Curve or disable ECC."
#endif
#endif

#ifndef NO_RSA
static const char samplePublicKeyRsaBuffer[] =
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x hansel\n"
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCqDwRVTRVk/wjPhoo66+Mztrc31KsxDZ"
    "+kAV0139PHQ+wsueNpba6jNn5o6mUTEOrxrz0LMsDJOBM7CmG0983kF4gRIihECpQ0rcjO"
    "P6BSfbVTE9mfIK5IsUiZGd8SoE9kSV2pJ2FvZeBQENoAxEFk0zZL9tchPS+OCUGbK4SDjz"
    "uNZl/30Mczs73N3MBzi6J1oPo7sFlqzB6ecBjK2Kpjus4Y1rYFphJnUxtKvB0s+hoaadru"
    "biE57dK6BrH5iZwVLTQKux31uCJLPhiktI3iLbdlGZEctJkTasfVSsUizwVIyRjhVKmbdI"
    "RGwkU38D043AR1h0mUoGCPIKuqcFMf gretel\n";
#endif

static const char sampleNoneBuffer[] =
    "holmes\n"
    "watson\n";


static int LoadNoneBuffer(byte* buf, word32 bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* username;

    /* Each line of none list is in the format
     *     username\n
     * This function modifies the passed-in buffer. */

    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        username = str;
        str = strchr(username, '\n');
        if (str == NULL) {
            return -1;
        }
        *str = 0;
        str++;
        if (PwMapNew(list, WOLFSSH_USERAUTH_NONE,
                     (byte*)username, (word32)strlen(username),
                     NULL, 0) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int LoadPasswordBuffer(byte* buf, word32 bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    char* username;
    char* password;

    /* Each line of passwd.txt is in the format
     *     username:password\n
     * This function modifies the passed-in buffer. */

    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        delimiter = strchr(str, ':');
        if (delimiter == NULL) {
            return -1;
        }
        username = str;
        *delimiter = 0;
        password = delimiter + 1;
        str = strchr(password, '\n');
        if (str == NULL) {
            return -1;
        }
        *str = 0;
        str++;
        if (PwMapNew(list, WOLFSSH_USERAUTH_PASSWORD,
                     (byte*)username, (word32)strlen(username),
                     (byte*)password, (word32)strlen(password)) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int LoadPublicKeyBuffer(byte* buf, word32 bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    byte* publicKey64;
    word32 publicKey64Sz;
    byte* username;
    word32 usernameSz;
    byte  publicKey[300];
    word32 publicKeySz;

    /* Each line of passwd.txt is in the format
     *     ssh-rsa AAAB3BASE64ENCODEDPUBLICKEYBLOB username\n
     * This function modifies the passed-in buffer. */
    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        /* Skip the public key type. This example will always be ssh-rsa. */
        delimiter = strchr(str, ' ');
        if (delimiter == NULL) {
            return -1;
        }
        str = delimiter + 1;
        delimiter = strchr(str, ' ');
        if (delimiter == NULL) {
            return -1;
        }
        publicKey64 = (byte*)str;
        *delimiter = 0;
        publicKey64Sz = (word32)(delimiter - str);
        str = delimiter + 1;
        delimiter = strchr(str, '\n');
        if (delimiter == NULL) {
            return -1;
        }
        username = (byte*)str;
        *delimiter = 0;
        usernameSz = (word32)(delimiter - str);
        str = delimiter + 1;
        publicKeySz = sizeof(publicKey);

        if (Base64_Decode(publicKey64, publicKey64Sz,
                          publicKey, &publicKeySz) != 0) {

            return -1;
        }

        if (PwMapNew(list, WOLFSSH_USERAUTH_PUBLICKEY,
                     username, usernameSz,
                     publicKey, publicKeySz) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int wsUserAuth(byte authType,
                      WS_UserAuthData* authData,
                      void* ctx)
{
    PwMapList* list;
    PwMap* map;
    byte authHash[WC_SHA256_DIGEST_SIZE];
    int ret;

    if (ctx == NULL) {
        fprintf(stderr, "wsUserAuth: ctx not set");
        return WOLFSSH_USERAUTH_FAILURE;
    }

    if (authType != WOLFSSH_USERAUTH_PASSWORD &&
#ifdef WOLFSSH_ALLOW_USERAUTH_NONE
        authType != WOLFSSH_USERAUTH_NONE &&
#endif
        authType != WOLFSSH_USERAUTH_PUBLICKEY) {

        return WOLFSSH_USERAUTH_FAILURE;
    }

    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
        wc_Sha256Hash(authData->sf.password.password,
                authData->sf.password.passwordSz,
                authHash);
    }
    else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
        wc_Sha256Hash(authData->sf.publicKey.publicKey,
                authData->sf.publicKey.publicKeySz,
                authHash);
    }

    list = (PwMapList*)ctx;
    map = list->head;

    while (map != NULL) {
        if (authData->usernameSz == map->usernameSz &&
            memcmp(authData->username, map->username, map->usernameSz) == 0 &&
            authData->type == map->type) {

            if (authData->type == WOLFSSH_USERAUTH_PUBLICKEY) {
                if (memcmp(map->p, authHash, WC_SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                   return WOLFSSH_USERAUTH_INVALID_PUBLICKEY;
                }
            }
            else if (authData->type == WOLFSSH_USERAUTH_PASSWORD) {
                if (memcmp(map->p, authHash, WC_SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                    passwdRetry--;
                    return (passwdRetry > 0) ?
                        WOLFSSH_USERAUTH_INVALID_PASSWORD :
                        WOLFSSH_USERAUTH_REJECTED;
                 }
            }
#ifdef WOLFSSH_ALLOW_USERAUTH_NONE
            else if (authData->type == WOLFSSH_USERAUTH_NONE) {
                return WOLFSSH_USERAUTH_SUCCESS;
            }
#endif /* WOLFSSH_ALLOW_USERAUTH_NONE */
            else {
                 return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
            }

            if (authData->type == map->type) {
                if (memcmp(map->p, authHash, WC_SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
                        passwdRetry--;
                        ret = (passwdRetry > 0) ?
                            WOLFSSH_USERAUTH_INVALID_PASSWORD :
                            WOLFSSH_USERAUTH_REJECTED;
                    }
                    else {
                        ret = WOLFSSH_USERAUTH_INVALID_PUBLICKEY;
                    }
                    return ret;
                }
            }
            else {
                return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
            }
        }
        map = map->next;
    }

    return WOLFSSH_USERAUTH_INVALID_USER;
}


static void ShowUsage(void)
{
    printf("echoserver %s\n", LIBWOLFSSH_VERSION_STRING);
    printf(" -?            display this help and exit\n");
    printf(" -1            exit after single (one) connection\n");
    printf(" -e            expect ECC public key from client\n");
    printf(" -E            use ECC private key\n");
#ifdef WOLFSSH_SHELL
    printf(" -f            echo input\n");
#endif
    printf(" -p <num>      port to connect on, default %d\n", wolfSshPort);
    printf(" -N            use non-blocking sockets\n");
#ifdef WOLFSSH_SFTP
    printf(" -d <string>   set the home directory for SFTP connections\n");
#endif
}


static void SignalTcpReady(func_args* serverArgs, word16 port)
{
#if defined(_POSIX_THREADS) && defined(NO_MAIN_DRIVER) && !defined(__MINGW32__)
    tcp_ready* ready = serverArgs->signal;
    pthread_mutex_lock(&ready->mutex);
    ready->ready = 1;
    ready->port = port;
    pthread_cond_signal(&ready->cond);
    pthread_mutex_unlock(&ready->mutex);
#else
    (void)serverArgs;
    (void)port;
#endif
}


THREAD_RETURN WOLFSSH_THREAD echoserver_test(void* args)
{
    func_args* serverArgs = (func_args*)args;
    WOLFSSH_CTX* ctx = NULL;
    PwMapList pwMapList;
    WS_SOCKET_T listenFd = 0;
    word32 defaultHighwater = EXAMPLE_HIGHWATER_MARK;
    word32 threadCount = 0;
    int multipleConnections = 1;
    int userEcc = 0;
    int peerEcc = 0;
    int echo = 0;
    int ch;
    word16 port = wolfSshPort;
    char* readyFile = NULL;
    const char* defaultSftpPath = NULL;
    char  nonBlock  = 0;

    int     argc = serverArgs->argc;
    char**  argv = serverArgs->argv;
    serverArgs->return_code = 0;

    if (argc > 0) {
    while ((ch = mygetopt(argc, argv, "?1d:efEp:R:N")) != -1) {
        switch (ch) {
            case '?' :
                ShowUsage();
                exit(EXIT_SUCCESS);

            case '1':
                multipleConnections = 0;
                break;

            case 'e' :
                userEcc = 1;
                break;

            case 'E':
                peerEcc = 1;
                break;

            case 'f':
                #ifdef WOLFSSH_SHELL
                    echo = 1;
                #endif
                break;

            case 'p':
                port = (word16)atoi(myoptarg);
                #if !defined(NO_MAIN_DRIVER) || defined(USE_WINDOWS_API)
                    if (port == 0)
                        err_sys("port number cannot be 0");
                #endif
                break;

            case 'R':
                readyFile = myoptarg;
                break;

            case 'N':
                nonBlock = 1;
                break;

            case 'd':
                defaultSftpPath = myoptarg;
                break;

            default:
                ShowUsage();
                exit(MY_EX_USAGE);
        }
    }
    }
    myoptind = 0;      /* reset for test cases */
    wc_InitMutex(&doneLock);

#ifdef WOLFSSH_TEST_BLOCK
    if (!nonBlock) {
        err_sys("Use -N when testing forced non blocking");
    }
#endif

#ifdef NO_RSA
    /* If wolfCrypt isn't built with RSA, force ECC on. */
    userEcc = 1;
    peerEcc = 1;
#endif
#ifndef HAVE_ECC
    /* If wolfCrypt isn't built with ECC, force ECC off. */
    userEcc = 0;
    peerEcc = 0;
#endif

    if (wolfSSH_Init() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't initialize wolfSSH.\n");
        exit(EXIT_FAILURE);
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Couldn't allocate SSH CTX data.\n");
        exit(EXIT_FAILURE);
    }

    memset(&pwMapList, 0, sizeof(pwMapList));
    if (serverArgs->user_auth == NULL)
        wolfSSH_SetUserAuth(ctx, wsUserAuth);
    else
        wolfSSH_SetUserAuth(ctx, ((func_args*)args)->user_auth);
    wolfSSH_CTX_SetBanner(ctx, echoserverBanner);
#ifdef WOLFSSH_AGENT
    wolfSSH_CTX_set_agent_cb(ctx, wolfSSH_AGENT_DefaultActions, NULL);
#endif

    {
        const char* bufName = NULL;
        byte buf[SCRATCH_BUFFER_SZ];
        word32 bufSz;

        bufSz = load_key(peerEcc, buf, SCRATCH_BUFFER_SZ);
        if (bufSz == 0) {
            fprintf(stderr, "Couldn't load key file.\n");
            exit(EXIT_FAILURE);
        }
        if (wolfSSH_CTX_UsePrivateKey_buffer(ctx, buf, bufSz,
                                             WOLFSSH_FORMAT_ASN1) < 0) {
            fprintf(stderr, "Couldn't use key buffer.\n");
            exit(EXIT_FAILURE);
        }

        bufSz = (word32)strlen(samplePasswordBuffer);
        memcpy(buf, samplePasswordBuffer, bufSz);
        buf[bufSz] = 0;
        LoadPasswordBuffer(buf, bufSz, &pwMapList);

        if (userEcc) {
        #ifdef HAVE_ECC
            bufName = samplePublicKeyEccBuffer;
        #endif
        }
        else {
        #ifndef NO_RSA
            bufName = samplePublicKeyRsaBuffer;
        #endif
        }
        if (bufName != NULL) {
            bufSz = (word32)strlen(bufName);
            memcpy(buf, bufName, bufSz);
            buf[bufSz] = 0;
            LoadPublicKeyBuffer(buf, bufSz, &pwMapList);
        }

        bufSz = (word32)strlen(sampleNoneBuffer);
        memcpy(buf, sampleNoneBuffer, bufSz);
        buf[bufSz] = 0;
        LoadNoneBuffer(buf, bufSz, &pwMapList);
    }
#ifdef WOLFSSL_NUCLEUS
    {
        int i;
        int ret = !NU_SUCCESS;

        /* wait for network and storage device */
        if (NETBOOT_Wait_For_Network_Up(NU_SUSPEND) != NU_SUCCESS) {
            fprintf(stderr, "Couldn't find network.\r\n");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < 15 && ret != NU_SUCCESS; i++)
        {
            fprintf(stdout, "Checking for storage device\r\n");

            ret = NU_Storage_Device_Wait(NU_NULL, NU_PLUS_TICKS_PER_SEC);
        }

        if (ret != NU_SUCCESS) {
            fprintf(stderr, "Couldn't find storage device.\r\n");
            exit(EXIT_FAILURE);
        }
    }
#endif

    /* if creating a ready file with port then override port to be 0 */
    if (readyFile != NULL) {
    #ifdef NO_FILESYSTEM
        fprintf(stderr, "cannot create readyFile with no file system.\r\n");
        exit(EXIT_FAILURE);
    #endif
        port = 0;
    }
    tcp_listen(&listenFd, &port, 1);
    /* write out port number listing to, to user set ready file */
    if (readyFile != NULL) {
    #ifndef NO_FILESYSTEM
        WFILE* f = NULL;
        int    ret;
        ret = WFOPEN(&f, readyFile, "w");
        if (f != NULL && ret == 0) {
            fprintf(f, "%d\n", (int)port);
            WFCLOSE(f);
        }
    #endif
    }

    do {
        WS_SOCKET_T      clientFd = 0;
    #ifdef WOLFSSL_NUCLEUS
        struct addr_struct clientAddr;
    #else
        SOCKADDR_IN_T clientAddr;
        socklen_t     clientAddrSz = sizeof(clientAddr);
    #endif
        WOLFSSH*      ssh;
        thread_ctx_t* threadCtx;

        threadCtx = (thread_ctx_t*)malloc(sizeof(thread_ctx_t));
        if (threadCtx == NULL) {
            fprintf(stderr, "Couldn't allocate thread context data.\n");
            exit(EXIT_FAILURE);
        }

        ssh = wolfSSH_new(ctx);
        if (ssh == NULL) {
            free(threadCtx);
            fprintf(stderr, "Couldn't allocate SSH data.\n");
            exit(EXIT_FAILURE);
        }
        wolfSSH_SetUserAuthCtx(ssh, &pwMapList);
        /* Use the session object for its own highwater callback ctx */
        if (defaultHighwater > 0) {
            wolfSSH_SetHighwaterCtx(ssh, (void*)ssh);
            wolfSSH_SetHighwater(ssh, defaultHighwater);
        }

    #ifdef WOLFSSH_SFTP
        if (defaultSftpPath) {
            if (wolfSSH_SFTP_SetDefaultPath(ssh, defaultSftpPath)
                    != WS_SUCCESS) {
                fprintf(stderr, "Couldn't store default sftp path.\n");
                exit(EXIT_FAILURE);
            }
        }
    #endif

    #ifdef WOLFSSL_NUCLEUS
        {
            byte   ipaddr[MAX_ADDRESS_SIZE];
            char   buf[16];
            short  addrLength;
            struct sockaddr_struct sock;

            addrLength = sizeof(struct sockaddr_struct);

            /* Get the local IP address for the socket.
             * 0.0.0.0 if ip adder any */
            if (NU_Get_Sock_Name(listenFd, &sock, &addrLength) != NU_SUCCESS) {
                fprintf(stderr, "Couldn't find network.\r\n");
                exit(EXIT_FAILURE);
            }

            WMEMCPY(ipaddr, &sock.ip_num, MAX_ADDRESS_SIZE);
            NU_Inet_NTOP(NU_FAMILY_IP, &ipaddr[0], buf, 16);
            fprintf(stdout, "Listening on %s:%d\r\n", buf, port);
        }
    #endif

        SignalTcpReady(serverArgs, port);

    #ifdef WOLFSSL_NUCLEUS
        clientFd = NU_Accept(listenFd, &clientAddr, 0);
    #else
        clientFd = accept(listenFd, (struct sockaddr*)&clientAddr,
                                                         &clientAddrSz);
    #endif
        if (clientFd == -1)
            err_sys("tcp accept failed");

        if (nonBlock)
            tcp_set_nonblocking(&clientFd);

        wolfSSH_set_fd(ssh, (int)clientFd);

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
        threadCtx->ctx = ctx;
#endif
        threadCtx->ssh = ssh;
        threadCtx->fd = clientFd;
        threadCtx->id = threadCount++;
        threadCtx->nonBlock = nonBlock;
        threadCtx->echo = echo;
#ifdef WOLFSSH_AGENT
        wolfSSH_set_agent_cb_ctx(ssh, &threadCtx->agentCbCtx);
#endif
        server_worker(threadCtx);

    } while (multipleConnections && !quit);

    wc_FreeMutex(&doneLock);
    PwMapListDelete(&pwMapList);
    wolfSSH_CTX_free(ctx);
    if (wolfSSH_Cleanup() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't clean up wolfSSH.\n");
        exit(EXIT_FAILURE);
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

    (void)defaultSftpPath;
    return 0;
}

#endif /* NO_WOLFSSH_SERVER */


#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;
        args.return_code = 0;
        args.user_auth = NULL;

        WSTARTTCP();

        #ifdef DEBUG_WOLFSSH
            wolfSSH_Debugging_ON();
        #endif

#ifndef WOLFSSL_NUCLEUS
        ChangeToWolfSshRoot();
#endif
#ifndef NO_WOLFSSH_SERVER
        echoserver_test(&args);
#else
        printf("wolfSSH compiled without server support\n");
#endif

        wolfSSH_Cleanup();

        return args.return_code;
    }


    int myoptind = 0;
    char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */

#ifdef WOLFSSL_NUCLEUS

    #define WS_TASK_SIZE 200000
    #define WS_TASK_PRIORITY 31
    static  NU_TASK serverTask;

    /* expecting void return on main function */
    static VOID main_nucleus(UNSIGNED argc, VOID* argv)
    {
        main((int)argc, (char**)argv);
    }


    /* using port 8080 because it was an open port on QEMU */
    VOID Application_Initialize (NU_MEMORY_POOL* memPool,
                                 NU_MEMORY_POOL* uncachedPool)
    {
        void* pt;
        int   ret;

        UNUSED_PARAMETER(uncachedPool);

        ret = NU_Allocate_Memory(memPool, &pt, WS_TASK_SIZE, NU_NO_SUSPEND);
        if (ret == NU_SUCCESS) {
            ret = NU_Create_Task(&serverTask, "wolfSSH Server", main_nucleus, 0,
                    NU_NULL, pt, WS_TASK_SIZE, WS_TASK_PRIORITY, 0,
                    NU_PREEMPT, NU_START);
            if (ret != NU_SUCCESS) {
                NU_Deallocate_Memory(pt);
            }
        }
    }
#endif /* WOLFSSL_NUCLEUS */
