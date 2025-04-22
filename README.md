Introduction
============

The deCONZ REST plugin provides a REST-API to access Zigbee 3.0 (Z30), Zigbee Home Automation (ZHA) and Zigbee Light Link (ZLL) lights, switches and sensors from Xiaomi Aqara, IKEA TRÅDFRI, Philips Hue, innr, Samsung and many more vendors.

A list of supported Zigbee devices can be found on the [Supported Devices](https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Supported-Devices) page.

To communicate with Zigbee devices the [RaspBee](https://phoscon.de/raspbee?ref=gh) / [RaspBee&nbsp;II](https://phoscon.de/raspbee2?ref=gh) Zigbee shield for Raspberry Pi, or a [ConBee](https://phoscon.de/conbee?ref=gh) / [ConBee&nbsp;II](https://phoscon.de/conbee2?ref=gh) / [ConBee&nbsp;III](https://phoscon.de/conbee3?ref=gh) USB dongle is required.

### API Documentation

* [REST-API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/)
* [deCONZ C++ Plugin API Documentation](https://phoscon.de/deconz-cpp).
* [DDF and C++ Device API Documentation](https://dresden-elektronik.github.io/deconz-dev-doc)

For community based support with deCONZ or Phoscon, please visit the [deCONZ Discord server](https://discord.gg/QFhTxqN). 

### Phoscon App
The Phoscon App is a browser based web application and supports lights, sensors and switches. For more information and screenshots visit the [Phoscon App Documentation](https://phoscon.de/app/doc?ref=gh).


### Release Schedule

deCONZ beta releases are scheduled roughly once per week. After 1–3 betas a stable version is released and a new beta cycle begins.

Current Beta: **v2.29.5-beta**  
Current Stable: **v2.29.5**

Next Beta: **v2.30.0-beta** Expected in April.
Next Stable: **v2.30.x** Expected in May.

Installation
============

##### Supported platforms
* Raspbian ~~Jessie~~, ~~Stretch~~, Buster, Bullseye and Bookworm
* Ubuntu ~~Xenial~~, Bionic, Focal Fossa and Jammy
* Windows 7, 10, 11

### Install deCONZ
You find the instructions for your platform and device on the Phoscon website:

* [RaspBee](https://phoscon.de/raspbee/install?ref=gh)
* [RaspBee&nbsp;II](https://phoscon.de/raspbee2/install?ref=gh)
* [ConBee](https://phoscon.de/conbee/install?ref=gh)
* [ConBee&nbsp;II](https://phoscon.de/conbee2/install?ref=gh)
* [ConBee&nbsp;III](https://phoscon.de/conbee3/install?ref=gh)

**Important:** If you're updating from a previous version **always make sure to create an backup** in the Phoscon App and read the changelog first.

https://github.com/dresden-elektronik/deconz-rest-plugin/releases

### Compiling the plugin

The build instructions are described in [BUILDING.md](BUILDING.md).

Precompiled deCONZ packages for manual installation
===================================================

The deCONZ application packages are available for the following platforms and contain the main application and the pre-compiled REST-API plugin.

* Windows  http://deconz.dresden-elektronik.de/win/
* Raspbian http://deconz.dresden-elektronik.de/raspbian/beta/
* Ubuntu and Debian 64-bit http://deconz.dresden-elektronik.de/ubuntu/beta/
* ARM64 systems http://deconz.dresden-elektronik.de/debian/beta/

To manually install a Linux .deb package enter these commands:

    sudo dpkg -i <package name>.deb
    sudo apt-get install -f

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
* [RaspBee](https://phoscon.de/raspbee?ref=gh) Zigbee shield for Raspberry Pi
* [RaspBee&nbsp;II](https://phoscon.de/raspbee2?ref=gh) Zigbee shield for Raspberry Pi
* [ConBee](https://phoscon.de/conbee?ref=gh) USB dongle for Raspberry Pi and PC
* [ConBee&nbsp;II](https://phoscon.de/conbee2?ref=gh) USB dongle for Raspberry Pi and PC
* [ConBee&nbsp;III](https://phoscon.de/conbee3?ref=gh) USB dongle for Raspberry Pi and PC

3rd party libraries
-------------------
The following libraries are used by the plugin:

* [ArduinoJSON](https://arduinojson.org)
* [SQLite](http://www.sqlite.org)
* [qt-json](https://github.com/lawand/droper/tree/master/qt-json)
* [colorspace](http://www.getreuer.info/home/colorspace)

License
=======
The plugin is available as open source and licensed under the BSD (3-Clause) license.

