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
  - If there are no hits on your for the device, please proceed.
  - If you're unsure if the device support is already requested, please ask for advise in our Discord chat: https://discord.gg/QFhTxqN
-->

## Device

- Product name: The device name as shown on the product or package.
- Device Type: (Light / Lock / Remote / Sensor / Siren / Thermostat)
- Manufacturer: As per deCONZ GUI Basic cluster.
- Model identifier: As per deCONZ GUI Basic cluster.

<!--
  Please refer to https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Request-Device-Support
  on how the Basic Cluster attributes are optained.
-->

## Screenshots

<!--
  Screenshots help to identify the device and it's capabilities. Please refer to:
  https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/Request-Device-Support
  for examples of the required screenshots.

  Required screenshots:
  - Endpoints and clusters of the node
  - Node Info panel
  - Basic Cluster attributes in the Cluster Info panel.

  In the Cluster Info panel press "read" button to retreive the values. Please note that at least "Manufacturer Name" and "Model Identifier" must be populated with data (therefore, must not be empty), otherwise that information will not be usable. For battery powered devices, after pressing read it is required to wake-up the device by pressing a button or any other means of interaction.
-->

<!--
  If available add screenshots of other clusters.

  Relevant clusters are: Simple Metering, Electrical Measurement, Power Configuration, Thermostat, etc. You can typically spare Identify, Alarms, Device Temperature, On/Off. Please ensure data has been read prior to taking any screenshots.
-->
