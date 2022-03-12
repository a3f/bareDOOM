#!/bin/sh

OPENOCD=${OPENOCD:-openocd}
BOARD=${BOARD:-stm32f429discovery}

ENDACTION="reset; shutdown"

if [ "$1" = "--daemon" ]; then
    ENDACTION=""
    shift
fi

IMAGE=${1:-images/barebox-stm32f429-disco.img}



${OPENOCD} -f board/${BOARD}.cfg --command \
     "program $IMAGE 0x08000000 verify; $ENDACTION"
