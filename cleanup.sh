#!/bin/sh

    cd /dev/shm

    ls -a

    ls -a | grep -E "*xplagiat*" | xargs rm {} -f

    # check contents just to make sure
    ls -a
