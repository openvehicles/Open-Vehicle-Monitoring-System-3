/* api.c
 *
 * Copyright (C) 2014-2017 wolfSSL Inc.
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


#include <stdio.h>
#include <wolfssh/ssh.h>


#define Fail(description, result) do {                                         \
    printf("\nERROR - %s line %d failed with:", __FILE__, __LINE__);           \
    printf("\n    expected: "); printf description;                            \
    printf("\n    result:   "); printf result; printf("\n\n");                 \
    abort();                                                                   \
} while(0)

#define Assert(test, description, result) if (!(test)) Fail(description, result)

#define AssertTrue(x)    Assert( (x), ("%s is true",     #x), (#x " => FALSE"))
#define AssertFalse(x)   Assert(!(x), ("%s is false",    #x), (#x " => TRUE"))
#define AssertNotNull(x) Assert( (x), ("%s is not null", #x), (#x " => NULL"))

#define AssertNull(x) do {                                                     \
    void* _x = (void *) (x);                                                   \
                                                                               \
    Assert(!_x, ("%s is null", #x), (#x " => %p", _x));                        \
} while(0)

#define AssertInt(x, y, op, er) do {                                           \
    int _x = x;                                                                \
    int _y = y;                                                                \
                                                                               \
    Assert(_x op _y, ("%s " #op " %s", #x, #y), ("%d " #er " %d", _x, _y));    \
} while(0)

#define AssertIntEQ(x, y) AssertInt(x, y, ==, !=)
#define AssertIntNE(x, y) AssertInt(x, y, !=, ==)
#define AssertIntGT(x, y) AssertInt(x, y,  >, <=)
#define AssertIntLT(x, y) AssertInt(x, y,  <, >=)
#define AssertIntGE(x, y) AssertInt(x, y, >=,  <)
#define AssertIntLE(x, y) AssertInt(x, y, <=,  >)

#define AssertStr(x, y, op, er) do {                                           \
    const char* _x = x;                                                        \
    const char* _y = y;                                                        \
    int   _z = strcmp(_x, _y);                                                 \
                                                                               \
    Assert(_z op 0, ("%s " #op " %s", #x, #y),                                 \
                                            ("\"%s\" " #er " \"%s\"", _x, _y));\
} while(0)

#define AssertStrEQ(x, y) AssertStr(x, y, ==, !=)
#define AssertStrNE(x, y) AssertStr(x, y, !=, ==)
#define AssertStrGT(x, y) AssertStr(x, y,  >, <=)
#define AssertStrLT(x, y) AssertStr(x, y,  <, >=)
#define AssertStrGE(x, y) AssertStr(x, y, >=,  <)
#define AssertStrLE(x, y) AssertStr(x, y, <=,  >)


enum WS_TestEndpointTypes {
    TEST_GOOD_ENDPOINT_SERVER = WOLFSSH_ENDPOINT_SERVER,
    TEST_GOOD_ENDPOINT_CLIENT = WOLFSSH_ENDPOINT_CLIENT,
    TEST_BAD_ENDPOINT_NEXT,
    TEST_BAD_ENDPOINT_LAST = 255
};

static void test_wolfSSH_CTX_new(void)
{
    WOLFSSH_CTX* ctx;

    AssertNull(ctx = wolfSSH_CTX_new(TEST_BAD_ENDPOINT_NEXT, NULL));
    wolfSSH_CTX_free(ctx);
    
    AssertNull(ctx = wolfSSH_CTX_new(TEST_BAD_ENDPOINT_LAST, NULL));
    wolfSSH_CTX_free(ctx);
    
    AssertNotNull(ctx = wolfSSH_CTX_new(TEST_GOOD_ENDPOINT_SERVER, NULL));
    wolfSSH_CTX_free(ctx);

    AssertNotNull(ctx = wolfSSH_CTX_new(TEST_GOOD_ENDPOINT_CLIENT, NULL));
    wolfSSH_CTX_free(ctx);
}


static void test_server_wolfSSH_new(void)
{
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));

    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
}


static void test_client_wolfSSH_new(void)
{
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));

    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
}


int main(void)
{
    AssertIntEQ(wolfSSH_Init(), WS_SUCCESS);

    test_wolfSSH_CTX_new();
    test_server_wolfSSH_new();
    test_client_wolfSSH_new();

    AssertIntEQ(wolfSSH_Cleanup(), WS_SUCCESS);

    return 0;
}
