#!/bin/sh

    cd /dev/shm

    ls -a

    ls -a | grep -E "*xmicul*" | xargs rm {} -f

    # check contents just to make sure
    ls -a

    ps aux | grep proj2

    killall -9 ios_proj2
    killall -9 proj2
