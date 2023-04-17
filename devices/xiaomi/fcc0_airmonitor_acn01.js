/* global R, ZclFrame */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x00F7) {
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
        case 0x0328: // device temperature (in °C)
          if (R.item('config/temperature') != null) {
            R.item('config/temperature').val = val * 100
          }
          break
        case 0x0421: // unknown
        case 0x0521: // RSSI dB
        case 0x0624: // LQI
          break
        case 0x0821: // firmware
          if (R.item('attr/swversion') != null) {
            R.item('attr/swversion').val = '0.0.0_' + ('0000' + (val & 0xFF).toString()).slice(-4)
          }
          break
        case 0x0A21: // parent NWK address
        case 0x0C20: // unknown
          break
        case 0x6429: // temperature (in 0.01 °C)
          if (R.item('state/temperature') != null) {
            R.item('state/temperature').val = val + R.item('config/offset').val
          }
          break
        case 0x6521: // humidity (in 0.01 %)
          if (R.item('state/humidity') != null) {
            R.item('state/humidity').val = val + R.item('config/offset').val
          }
          break
        case 0x6621: // tvoc level (in ppb)
          if (R.item('state/airqualityppb') != null) {
            R.item('state/airqualityppb').val = val
          }
          break
        case 0x6720: // air quality (as 6 - #stars)
          if (R.item('state/airquality') != null) {
            R.item('state/airquality').val = ['excellent', 'good', 'moderate', 'poor', 'unhealthy'][val - 1]
          }
          break
        default:
          // console.log('warning: unknown tag: 0x' + tag.toString(16))
          break
      }
    }
  }
}
