#!/bin/bash

cd "`dirname \"$0\"`/.."

autoreconf -fiv
CC=${CLANG:-clang} ./configure --host=none-none-none
make
