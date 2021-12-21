#!/bin/sh
#
# Create configure and makefile stuff...
#

# Git hooks should come before autoreconf.
if test -d .git; then
  if ! test -d .git/hooks; then
    mkdir .git/hooks
  fi
  ln -s -f ../../pre-commit.sh .git/hooks/pre-commit
  ln -s -f ../../pre-push.sh .git/hooks/pre-push
fi

# touch options.h (make sure it exists)
touch ./wolfssl/options.h

# touch fips files for non fips distribution
touch ./ctaocrypt/src/fips.c
touch ./ctaocrypt/src/fips_test.c
touch ./wolfcrypt/src/fips.c
touch ./wolfcrypt/src/fips_test.c
touch ./wolfcrypt/src/wolfcrypt_first.c
touch ./wolfcrypt/src/wolfcrypt_last.c
touch ./wolfssl/wolfcrypt/fips.h

# touch CAVP selftest files for non-selftest distribution
touch ./wolfcrypt/src/selftest.c

# touch async crypt files
touch ./wolfcrypt/src/async.c
touch ./wolfssl/wolfcrypt/async.h

# touch async port files
touch ./wolfcrypt/src/port/intel/quickassist.c
touch ./wolfcrypt/src/port/intel/quickassist_mem.c
touch ./wolfcrypt/src/port/cavium/cavium_nitrox.c
if [ ! -d ./wolfssl/wolfcrypt/port/intel ]; then
  mkdir ./wolfssl/wolfcrypt/port/intel
fi
touch ./wolfssl/wolfcrypt/port/intel/quickassist.h
touch ./wolfssl/wolfcrypt/port/intel/quickassist_mem.h
if [ ! -d ./wolfssl/wolfcrypt/port/cavium ]; then
  mkdir ./wolfssl/wolfcrypt/port/cavium
fi
touch ./wolfssl/wolfcrypt/port/cavium/cavium_nitrox.h

# If this is a source checkout then call autoreconf with error as well
if test -e .git; then
  WARNINGS="all,error"

else
  WARNINGS="all"
fi

autoreconf --install --force --verbose

