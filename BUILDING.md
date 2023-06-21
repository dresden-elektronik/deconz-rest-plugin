# Building

## Supported platforms
* Raspbian ~~Jessie~~, ~~Stretch~~, Buster and Bullseye
* Ubuntu ~~Xenial~~, Bionic, Focal Fossa and Jammy
* Windows 7, 10, 11

There are ways to build the plugin:
1. [CMake](#build-with-cmake)
2. [QMake (deprecated)](#build-with-qmake)

## Build with CMake

CMake is the new build system to compile the REST-API plugin. The former `deconz-dev` package isn't needed anymore since the headers and sources of the deCONZ library are pulled from https://github.com/dresden-elektronik/deconz-lib automatically.

### Linux

On Debian/Ubuntu the following development packages need to be installed:

```
apt-get update && \
apt-get install --no-install-recommends -y \
lsb-release ca-certificates build-essential pkg-config qt5-default git \
libqt5serialport5-dev  libqt5websockets5-dev qtdeclarative5-dev  \
sqlite3 libsqlite3-dev libgpiod-dev libssl-dev curl cmake ninja-build
```

1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Checkout the latest version from `cmake_1` branch

        cd deconz-rest-plugin
        git checkout -b cmake_1 origin/cmake_1

3. Compile the plugin

        cmake -DCMAKE_INSTALL_PREFIX=/usr -G Ninja -S . -B build
        cmake --build build
   
4. Install in local temporary directory
   (This step changes the RPATH so the plugin can find the official `/usr/lib/libdeCONZ.so` library.)

        cmake --install build --prefix tmp

The compiled plugin can be found in current directory `tmp/share/deCONZ/plugins`.

### Windows

MYSYS2 MINGW32 needs to be installed.

Open the mingw32 shell from start menu.


### Build

1. Checkout this repository

2. Open "x86 Native Tools Command Promt for VS 2022" via Windows Start Menu

3. Navigate to the source directory, e.g. `cd C:\gcfflasher` 

3. Compile the executable with CMake

```
cmake -S . -B build
cmake --build build --config Release
```

### macOS

(not officially supported yet, requires core sources for macOS build)

Install dependencies via Homebrew.
```
brew install qt@5 ninja cmake
```

```
cmake -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt\@5/5.15.9/lib/cmake  -S . -B build
```

## Build with QMake

**Important:** Building via  QMake is deprecated and will be removed in future.

This method is only supported on Linux.

### Install deCONZ development package

    sudo apt install deconz-dev

### Get and compile the plugin

1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Checkout the latest version

        cd deconz-rest-plugin
        git checkout -b mybranch HEAD

3. Compile the plugin

        qmake && make -j2

**Note** On Raspberry Pi 1 use `qmake && make`

4. Replace original plugin

        sudo cp ../libde_rest_plugin.so /usr/share/deCONZ/plugins
