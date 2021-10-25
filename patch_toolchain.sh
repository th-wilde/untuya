#! /bin/bash
cd `dirname "$0"`

# Patch toolchain libs define (u)int32 as (u)int
# You probably need to run this as root
patch /usr/include/newlib/sys/_stdint.h patches/_stdint.h.diff
patch /usr/lib/gcc/arm-none-eabi/8.3.1/include/stdint.h patches/stdint.h.diff
