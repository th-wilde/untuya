# untuya
A custom firmware to bring MQTT to XR809 SOC powered door and widow sensors. (like the tuya XR3-Module)

<img src="/doc/assets/Tuya_Device.jpg?raw=true" height="250" alt="Overview shot of sensor with packaging" title="Overview shot of sensor with packaging"> <img src="/doc/assets/Tuya_Device_detail.jpg?raw=true" height="250" alt="Detail shot of sensor pcb" title="Detail shot of sensor pcb">

## Introduction
Some time ago I bought some tuya wifi door/window sensors to extend my DIY smart home. I hoped to find some real smart, offline-able, easy to integrate or ESP based Devices. But nope. That stuff only talks encrypted to the vendor cloud. Opening one up revealed that it isn’t ESP powered. So no way to get one of those awesome custom firmware on it.  Even localtuya wont work, since polling for the state not practical. The only way would be to use the API of the vendor cloud. But this dependence with all the drawbacks (privacy, availability, required internet access) prevented me from installing thous sensors.

Than I discovered [this article](https://www.elektroda.com/rtvforum/topic3806769.html) by p.kaczmarek2 about how to develop software for the XR809 SOC. That SOC is used by the XR3-Module from tuya. Some progress was posted there, but no ready to use solution has arisen. (AFAIK) So here I am with my own firmware.

I am not an experienced MCU or C developer. My primary objective was just to get it working for me. I give no warranty for this piece of software. Documenting this already took a lot of time and I have no time for any substantial maintenance or further development. Probably there are lots of issues with this firmware. Feel free to use, improve and share.

## Getting started
In this section teaches how to configure and flash the untuya firmware with the precompiled binaries. If you are interested in building the firmware from the source, check the Build cheater below. Anyway, you have to clone this project with git and initialize the SOC sdk that is referenced as an submodule. This can be done in one step with `git clone --recurse-submodules https://github.com/th-wilde/untuya.git`.

You need also:
* A 3,3V TTL uart/com adapter
* A x86 Windows/Linux machine
  * since the image- and flashing-tool mkimage[.exe] and phoenixMC[.exe] from the XR903-sdk are only distributed as executables

### Configure the firmware
The firmware currently can not be configured at runtime. The configuration is hard coded in the binaries of the firmware. To make life easier, I provide precompiled binaries that can be modified with a simple offline web-form. 

The “build_output” directory contains a prebuild firmware. To configure the firmware for your needs, use the “Configure_Firmware.html”. That will allow you to patch your configuration hardcoded into the binary files (open `untuya.bin` and save it as `app.bin`) of the firmware. After that use the `./chreate_image.sh`/` chreate_image.cmd` to create a flashable firmware image.

That image (xr_system.img) can than be flashed with the `./phoenixMC`/`phoenixMC.exe` to the SOC.

<img src="/doc/assets/Firmware-Configuration.PNG?raw=true" height="250" alt="Screenshot of the configuration via offline web-form" title="Screenshot of the configuration via offline web-form">

### Flash the Firmware
Wire up the SOC like shown below. The SOC can be programmed via a UART interface (TXD0 and RXD0). The VCC is 3,3 volts. Any usual TTL adapter should do the job. Just ensure the VCC of the adapter is also 3,3 volts.

To flash a firmware, the SOC must be put into a programming mode. This is done similar to an ESP. Here the two GPIO-Ports PB02 and PB03 need to be connected to ground while powering on the SOC.

On Linux the `phoenixMC`-flashing-tool reads its configuration from the `settings.ini` file additional to it’s command line parameters. Just enter the `build_output` and run the tool from there. Maybe you need to provide the right device-file (for example `/dev/ttyUSB0`) via the command line parameters or by editing the `settings.ini`.

On Windows the `phoenixMC.exe`-flashing-tool comes with a graphical interface in chinese localization. It’s not crazy complicated to use. The picture below should speak for itself.

After flashing just cycle the power to restart the SOC.

<img src="/doc/assets/Wiring.png?raw=true" height="250" alt="Wiring diagram for flashing" title="Wiring diagram for flashing"> <img src="/doc/assets/Arrangement.jpg?raw=true" height="250" alt="Picture of arrangement for flashing" title="Picture of arrangement for flashing">
<img src="/doc/assets/FlashWin.png?raw=true" height="250" alt="Screenshot of the flashing tool on windows" title="Screenshot of the flashing tool on windows">

### Usage
On powering up and input (open/close) it will connect to the WIFI, publish it’s state to `tele/[unique_device_name]/state` on the MQTT broker and shutdown. It’s that simple. And if you using Home Assistant, that the device should be automatically found due to the mqtt discovery feature.

## Troubleshooting
Some common issues I encountered:

Flashing is not starting or is aborted:

Make sure that the SOC is in programming mode. This can be tricky when the SOC has shut down to save power. Then a **short** power cycle wont work since the power consumption is so low that the capacities can buffer the outage for quiet some time. Wait longer or wake the SOC up by a signal (wakeup io, see next chapter) it that is not the problem try a lower transmission rate (baut rate) on the UART interface. The default of 921600 was not stable for me. 115200 worked fine.

## How the sensor works
The door/window sensor is a battery powered device. Therefor power saving measures are essential to get a reasonable battery life. The SOC is powered down almost all of the time. Only on an input (opening/closing) the SOC is started to communicate the change.

My model uses a hall effect sensor that is connected to PA19. This is an GPIO-Port that also can be configured as a wakeup-io when the soc is shut down. This allows to use one and the same GPIO-Port to manage the wake up and to signal the open/close state. There is also a push button and a LED on the device, but I haven’t checked how they are wired. 

The SOC will also power on periodically and updates its state topic. This is used as a heartbeat to allow detection of a failed sensor. (probably an empty battery)

## How to build the firmware from source code
I used a x86 Debian 11 (Bullseye) system as the build environment. Follow the following steps to set it up: 

1. Clone the repository with git, like written before. 
   * `git clone --recurse-submodules https://github.com/th-wilde/untuya.git`
1. install the following packages: `gcc-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib`, `libnewlib-arm-none-eabi`, `libnewlib-dev`, `gcc-arm-none-eabi` 
   * `sudo apt install gcc-arm-none-eabi libstdc++-arm-none-eabi-newlib libnewlib-arm-none-eabi libnewlib-dev gcc-arm-none-eabi`
1. patch some header-files of the toolchain by running `./patch_toolchain.sh` as root
   * Patch the toolchain to use (u)int as (u)int32_t. The SDK tries to redefine the c# precompiler macros. But this seams not to work. Patch_toolchan.sh will do the job. Run it as root (`sudo ./patch_toolchain.sh`), since the files needed to change are part of the previously installed packages.
1. Run `./build_binaries.sh` to build SDK and untuya. The binaries of the firmware will be written to the `build_output` folder.
1. Configure, create and flash the image like written before.

