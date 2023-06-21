# Building

### Supported platforms
* Raspbian ~~Jessie~~, ~~Stretch~~, Buster and Bullseye
* Ubuntu ~~Xenial~~, Bionic, Focal Fossa and Jammy
* Windows 7, 10, 11

There are ways to build the plugin:
1. [CMake](#build-with-cmake)
2. [QMake (deprecated)](#build-with-qmake)

### Build with CMake

CMake is the new build system to compile the REST-API plugin. The former `deconz-dev` package isn't needed anymore since the headers and sources of the deCONZ library are pulled from https://github.com/dresden-elektronik/deconz-lib

Building works on all platforms not only Linux.


**TODO** note the required code is currently only in my personal  repository.

**TODO** list build dependencies for various platforms (CMake, Ninja, …).

**TODO** describe setup for IDEs like Visual Studio and QtCreator.

#### Linux

1. Checkout the repository

        git clone https://github.com/manup/deconz-rest-plugin.git

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

#### macOS

(not officially supported yet, requires core sources for macOS build)

Install dependencies via Homebrew.
```
brew install qt@5 ninja cmake
```

```
cmake -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt\@5/5.15.9/lib/cmake  -S . -B build
```

### Build with QMake

**Important:** Building via  QMake is deprecated and will be removed in future.

This method is only supported on Linux.

#### Install deCONZ development package

    sudo apt install deconz-dev

#### Get and compile the plugin

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
