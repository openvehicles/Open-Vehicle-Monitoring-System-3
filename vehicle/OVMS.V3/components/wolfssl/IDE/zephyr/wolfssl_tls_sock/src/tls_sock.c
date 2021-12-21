/* tls_sock.c
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
#define MAX_SEND_SIZE         256

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
        printf("wolfSSL Error: %d\n", wolfSSL_get_error(ssl, -1));
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
        printf("wolfSSL Error: %d\n", wolfSSL_get_error(ssl, -1));
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


int wolfssl_server_accept_tcp(WOLFSSL* ssl, SOCKET_T* fd, SOCKET_T* acceptfd)
{
    int ret = 0;
    SOCKET_T      sockfd;
    SOCKET_T      clientfd = WOLFSSL_SOCKET_INVALID;
    SOCKADDR_IN_T client;
    socklen_t     client_len = sizeof(client);
    word16        port = 443;
    struct sockaddr_in bind_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0)
        ret = -1;

    if (ret == 0) {
        *fd = sockfd;
        printf("Server Listen\n");
        listen(sockfd, 5);
        if (WOLFSSL_SOCKET_IS_INVALID(sockfd))
            ret = -1;
    }
    if (ret == 0) {
        printf("Server Accept\n");
        clientfd = accept(sockfd, (struct sockaddr*)&client,
                          (ACCEPT_THIRD_T)&client_len);
        if (WOLFSSL_SOCKET_IS_INVALID(clientfd))
            ret = -1;
    }
    if (ret == 0) {
        *acceptfd = clientfd;
        tcp_set_nonblocking(&clientfd);
    }

    if (ret == 0) {
        printf("Server has client\n");
        if (wolfSSL_set_fd(ssl, clientfd) != WOLFSSL_SUCCESS)
            ret = -1;
    }

    return ret;
}

/* Thread to do the server operations. */
void server_thread(void* arg1, void* arg2, void* arg3)
{
    int           ret = 0;
    WOLFSSL_CTX*  server_ctx = NULL;
    WOLFSSL*      server_ssl = NULL;
    SOCKET_T      sockfd   = WOLFSSL_SOCKET_INVALID;
    SOCKET_T      clientfd = WOLFSSL_SOCKET_INVALID;


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

    if (ret == 0)
        ret = wolfssl_server_accept_tcp(server_ssl, &sockfd, &clientfd);

    while (ret == 0) {
        k_sleep(100);
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
    if (clientfd != WOLFSSL_SOCKET_INVALID)
        CloseSocket(clientfd);
    if (sockfd != WOLFSSL_SOCKET_INVALID)
        CloseSocket(sockfd);
}

int wolfssl_client_connect_tcp(WOLFSSL* ssl, SOCKET_T* fd)
{
    int              ret = 0;
    SOCKET_T         sockfd = WOLFSSL_SOCKET_INVALID;
    static struct    addrinfo hints;
    struct addrinfo* res;

    XMEMSET(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("192.0.2.1", "443", &hints, &res) != 0)
        ret = -1;

    if (ret == 0) {
        printf("Client socket\n");
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (WOLFSSL_SOCKET_IS_INVALID(sockfd))
            ret = -1;
    }
    if (ret == 0) {
        *fd = sockfd;
        tcp_set_nonblocking(&sockfd);

        printf("Client Connect\n");
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0)
            ret = -1;
    }

    if (ret == 0) {
        printf("Client Connected\n");
        if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS)
            ret = -1;
    }

    return ret;
}

/* Thread to do the client operations. */
void client_thread()
{
    int ret = 0;
    WOLFSSL_CTX* client_ctx = NULL;
    WOLFSSL*     client_ssl = NULL;
    SOCKET_T     sockfd = WOLFSSL_SOCKET_INVALID;

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

    if (ret == 0)
        ret = wolfssl_client_connect_tcp(client_ssl, &sockfd);

    while (ret == 0) {
        k_sleep(10);
        ret = wolfssl_client_connect(client_ssl);
        if (ret == 0 && wolfSSL_is_init_finished(client_ssl))
            break;
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

#ifdef WOLFSSL_STATIC_MEMORY
    printf("Client Memory Stats\n");
#endif
    wolfssl_memstats(client_ssl);
    wolfssl_free(client_ctx, client_ssl);
    if (sockfd != WOLFSSL_SOCKET_INVALID)
        CloseSocket(sockfd);
}

int main()
{
    int          ret = 0;
    THREAD_TYPE  serverThread;

    wolfSSL_Init();

    /* Start server */
    start_thread(server_thread, NULL, &serverThread);

    k_sleep(100);
    client_thread();

    join_thread(serverThread);

    wolfSSL_Cleanup();

    printf("Done\n");

    return 0;
}

