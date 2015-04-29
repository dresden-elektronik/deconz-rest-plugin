Introduction
============

The [deCONZ](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1) REST plugin provides a REST API to access ZigBee Home Automation (HA) and ZigBee Light Link (ZLL) lights like dresden elektroniks [Wireless Light Control](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control) system and Philips Hue.

As hardware the [RaspBee](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/raspbee?L=1) ZigBee Shield for Raspberry Pi is used to directly communicate with the ZigBee devices.

To learn more about the REST API itself please visit the [REST API Documentation](http://dresden-elektronik.github.io/deconz-rest-doc/) page.

License
=======
The plugin is available as open source and licensed under the BSD (3-Clause) license.


Usage
=====

Currently the compilation of the plugin is only supported within the Raspbian distribution.

##### Install deCONZ and development package
1. Download deCONZ package

  `wget http://www.dresden-elektronik.de/rpi/deconz/deconz-latest.deb`

2. Install deCONZ package

  `sudo dpkg -i deconz-latest.deb`
  
3. Download deCONZ development package

  `wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-latest.deb`

4. Install deCONZ development package

  `sudo dpkg -i deconz-dev-latest.deb`

##### Get and compile the plugin
1. Checkout the repository

  `git clone https://github.com/dresden-elektronik/deconz-rest-plugin.git`
 
2. If you haven't already, install qt4

  `sudo apt-get install libqt4-dev libqt4-core`

3. Compile the plugin

  `cd deconz-rest-plugin`

  `qmake-qt4 && make`

Hardware requirements
---------------------

* Raspberry Pi
* [RaspBee](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/raspbee?L=1) ZigBee Shield for Raspberry Pi
* or a deRFusb23e0x wireless USB dongle

3rd party libraries
-------------------
The following libraries are used by the plugin:

* [SQLite](http://www.sqlite.org)
* [qt-json](https://github.com/lawand/droper/tree/master/qt-json)
* [colorspace](http://www.getreuer.info/home/colorspace)
