### Compatibility



In order for the device to work properly, the [firmware](https://github.com/Koenkk/zigbee-OTA/raw/master/images/Terncy/TERNCY-SD01_v46.OTA) 
must be updated, as in earlier versions, there is no manufacturer name included. Additionally, it must be ensured that deconz exposes 
endpoint 6E through the coordinator. Please see [this](https://github.com/dresden-elektronik/deconz-rest-plugin/issues/4728#issuecomment-860883260) 
post for guidance.