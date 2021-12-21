/* tls_threaded.c
 *
 * Copyright (C) 2006-2020 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#define USE_CERT_BUFFERS_2048
#include <wolfssl/certs_test.h>
#include <wolfssl/test.h>

#ifdef WOLFSSL_ZEPHYR
#define printf   printk
#endif

#define BUFFER_SIZE           2048
#define STATIC_MEM_SIZE       (96*1024)
#define THREAD_STACK_SIZE     (12*1024)

/* The stack to use in the server's thread. */
K_THREAD_STACK_DEFINE(server_stack, THREAD_STACK_SIZE);

#ifdef WOLFSSL_STATIC_MEMORY
    static WOLFSSL_HEAP_HINT* HEAP_HINT_SERVER;
    static WOLFSSL_HEAP_HINT* HEAP_HINT_CLIENT;

    static byte gMemoryServer[STATIC_MEM_SIZE];
    static byte gMemoryClient[STATIC_MEM_SIZE];
#else
    #define HEAP_HINT_SERVER NULL
    #define HEAP_HINT_CLIENT NULL
#endif /* WOLFSSL_STATIC_MEMORY */

/* Buffer to hold data for client to read. */
unsigned char client_buffer[BUFFER_SIZE];
int client_buffer_sz = 0;
wolfSSL_Mutex client_mutex;

/* Buffer to hold data for server to read. */
unsigned char server_buffer[BUFFER_SIZE];
int server_buffer_sz = 0;
wolfSSL_Mutex server_mutex;

/* Application data to send. */
static const char msgHTTPGet[] = "GET /index.html HTTP/1.0\r\n\r\n";
static const char msgHTTPIndex[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Connection: close\n"
    "\n"
    "<html>\n"
    "<head>\n"
    "<title>Welcome to wolfSSL!</title>\n"
    "</head>\n"
    "<body>\n"
    "<p>wolfSSL has successfully performed handshake!</p>\n"
    "</body>\n"
    "</html>\n";

/* wolfSSL client wants to read data from the server. */
static int recv_client(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    wc_LockMutex(&client_mutex);
    if (client_buffer_sz > 0) {
        /* Take as many bytes is available or requested from buffer. */
        if (sz > client_buffer_sz)
            sz = client_buffer_sz;
        XMEMCPY(buff, client_buffer, sz);
        if (sz < client_buffer_sz) {
            XMEMMOVE(client_buffer, client_buffer + sz, client_buffer_sz - sz);
        }
        client_buffer_sz -= sz;
    }
    else
        sz = WOLFSSL_CBIO_ERR_WANT_READ;
    wc_UnLockMutex(&client_mutex);

    return sz;
}

/* wolfSSL client wants to write data to the server. */
static int send_client(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    wc_LockMutex(&server_mutex);
    if (server_buffer_sz < BUFFER_SIZE)
    {
        /* Put in as many bytes requested or will fit in buffer. */
        if (sz > BUFFER_SIZE - server_buffer_sz)
            sz = BUFFER_SIZE - server_buffer_sz;
        XMEMCPY(server_buffer + server_buffer_sz, buff, sz);
        server_buffer_sz += sz;
    }
    else
        sz = WOLFSSL_CBIO_ERR_WANT_WRITE;
    wc_UnLockMutex(&server_mutex);

    return sz;
}

/* wolfSSL server wants to read data from the client. */
static int recv_server(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    wc_LockMutex(&server_mutex);
    if (server_buffer_sz > 0) {
        /* Take as many bytes is available or requested from buffer. */
        if (sz > server_buffer_sz)
            sz = server_buffer_sz;
        XMEMCPY(buff, server_buffer, sz);
        if (sz < server_buffer_sz) {
            XMEMMOVE(server_buffer, server_buffer + sz, server_buffer_sz - sz);
        }
        server_buffer_sz -= sz;
    }
    else
        sz = WOLFSSL_CBIO_ERR_WANT_READ;
    wc_UnLockMutex(&server_mutex);

    return sz;
}

/* wolfSSL server wants to write data to the client. */
static int send_server(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    wc_LockMutex(&client_mutex);
    if (client_buffer_sz < BUFFER_SIZE)
    {
        /* Put in as many bytes requested or will fit in buffer. */
        if (sz > BUFFER_SIZE - client_buffer_sz)
            sz = BUFFER_SIZE - client_buffer_sz;
        XMEMCPY(client_buffer + client_buffer_sz, buff, sz);
        client_buffer_sz += sz;
    }
    else
        sz = WOLFSSL_CBIO_ERR_WANT_WRITE;
    wc_UnLockMutex(&client_mutex);

    return sz;
}

/* Create a new wolfSSL client with a server CA certificate. */
static int wolfssl_client_new(WOLFSSL_CTX** ctx, WOLFSSL** ssl)
{
    int ret = 0;
    WOLFSSL_CTX* client_ctx = NULL;
    WOLFSSL*     client_ssl = NULL;

    /* Create and initialize WOLFSSL_CTX */
    if ((client_ctx = wolfSSL_CTX_new_ex(wolfTLSv1_2_client_method(),
                                                   HEAP_HINT_CLIENT)) == NULL) {
        printf("ERROR: failed to create WOLFSSL_CTX\n");
        ret = -1;
    }

    if (ret == 0) {
        /* Load client certificates into WOLFSSL_CTX */
        if (wolfSSL_CTX_load_verify_buffer(client_ctx, ca_cert_der_2048,
                sizeof_ca_cert_der_2048, WOLFSSL_FILETYPE_ASN1) !=
                WOLFSSL_SUCCESS) {
            printf("ERROR: failed to load CA certificate\n");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Register callbacks */
        wolfSSL_SetIORecv(client_ctx, recv_client);
        wolfSSL_SetIOSend(client_ctx, send_client);

        /* Create a WOLFSSL object */
        if ((client_ssl = wolfSSL_new(client_ctx)) == NULL) {
            printf("ERROR: failed to create WOLFSSL object\n");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* make wolfSSL object nonblocking */
        wolfSSL_set_using_nonblock(client_ssl, 1);

        /* Return newly created wolfSSL context and object */
        *ctx = client_ctx;
        *ssl = client_ssl;
    }
    else {
        if (client_ssl != NULL)
            wolfSSL_free(client_ssl);
        if (client_ctx != NULL)
            wolfSSL_CTX_free(client_ctx);
    }

    return ret;
}

/* Client connecting to server using TLS */
static int wolfssl_client_connect(WOLFSSL* ssl)
{
    int ret = 0;

    if (wolfSSL_connect(ssl) != WOLFSSL_SUCCESS) {
        if (!wolfSSL_want_read(ssl) && !wolfSSL_want_write(ssl))
            ret = -1;
    }

    return ret;
}



/* Create a new wolfSSL server with a certificate for authentication. */
static int wolfssl_server_new(WOLFSSL_CTX** ctx, WOLFSSL** ssl)
{
    int ret = 0;
    WOLFSSL_CTX* server_ctx = NULL;
    WOLFSSL*     server_ssl = NULL;

    /* Create and initialize WOLFSSL_CTX */
    if ((server_ctx = wolfSSL_CTX_new_ex(wolfTLSv1_2_server_method(),
                                                   HEAP_HINT_SERVER)) == NULL) {
        printf("ERROR: failed to create WOLFSSL_CTX\n");
        ret = -1;
    }

    if (ret == 0) {
        /* Load client certificates into WOLFSSL_CTX */
        if (wolfSSL_CTX_use_certificate_buffer(server_ctx,
                server_cert_der_2048, sizeof_server_cert_der_2048,
                WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
            printf("ERROR: failed to load server certificate\n");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Load client certificates into WOLFSSL_CTX */
        if (wolfSSL_CTX_use_PrivateKey_buffer(server_ctx,
                server_key_der_2048, sizeof_server_key_der_2048,
                WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
            printf("ERROR: failed to load server key\n");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Register callbacks */
        wolfSSL_SetIORecv(server_ctx, recv_server);
        wolfSSL_SetIOSend(server_ctx, send_server);

        /* Create a WOLFSSL object */
        if ((server_ssl = wolfSSL_new(server_ctx)) == NULL) {
            printf("ERROR: failed to create WOLFSSL object\n");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* make wolfSSL object nonblocking */
        wolfSSL_set_using_nonblock(server_ssl, 1);

        /* Return newly created wolfSSL context and object */
        *ctx = server_ctx;
        *ssl = server_ssl;
    }
    else {
        if (server_ssl != NULL)
            wolfSSL_free(server_ssl);
        if (server_ctx != NULL)
            wolfSSL_CTX_free(server_ctx);
    }

    return ret;
}

/* Server accepting a client using TLS */
static int wolfssl_server_accept(WOLFSSL* ssl)
{
    int ret = 0;

    if (wolfSSL_accept(ssl) != WOLFSSL_SUCCESS) {
        if (!wolfSSL_want_read(ssl) && !wolfSSL_want_write(ssl))
            ret = -1;
    }

    return ret;
}


/* Send application data. */
static int wolfssl_send(WOLFSSL* ssl, const char* msg)
{
    int ret = 0;
    int len;

    printf("Sending:\n%s\n", msg);
    len = wolfSSL_write(ssl, msg, XSTRLEN(msg));
    if (len < 0)
        ret = len;
    else if (len != XSTRLEN(msg))
        ret = -1;

    return ret;
}

/* Receive application data. */
static int wolfssl_recv(WOLFSSL* ssl)
{
    int ret;
    byte reply[256];

    ret = wolfSSL_read(ssl, reply, sizeof(reply)-1);
    if (ret > 0) {
        reply[ret] = '\0';
        printf("Received:\n%s\n", reply);
        ret = 1;
    }
    else if (wolfSSL_want_read(ssl) || wolfSSL_want_write(ssl))
        ret = 0;

    return ret;
}


/* Free the WOLFSSL object and context. */
static void wolfssl_free(WOLFSSL_CTX* ctx, WOLFSSL* ssl)
{
    if (ssl != NULL)
        wolfSSL_free(ssl);
    if (ctx != NULL)
        wolfSSL_CTX_free(ctx);
}


/* Display the static memory usage. */
static void wolfssl_memstats(WOLFSSL* ssl)
{
#ifdef WOLFSSL_STATIC_MEMORY
    WOLFSSL_MEM_CONN_STATS ssl_stats;

    XMEMSET(&ssl_stats, 0 , sizeof(ssl_stats));

    if (wolfSSL_is_static_memory(ssl, &ssl_stats) != 1)
        printf("static memory was not used with ssl");
    else {
        printf("*** This is memory state before wolfSSL_free is called\n");
        printf("peak connection memory = %d\n", ssl_stats.peakMem);
        printf("current memory in use  = %d\n", ssl_stats.curMem);
        printf("peak connection allocs = %d\n", ssl_stats.peakAlloc);
        printf("current connection allocs = %d\n",ssl_stats.curAlloc);
        printf("total connection allocs   = %d\n",ssl_stats.totalAlloc);
        printf("total connection frees    = %d\n\n", ssl_stats.totalFr);
    }
#else
    (void)ssl;
#endif
}


/* Start the server thread. */
void start_thread(THREAD_FUNC func, func_args* args, THREAD_TYPE* thread)
{
    k_thread_create(thread, server_stack, K_THREAD_STACK_SIZEOF(server_stack),
                    func, args, NULL, NULL, 5, 0, K_NO_WAIT);
}

void join_thread(THREAD_TYPE thread)
{
    /* Threads are handled in the kernel. */
}


/* Thread to do the server operations. */
void server_thread(void* arg1, void* arg2, void* arg3)
{
    int ret = 0;
    WOLFSSL_CTX* server_ctx = NULL;
    WOLFSSL*     server_ssl = NULL;


#ifdef WOLFSSL_STATIC_MEMORY
    if (wc_LoadStaticMemory(&HEAP_HINT_SERVER, gMemoryServer,
                               sizeof(gMemoryServer),
                               WOLFMEM_GENERAL | WOLFMEM_TRACK_STATS, 1) != 0) {
        printf("unable to load static memory");
        ret = -1;
    }

    if (ret == 0)
#endif
        ret = wolfssl_server_new(&server_ctx, &server_ssl);

    while (ret == 0) {
        ret = wolfssl_server_accept(server_ssl);
        if (ret == 0 && wolfSSL_is_init_finished(server_ssl))
            break;
    }

    /* Receive HTTP request */
    while (ret == 0) {
        ret = wolfssl_recv(server_ssl);
    }
    if (ret == 1)
        ret = 0;
    /* Send HTTP response */
    if (ret == 0)
        ret = wolfssl_send(server_ssl, msgHTTPIndex);

    printf("Server Return: %d\n", ret);

#ifdef WOLFSSL_STATIC_MEMORY
    printf("Server Memory Stats\n");
#endif
    wolfssl_memstats(server_ssl);
    wolfssl_free(server_ctx, server_ssl);
}

int main()
{
    int ret = 0;
    WOLFSSL_CTX* client_ctx = NULL;
    WOLFSSL*     client_ssl = NULL;
    THREAD_TYPE serverThread;

    wolfSSL_Init();

    wc_InitMutex(&client_mutex);
    wc_InitMutex(&server_mutex);

    /* Start server */
    start_thread(server_thread, NULL, &serverThread);

#ifdef WOLFSSL_STATIC_MEMORY
    if (wc_LoadStaticMemory(&HEAP_HINT_CLIENT, gMemoryClient,
                               sizeof(gMemoryClient),
                               WOLFMEM_GENERAL | WOLFMEM_TRACK_STATS, 1) != 0) {
        printf("unable to load static memory");
        ret = -1;
    }

    if (ret == 0)
#endif
    {
        /* Client connection */
        ret = wolfssl_client_new(&client_ctx, &client_ssl);
    }

    while (ret == 0) {
        ret = wolfssl_client_connect(client_ssl);
        if (ret == 0 && wolfSSL_is_init_finished(client_ssl))
            break;
        k_sleep(10);
    }

    if (ret == 0) {
        printf("Handshake complete\n");

        /* Send HTTP request */
        ret = wolfssl_send(client_ssl, msgHTTPGet);
    }
    /* Receive HTTP response */
    while (ret == 0) {
        k_sleep(10);
        ret = wolfssl_recv(client_ssl);
    }
    if (ret == 1)
        ret = 0;

    printf("Client Return: %d\n", ret);

    join_thread(serverThread);

#ifdef WOLFSSL_STATIC_MEMORY
    printf("Client Memory Stats\n");
#endif
    wolfssl_memstats(client_ssl);
    wolfssl_free(client_ctx, client_ssl);

    wolfSSL_Cleanup();

    printf("Done\n");

    return (ret == 0) ? 0 : 1;
}

