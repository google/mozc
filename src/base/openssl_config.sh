#!/bin/sh
# Copyright 2012 Google Inc. All Rights Reserved.
#
# This script generates opensslconf.h.

if [ $# -ne 2 ]; then
  echo "Usage: openssl_config.sh BASE_DIR GEN_OUT_DIR"
  exit 1
fi

GEN_OUT_DIR=$1
BASE_DIR=$2
CONFIG_ARGS="no-asm no-hw no-krb5 no-dso -DOPENSSL_NO_SOCK=1 -D_GNU_SOURCE"

cp -rf ${BASE_DIR}/third_party/openssl/openssl ${GEN_OUT_DIR}
cd ${GEN_OUT_DIR}/openssl
find ${GEN_OUT_DIR}/openssl | xargs chmod +w
MACHINE=i686 MAKEFLAGS= ./config ${CONFIG_ARGS}
