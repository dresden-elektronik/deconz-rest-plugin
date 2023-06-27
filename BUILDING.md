# Building

## Supported platforms
* Raspbian / Debian ~~Jessie~~, ~~Stretch~~, Buster, Bullseye and Bookworm
* Ubuntu ~~Xenial~~, Bionic, Focal Fossa and Jammy
* Windows 7, 10, 11

There are two ways to build the plugin:
1. [CMake](#build-with-cmake)
2. [QMake (deprecated)](#build-with-qmake)

## Build with CMake

CMake is the new build system to compile the REST-API plugin. The former `deconz-dev` package isn't needed anymore since the headers and sources of the deCONZ library are pulled from https://github.com/dresden-elektronik/deconz-lib automatically.

### Linux

On Debian Buster the following development packages need to be installed.

**Note:** On newer Ubuntu versions `qt5-default` isn't available, replace it with `qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools` instead.

```
apt-get update && \
apt-get install --no-install-recommends -y \
qt5-default \
lsb-release ca-certificates build-essential pkg-config git \
libqt5serialport5-dev  libqt5websockets5-dev qtdeclarative5-dev  \
sqlite3 libsqlite3-dev libgpiod-dev libssl-dev curl cmake ninja-build
```

1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Compile the plugin

        cmake -DCMAKE_INSTALL_PREFIX=/usr -G Ninja -B build
        cmake --build build
   
3. Install in local temporary directory
   (This step changes the RPATH so the plugin can find the official `/usr/lib/libdeCONZ.so` library.)

        cmake --install build --prefix tmp

The compiled plugin is located at: `tmp/share/deCONZ/plugins/libde_rest_plugin.so`

### Windows

MSYS2 MINGW32 needs to be installed, it can be downloaded from https://msys2.org 


In MSYS2 MINGW32 shell the following packages need to be installed:

```
pacman -Sy mingw-w64-i686-qt5 mingw-w64-i686-openssl mingw-w64-i686-sqlite3
```

### Build

1. Checkout this repository

2. Open MSYS2 MINGW32 shell via Windows Start Menu

3. Navigate to the source directory, e.g. `cd /c/src/deconz-rest-plugin` 

3. Compile the plugin with CMake

```
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw32/lib/cmake -B build
cmake --build build
```

After compilation the plugin can be found in the build directory: `de_rest_plugin.dll`

### macOS

(not officially supported yet, requires core sources for macOS build)

Install dependencies via Homebrew.
```
brew install qt@5 ninja cmake
```

```
cmake -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt\@5/5.15.9/lib/cmake -B build
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
