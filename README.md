Introduction
============

The [deCONZ](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1) REST plugin provides a REST-API to access Zigbee 3.0 (Z30), Zigbee Home Automation (ZHA) and Zigbee Light Link (ZLL) lights, switches and sensors from Xiaomi Aqara, IKEA TRÅDFRI, Philips Hue, innr, Samsung and many more vendors.

A list of supported Zigbee devices can be found on the [Supported Devices](https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Supported-Devices) page.

As hardware the [RaspBee](https://phoscon.de/raspbee?ref=gh) Zigbee Shield for Raspberry Pi, a [ConBee](https://phoscon.de/conbee?ref=gh) or [ConBee&nbsp;II](https://phoscon.de/conbee2?ref=gh) USB-dongle is used to communicate with Zigbee devices.

To learn more about the REST-API itself please visit the [REST-API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/) page.

### Phoscon App
The Phoscon App is browser based and supports lights, sensors and switches. For more information and screenshots check out:

* [Phoscon App Documentation](https://phoscon.de/app/doc?ref=gh)
* [Phoscon App Description](https://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/phoscon-app?L=1&ref=gh)

Installation
============

##### Supported platforms
* Raspbian Jessie, Stretch and Buster
* Ubuntu Xenial and Bionic (AMD64)
* Windows 7 and 10

### Install deCONZ
You find the instructions for your platform and device on the Phoscon website:

* [RaspBee](https://phoscon.de/raspbee/install?ref=gh)
* [ConBee](https://phoscon.de/conbee/install?ref=gh)
* [ConBee&nbsp;II](https://phoscon.de/conbee2/install?ref=gh)

**Important:** If you're updating from a previous version **always make sure to create an backup** in the Phoscon App and read the changelog first.

https://github.com/dresden-elektronik/deconz-rest-plugin/releases

### Install deCONZ development package (optional, Linux only)

**Important:** The deCONZ package already contains the REST-API plugin, the development package is **only** needed if you wan't to modify the plugin or try the latest commits from master branch.

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

Precompiled deCONZ packages for manual installation
===================================================

The deCONZ application packages are available for the following platforms and contain the main application and the pre-compiled REST-API plugin.

* Windows  https://www.dresden-elektronik.de/deconz/win/
* Raspbian https://www.dresden-elektronik.de/deconz/raspbian/beta/
* Ubuntu 64-bit https://www.dresden-elektronik.de/deconz/ubuntu/beta/

Headless support for Linux
--------------------------

The deCONZ package contains a systemd script, which allows deCONZ to run without a X11 server.

1. Enable the service at boot time

```bash
$ sudo systemctl enable deconz
```

2. Disable deCONZ GUI autostart service

The dresden elektronik sd-card image and default installation method autostarts deCONZ GUI.
The following commands disable the deCONZ GUI service:

```bash
$ sudo systemctl disable deconz-gui
$ sudo systemctl stop deconz-gui
```

Hardware requirements
---------------------

* Raspberry Pi 1, 2B, 3B, 3B+ or 4B
* [RaspBee](https://phoscon.de/raspbee?ref=gh) Zigbee Shield for Raspberry Pi
* [ConBee](https://phoscon.de/conbee?ref=gh) USB-dongle for Raspberry Pi and PC
* [ConBee&nbsp;II](https://phoscon.de/conbee2?ref=gh) USB-dongle for Raspberry Pi and PC

3rd party libraries
-------------------
The following libraries are used by the plugin:

* [SQLite](http://www.sqlite.org)
* [qt-json](https://github.com/lawand/droper/tree/master/qt-json)
* [colorspace](http://www.getreuer.info/home/colorspace)

License
=======
The plugin is available as open source and licensed under the BSD (3-Clause) license.

