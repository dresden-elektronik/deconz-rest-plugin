Introduction
============

The [deCONZ](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1) REST plugin provides a REST API to access ZigBee Home Automation (ZHA) and ZigBee Light Link (ZLL) lights, switches and sensors like the dresden elektronik [Wireless Light Control](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control) system, IKEA TRÅDFRI and Philips Hue.

As hardware the [RaspBee](https://www.dresden-elektronik.de/raspbee?L=1&ref=gh) ZigBee Shield for Raspberry Pi or a [ConBee](https://www.dresden-elektronik.de/conbee?L=1&ref=gh) USB dongle is used to directly communicate with the ZigBee devices.

To learn more about the REST API itself please visit the [REST API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/) page.

License
=======
The plugin is available as open source and licensed under the BSD (3-Clause) license.

Usage
=====

Currently the compilation of the plugin is only supported for Raspbian Jessie distribution.
Packages for Qt4 and Raspbian Wheezy are available but not described here.

##### Install deCONZ and development package
1. Download deCONZ package

        wget http://www.dresden-elektronik.de/rpi/deconz/beta/deconz-2.04.79-qt5.deb

2. Install deCONZ package

        sudo dpkg -i deconz-2.04.79-qt5.deb

3. Install missing dependencies

        sudo apt install -f

4. Download deCONZ development package

        wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-2.04.79.deb

5. Install deCONZ development package

        sudo dpkg -i deconz-dev-2.04.79.deb

##### Get and compile the plugin
1. Checkout the repository

        git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git

2. Checkout related version tag

        cd deconz-rest-plugin
        git checkout -b mybranch V2_04_79

3. Compile the plugin

        qmake && make -j2

**Note** On Raspberry Pi 1 use `qmake && make`

4. Replace original plugin

        sudo cp ../libde_rest_plugin.so /usr/share/deCONZ/plugins

Software requirements
---------------------
* Raspbian Jessie or Stretch and Qt5

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
