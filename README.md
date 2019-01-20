Introduction
============

The [deCONZ](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1) REST plugin provides a REST API to access ZigBee Home Automation (ZHA) and ZigBee Light Link (ZLL) lights, switches and sensors like the dresden elektronik [Wireless Light Control](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control) system, Xiaomi Aqara, IKEA TRÅDFRI and Philips Hue.

As hardware the [RaspBee](https://www.dresden-elektronik.de/raspbee?L=1&ref=gh) ZigBee Shield for Raspberry Pi or a [ConBee](https://www.dresden-elektronik.de/conbee?L=1&ref=gh) USB dongle is used to directly communicate with the ZigBee devices.

To learn more about the REST API itself please visit the [REST API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/) page.

### Phoscon App
The *Phoscon App* is the successor of the 2016 WebApp (Wireless Light Control), it's browser based and supports more sensors and switches. For more information and screenshots check out:

* [Phoscon App Documentation](https://doc.phoscon.de/app/doc.html?ref=gh)
* [Phoscon App Description](https://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/phoscon-app?L=1&ref=gh)


Precompiled deCONZ packages
===========================

The deCONZ application is available for the following platforms and contains the main application and the pre-compiled REST-API plugin.

* Windows  https://www.dresden-elektronik.de/deconz/win/
* Raspbian https://www.dresden-elektronik.de/deconz/raspbian/beta/
* Ubuntu 64-bit https://www.dresden-elektronik.de/deconz/ubuntu/beta/

Installation Raspberry Pi
=========================

##### Supported platforms
* Raspbian Jessie
* Raspbian Stretch

##### Supported devices

A uncomplete list of supported devices can be found here:

https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Supported-Devices

### Install deCONZ

**Important** If you're updateing from a previous version **always make sure to create an backup** in the Phoscon App and read the changelog first.

https://github.com/dresden-elektronik/deconz-rest-plugin/releases

1. Download deCONZ package

        wget http://www.dresden-elektronik.de/rpi/deconz/beta/deconz-2.05.57-qt5.deb

2. Install deCONZ package

        sudo dpkg -i deconz-2.05.57-qt5.deb

**Important** this step might print some errors *that's ok* and will be fixed in the next step.

3. Install missing dependencies

        sudo apt update
        sudo apt install -f

##### Install deCONZ development package (optional)

The deCONZ package already contains the REST API plugin, the development package is only needed if you wan't to modify the plugin or try the latest commits from master branch.

1. Download deCONZ development package

        wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-2.05.57.deb

2. Install deCONZ development package

        sudo dpkg -i deconz-dev-2.05.57.deb

3. Install missing dependencies

        sudo apt update
        sudo apt install -f

##### Get and compile the plugin
1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Checkout related version tag

        cd deconz-rest-plugin
        git checkout -b mybranch V2_05_57

3. Compile the plugin

        qmake && make -j2

**Note** On Raspberry Pi 1 use `qmake && make`

4. Replace original plugin

        sudo cp ../libde_rest_plugin.so /usr/share/deCONZ/plugins

Headless support
----------------

The beta version contains a systemd script, which allows deCONZ to run without a X11 server.

1. Enable the service at boot time

```bash
$ sudo systemctl enable deconz
```

2. Disable deCONZ GUI Autostart

The dresden elektronik Raspbian sd-card image autostarts deCONZ GUI.

```bash
$ sudo systemctl disable deconz-gui
$ sudo systemctl stop deconz-gui
```

On older versions of deCONZ this can be done by removing the X11 Autostart file.

```bash
$ rm -f /home/pi/.config/autostart/deCONZ.desktop
```


Software requirements
---------------------
* Raspbian Jessie or Raspbian Stretch with Qt5

**Important** The serial port must be configured as follows to allow communication with the RaspBee.

    $ sudo raspi-config

    () Interfacting Options > Serial

        * Would you like a login shell accessible over serial?
          > No
        * Would you like the serial port hardware to be enabled?
          > Yes

After changing the settings reboot the Raspberry Pi.


Hardware requirements
---------------------

* Raspberry Pi 1, 2 or 3
* [RaspBee](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/raspbee?L=1) ZigBee Shield for Raspberry Pi
* [ConBee](https://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/conbee/?L=1) USB dongle for Raspberry Pi and PC

3rd party libraries
-------------------
The following libraries are used by the plugin:

* [SQLite](http://www.sqlite.org)
* [qt-json](https://github.com/lawand/droper/tree/master/qt-json)
* [colorspace](http://www.getreuer.info/home/colorspace)

License
=======
The plugin is available as open source and licensed under the BSD (3-Clause) license.

