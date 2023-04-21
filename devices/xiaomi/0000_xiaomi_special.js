/* global R, ZclFrame, log10 */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x00F7 || attrid === 0x01FF || attrid === 0xFF01) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  var i = status === 0 ? 4 : 3
  if (dt === 0x41) { // ostring
    var len = ZclFrame.at(i)
    i++
    while (len > 0) {
      const dt = ZclFrame.at(i + 1)
      const tag = ZclFrame.at(i) << 8 | dt
      i += 2
      len -= 2
      var val
      switch (dt) {
        case 0x10: // bool
        case 0x20: // uint8
        case 0x28: // int8
          val = ZclFrame.at(i)
          i += 1
          len -= 1
          break
        case 0x21: // uint16
        case 0x29: // int16
          val = ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
          i += 2
          len -= 2
          break
        case 0x23: // uint32
        case 0x2B: // int32
          val = ZclFrame.at(i + 2) << 24 | ZclFrame.at(i + 2) << 16 |
            ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
          i += 4
          len += 4
          break
        case 0x39: // float
          // TODO decode float info Number
          i += 4
          len += 4
          break
        case 0x24: // uint40
          // JavaScript only does bitwise operators up to 32-bit
          i += 5
          len -= 5
          break
        case 0x25: // uint48
          // JavaScript only does bitwise operators up to 32-bit
          i += 6
          len -= 6
          break
        case 0x27: // uint64
          // JavaScript only does bitwise operators up to 32-bit
          i += 8
          len -= 8
          break
        default:
          // console.log('error: unsupported data type: 0x' + dt.toString(16))
          len = 0
          continue
      }
      switch (tag) {
        case 0x0121: // battery voltage (in 0.001 V)
          if (R.item('config/battery') != null) {
            const vmin = 2700
            const vmax = 3000
            const v = Math.max(vmin, Math.min(val, vmax))
            const bat = ((v - vmin) / (vmax - vmin)) * 100
            R.item('config/battery').val = Math.max(0, Math.min(bat, 100))
          }
          break
        case 0x0328: // device temperature (in °C)
          if (R.item('config/temperature') != null) {
            R.item('config/temperature').val = val * 100
          }
          break
        case 0x0421: // unknown
        case 0x0521: // RSSI dB
        case 0x0624: // LQI
        case 0x0727: // unknwon
          break
        case 0x0821: // firmware
          if (R.item('attr/swversion') != null) {
            R.item('attr/swversion').val = '0.0.0_' + ('0000' + (val & 0xFF).toString()).slice(-4)
          }
          break
        case 0x0921: // unknown
        case 0x0A21: // parent NWK address
        case 0x0B20: // lightlevel (in lux)
          if (R.item('state/lightlevel') != null) {
            val = 10000 * log10(val) + 1
            R.item('state/lightlevel').val = Math.min(val, 0xFFFE)
            val -= R.item('config/tholddark').val
            R.item('state/dark').val = val > 0
            val -= R.item('config/tholdoffset').val
            R.item('state/daylight').val = val >= 0
          }
          break
        case 0x0C20: // unknown
          break
        case 0x0D23: // firmware
          if (R.item('attr/swversion') != null) {
            R.item('attr/swversion').val = '0.0.0_' + ('0000' + (val & 0xFF).toString()).slice(-4)
          }
          break
        case 0x1020: // unknown
        case 0x1123: // unknown
        case 0x1220: // unknown
          break
        case 0x6410: // on/off
          if (R.item('state/open') != null) {
            R.item('state/open').val = val !== 0
          }
          // if (R.item('state/presence') != null) { // don't update
          //   R.item('state/presence').val = val !== 0
          // }
          if (R.item('state/water') != null) { // not updated in C++ code?
            R.item('state/water').val = val !== 0
          }
          break
        case 0x6420: // lift (in % closed)
          if (R.item('state/lift') !== null) {
            R.item('state/lift').val = val
          }
          break
        case 0x6429: // temperature (in 0.01 °C)
          if (R.item('state/temperature') != null && val !== -10000) {
            R.item('state/temperature').val = val + R.item('config/offset').val
          }
          break
        case 0x6520: // battery level (in %)
          if (R.item('state/battery') != null) {
            R.item('state/battery').val = val
          }
          break
        case 0x6521: // humidity (in 0.01 %)
          if (R.item('state/humidity') != null) {
            R.item('state/humidity').val = val + R.item('config/offset').val
          }
          break
        case 0x6620: // unknown
          if (R.item('config/sensitivity') != null) {
            R.item('config/sensitivity').val = val - 1
          }
          break
        case 0x6621: // tvoc level (in ppb)
          if (R.item('state/airqualityppb') != null) {
            R.item('state/airqualityppb').val = val
          }
          break
        case 0x662B: // air pressure (in Pa)
          if (R.item('state/pressure') != null) {
            R.item('state/pressure').val = Math.round(val / 100) + R.item('config/offset').val
          }
          break
        case 0x6720: // air quality (as 6 - #stars), unknown
          if (R.item('state/airquality') != null) {
            R.item('state/airquality').val = ['excellent', 'good', 'moderate', 'poor', 'unhealthy'][val - 1]
          }
          if (R.item('config/devicemode') != null) {
            R.item('config/devicemode').val = ['undirected', 'leftright'][val]
          }
          break
        case 0x6820: // unknown
          break
        case 0x6920: // battery charging
          if (R.item('state/charging') != null) {
            R.item('state/charing').val = val === 1
          }
          if (R.item('config/triggerdistance') != null) {
            R.item('config/triggerdistance').val = ['far', 'medium', 'near'][val]
          }
          break
        case 0x6A20: // unknown
        case 0x6B20: // unknown
        case 0x9539: // consumption
        case 0x9721: // unknown
        case 0x9821: // unknown
        case 0x9839: // power
        case 0x9921: // unknown
        case 0x9A20: // unknown
        case 0x9A21: // unknown
        case 0x9A25: // unknown
          break
        default:
          // console.log('warning: unknown tag: 0x' + tag.toString(16))
          break
      }
    }
  }
}
