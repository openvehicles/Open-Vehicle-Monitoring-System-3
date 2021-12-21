#!/bin/sh

WOLFSSL_SRC_DIR=../../..

if [ ! -d $WOLFSSL_SRC_DIR ]; then
    echo "Directory does not exist: $WOLFSSL_SRC_DIR"
    exit 1
fi
if [ ! -f $WOLFSSL_SRC_DIR/wolfcrypt/test/test.c ]; then
    echo "Missing source file: $WOLFSSL_SRC_DIR/wolfcrypt/test/test.h"
    exit 1
fi

ZEPHYR_DIR=
if [ $# -ne 1 ]; then
    echo "Need location of zephyr project as a command line argument"
    exit 1
else
    ZEPHYR_DIR=$1
fi
if [ ! -d $ZEPHR_DIR ]; then
    echo "Zephyr project directory does not exist: $ZEPHYR_DIR"
    exit 1
fi
ZEPHYR_CRYPTO_DIR=$ZEPHYR_DIR/zephyr/samples/crypto
if [ ! -d $ZEPHYR_CRYPTO_DIR ]; then
    echo "Zephyr crypto directory does not exist: $ZEPHYR_CRYPTO_DIR"
    exit 1
fi
ZEPHYR_WOLFSSL_DIR=$ZEPHYR_CRYPTO_DIR/wolfssl_test

echo "wolfSSL directory:"
echo "  $ZEPHYR_WOLFSSL_DIR"
rm -rf $ZEPHYR_WOLFSSL_DIR
mkdir $ZEPHYR_WOLFSSL_DIR

echo "Copy in Build files ..."
cp -r * $ZEPHYR_WOLFSSL_DIR/
rm $ZEPHYR_WOLFSSL_DIR/$0

echo "Copy Source Code ..."
rm -rf $ZEPHYR_WOLFSSL_DIR/src
mkdir $ZEPHYR_WOLFSSL_DIR/src

cp -rf ${WOLFSSL_SRC_DIR}/wolfcrypt/test/test.c $ZEPHYR_WOLFSSL_DIR/src/
cp -rf ${WOLFSSL_SRC_DIR}/wolfcrypt/test/test.h $ZEPHYR_WOLFSSL_DIR/src/

echo "Done"

