## TruePrompter
![build](https://github.com/ayles/TruePrompter/actions/workflows/build.yaml/badge.svg)

Teleprompter server with speech recognition

## Build
```
sudo apt install libopenblas-dev libboost-all-dev libssl-dev libwebsocketpp-dev protobuf-compiler ninja-build
git clone https://github.com/ayles/TruePrompter.git
cd TruePrompter
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=Ninja ..
cmake --build . --target TruePrompter
cd ..
```

**NOTE**: building without specified target will not work

## Build (for Raspberry PI)

### Mounting & preparing chroot

```
sudo apt install qemu qemu-user-static qemu-utils binfmt-support kpartx
mkdir raspi_files && cd raspi_files
wget http://downloads.raspberrypi.org/raspios_lite_arm64/images/raspios_lite_arm64-2021-05-28/2021-05-07-raspios-buster-arm64-lite.zip
unzip 2021-05-07-raspios-buster-arm64-lite.zip
qemu-img resize -f raw 2021-05-07-raspios-buster-arm64-lite.img 16G

fdisk 2021-05-07-raspios-buster-arm64-lite.img
# commands: p -> d -> 2 -> n -> p -> 2
# then, enter first sector as from first p command of partition 2
# then, use default last sector value
# then, press N to not delete the ext4 signature

sudo kpartx -a -v 2020-05-27-raspios-buster-lite-armhf.img
sudo resize2fs /dev/mapper/loop0p2
mkdir disk/
sudo mount /dev/mapper/loop0p2 ./disk
sudo cp /usr/bin/qemu-aarch64-static ./disk/usr/bin
sudo mount -o bind /dev ./disk/dev
sudo mount -o bind /proc ./disk/proc
sudo mount -o bind /sys ./disk/sys
sudo update-binfmts --enable qemu-aarch64
sudo chroot ./disk
```

**NOTE**: `/dev/mapper/loop0p2` can be any `/dev/mapper/loopXp2`, use one from output of `kpartx` command

### Building

Building is same as above, except you need to install git & cmake on fresh system
```
sudo apt update
sudo apt install git
# Install fresh cmake for linux aarch64 from https://cmake.org/download/
cd /home/pi
... build commands ...
```

## Usage
```
build/src/TruePrompter 8080 small_model
```
