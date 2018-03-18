Introduction
============

The [deCONZ](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1) REST plugin provides a REST API to access ZigBee Home Automation (ZHA) and ZigBee Light Link (ZLL) lights, switches and sensors like the dresden elektronik [Wireless Light Control](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control) system, IKEA TRÅDFRI and Philips Hue.

As hardware the [RaspBee](https://www.dresden-elektronik.de/raspbee?L=1&ref=gh) ZigBee Shield for Raspberry Pi or a [ConBee](https://www.dresden-elektronik.de/conbee?L=1&ref=gh) USB dongle is used to directly communicate with the ZigBee devices.

To learn more about the REST API itself please visit the [REST API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/) page.

### Phoscon App Beta
The *Phoscon App* is the successor of the current WebApp (Wireless Light Control), it's browser based too and in open beta state, for more information and screenshots check out:

https://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/phoscon-app?L=1

Development updates are posted here:

https://github.com/dresden-elektronik/phoscon-app-beta

Installation
============

##### Supported platforms
* Raspbian Jessie
* Raspbian Stretch

Raspbian Wheezy and Qt4 is no longer maintained.

##### Supported devices

A uncomplete list of supported devices can be found here:

https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Supported-Devices

### Install deCONZ
1. Download deCONZ package

        wget http://www.dresden-elektronik.de/rpi/deconz/beta/deconz-2.05.14-qt5.deb

2. Install deCONZ package

        sudo dpkg -i deconz-2.05.14-qt5.deb

**Important** this step might print some errors *that's ok* and will be fixed in the next step.

3. Install missing dependencies

        sudo apt update
        sudo apt install -f

##### Install deCONZ development package (optional)

The deCONZ package already contains the REST API plugin, the development package is only needed if you wan't to modify the plugin or try the latest commits from master branch.

1. Download deCONZ development package

        wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-2.05.14.deb

2. Install deCONZ development package

        sudo dpkg -i deconz-dev-2.05.14.deb

3. Install missing dependencies

        sudo apt update
        sudo apt install -f

##### Get and compile the plugin
1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Checkout related version tag

        cd deconz-rest-plugin
        git checkout -b mybranch V2_05_14

3. Compile the plugin

        qmake && make -j2

**Note** On Raspberry Pi 1 use `qmake && make`

4. Replace original plugin

        sudo cp ../libde_rest_plugin.so /usr/share/deCONZ/plugins

Headless support
----------------

The beta version contains a systemd script, which allows deCONZ to run without a X11 server.

**Note** The service does not yet support deCONZ updates via WebApp, therefore these must be installed manually. A further systemd script will handle updates in future versions.

1. Enable the service at boot time

```bash
$ sudo systemctl enable deconz
```

2. Disable X11 deCONZ autostart script

The dresden elektronik Raspbian sd-card image contains a autostart script for X11 which should be removed.

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

