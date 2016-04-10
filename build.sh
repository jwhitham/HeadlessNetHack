#!/bin/bash -xe
export CC=gcc
cd sys/unix
bash ./setup.sh x
cd ../..
make
make install
gzip -cd example.demo.gz > hackdir/example.demo
gzip -cd example2.demo.gz > hackdir/example2.demo
