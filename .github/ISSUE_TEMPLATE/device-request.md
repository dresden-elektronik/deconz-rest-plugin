---
name: Device Request
about: Request a new device to be supported.
title: "Device name"
labels: Device Request
assignees: ''

---

<!--
  - Before requesting a device, please make sure to search the open and closed issues for any requests in the past.
  - Sometimes devices have been requested before but are not implemented yet due to various reasons.
  - If there are no hits for your device, please proceed.
  - If you're unsure whether device support was already requested, please ask for advise in our Discord chat: https://discord.gg/QFhTxqN
-->

## Device

- Product name: The device name as shown on the product or package.
- Manufacturer: As per deCONZ GUI Basic cluster.
- Model identifier: As per deCONZ GUI Basic cluster.
- Device type : Please remove all unrelated device types. 
  - Light
  - Lock
  - Remote
  - Sensor
  - Siren
  - Switch
  - Thermostat
  - Other: 

<!--
  Please refer to https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Request-Device-Support
  on how the Basic Cluster attributes are obtained.
-->

## Screenshots

<!--
  Screenshots help to identify the device and its capabilities. Please refer to:
  https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Request-Device-Support
  for examples of the required screenshots.

  Required screenshots:
  - Endpoints and clusters of the node
  - Node Info panel

  In the Cluster Info panel press "read" button to retreive the values. Please note that at least "Manufacturer Name" and "Model Identifier" must be populated with data (therefore, must not be empty), otherwise that information will not be usable. For battery powered devices, after pressing read it is required to wake-up the device by pressing a button or any other means of interaction.
-->

<!--
  If available add screenshots of other clusters. Please Remove sections that are not available on your device.

  Relevant clusters are: Simple Metering, Electrical Measurement, Power Configuration, Thermostat, etc. You can typically spare Identify, Alarms, Device Temperature, On/Off. Please ensure data has been read prior to taking any screenshots.
-->

### Basic

### Identify

### Alarms

### Device Temperature

### Groups

### Scenes

### On/Off

### Level Control

### Color Control

### Simple Metering

### Diagnostics

### Other clusters that are not mentioned above
