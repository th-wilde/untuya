#! /bin/bash
cd `dirname "$0"`

# enshure that relevant tools are executable
chmod +x XR809SDK/tools/mkimage
chmod +x XR809SDK/tools/phoenixMC

cd build_output
../XR809SDK/tools/mkimage -O -c ../XR809SDK/project/image_cfg/xr809/image_sta_ap_xip.cfg -o xr_system.img
