#!/bin/sh

# commit-tests.sh

# make sure current config is ok
echo "Testing current config..."
if ! make clean check
then
    echo "Current config make test failed"
    exit 1
fi


# make sure basic config is ok
echo "Testing basic config..."
if ! ./configure
then
    echo "Basic config ./configure failed"
    exit 1
fi

if ! make check
then
    echo "Basic config make test failed"
    exit 1
fi

# make sure the all enabled config is ok
echo "Testing enabled all config..."
if ! ./configure --enable-all
then
    echo "Enabled all config ./configure failed"
    exit 1
fi

if ! make check
then
    echo "Enabled all config make test failed"
    exit 1
fi

exit 0
