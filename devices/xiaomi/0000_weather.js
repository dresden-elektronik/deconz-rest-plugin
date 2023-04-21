/* global R, ZclFrame */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0xFF01) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  var i = status === 0 ? 4 : 3
  if (dt === 0x41) {
    var len = ZclFrame.at(i)
    i++
    while (len > 0) {
      const dt = ZclFrame.at(i + 1)
      const tag = ZclFrame.at(i) << 8 | dt
      i += 2
      len -= 2
      var val
      switch (dt) {
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
        case 0x2B: // int32
          val = ZclFrame.at(i + 2) << 24 | ZclFrame.at(i + 2) << 16 |
            ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
          i += 4
          len += 4
          break
        case 0x24: // uint40
          i += 5
          len -= 5
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
        case 0x0421: // unknown
        case 0x0521: // RSSI dB
        case 0x0624: // LQI
          break
        case 0x0A21: // parent NWK address
          break
        case 0x6429: // temperature (in 0.01 °C)
          if (R.item('state/temperature') != null && val !== -10000) {
            R.item('state/temperature').val = val + R.item('config/offset').val
          }
          break
        case 0x6521: // humidity (in 0.01 %)
          if (R.item('state/humidity') != null) {
            R.item('state/humidity').val = val + R.item('config/offset').val
          }
          break
        case 0x662B: // air pressure (in Pa)
          if (R.item('state/pressure') != null) {
            R.item('state/pressure').val = Math.round(val / 100) + R.item('config/offset').val
          }
          break
        default:
          // console.log('warning: unknown tag: 0x' + tag.toString(16))
          break
      }
    }
  }
}
