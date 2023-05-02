#!/bin/bash
cd /root
$HOME/premake5 --os=linux gmake2

cd $HOME/projects/linux/gmake2

export CFLAGS=-m32
export LDFLAGS=-m32

make clean && make build=release && cp x86/ReleaseWithSymbols/*.dll /output
ls -alh /output/
