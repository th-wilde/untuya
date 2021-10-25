#! /bin/bash
cd `dirname "$0"`

# create project inside the sdk submodule
mkdir XR809SDK/project/untuya
cp -r src/* XR809SDK/project/untuya

# enshure that relevant tools are executable
chmod +x XR809SDK/tools/mkimage
chmod +x XR809SDK/tools/phoenixMC

# patch sdk
patch XR809SDK/gcc.mk patches/gcc.mk.diff

# build sqk
cd XR809SDK/src
make
make install

# build the project
cd ../project/untuya/gcc
make
make image

# collect the binaries
cd ../../../..
cp XR809SDK/project/untuya/gcc/* build_output
cp XR809SDK/project/untuya/image/xr809/* build_output

#clear project inside the sdk submodule
rm -r XR809SDK/project/untuya
