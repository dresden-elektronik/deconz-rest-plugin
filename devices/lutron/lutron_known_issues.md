### Compatibility

The Lutron Aurora only works well with the Hue bridge.
Its latest firmware, v3.8, won't work al all with the deCONZ REST API.
You need to downgrade the firmware to
[v3.4](http://fds.dc1.philips.com/firmware/ZGB_1144_0000/3040/Superman_v3_04_Release_3040.ota).
Even then, the Aurora won't issue reports from the Hue-specific cluster FC00.
Consequently, the REST API cannot generate advanced button and rotary events.
