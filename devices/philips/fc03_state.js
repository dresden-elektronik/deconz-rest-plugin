/* global R, ZclFrame */
/* eslint-disable no-var */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x0002) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  var i = status === 0 ? 4 : 3
  if (dt === 0x41) {
    var len = ZclFrame.at(i)
    i++
    if (len >= 2) {
      const mode = ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
      i += 2
      len -= 2
      if ([0x000B, 0x000F, 0x00AB, 0x014B].indexOf(mode) >= 0 && len >= 2) {
        R.item('state/on').val = ZclFrame.at(i) !== 0
        R.item('state/bri').val = ZclFrame.at(i + 1)
        i += 2
        len -= 2
      }
      if (mode === 0x000F && len >= 2) {
        R.item('state/ct').val = ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
        R.item('state/colormode').val = 'ct'
        i += 2
        len -= 2
      }
      if ([0x000B, 0x000F, 0x00AB, 0x014B].indexOf(mode) >= 0 && len >= 4) {
        R.item('state/x').val = ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
        R.item('state/y').val = ZclFrame.at(i + 3) << 8 | ZclFrame.at(i + 2)
        i += 4
        len -= 4
        if (mode === 0x000B && R.item('state/colormode').val !== 'hs') {
          R.item('state/colormode').val = 'xy'
        }
      }
      if (mode === 0x00AB && len >= 2) {
        const effect = ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
        i += 2
        len -= 2
        switch (effect) {
          case 0x8001:
            R.item('state/effect').val = 'candle'
            break
          case 0x8002:
            R.item('state/effect').val = 'fireplace'
            break
          case 0x8003:
            R.item('state/effect').val = 'loop'
            break
          case 0x8009:
            R.item('state/effect').val = 'sunrise'
            break
          case 0x800a:
            R.item('state/effect').val = 'sparkle'
            break
          default:
            R.item('state/effect').val = '0x' + effect.toString(16)
            break
        }
        R.item('state/colormode').val = 'effect'
      } else {
        if (R.item('state/effect').val !== 'colorloop') {
          R.item('state/effect').val = 'none'
        }
      }
      if (mode === 0x014B && len >= 2) {
        const vLen = ZclFrame.at(i)
        i++
        len--
        if (len >= vLen + 2) {
          const nPoints = ZclFrame.at(i) >> 4
          const style = ZclFrame.at(i + 1)
          i += 4
          len -= 4
          const map = { points: [] }
          switch (style) {
            case 0x00:
            case 0x01:
              map.style = 'linear'
              break
            case 0x02:
              map.style = 'scattered'
              break
            case 0x04:
              map.style = 'mirrored'
              break
            default:
              map.style = '0x' + style.toString(16)
              break
          }
          const maxX = 0.7347
          const maxY = 0.8431
          for (var n = 1; n <= nPoints; n++) {
            const point = ZclFrame.at(i + 2) << 16 | ZclFrame.at(i + 1) << 8 | ZclFrame.at(i)
            const rawX = point & 0x000FFF
            const rawY = (point & 0xFFF000) >> 12
            const x = Math.ceil(rawX * maxX / 0.4095) / 10000
            const y = Math.ceil(rawY * maxY / 0.4095) / 10000
            map.points.push([x, y])
            i += 3
            len -= 3
          }
          map.segments = ZclFrame.at(i) >> 3
          map.color_adjustment = ZclFrame.at(i) & 0x07
          map.offset = ZclFrame.at(i + 1) >> 3
          map.offset_adjustment = ZclFrame.at(i + 1) & 0x07
          i += 2
          len -= 2
          R.item('state/gradient').val = JSON.stringify(map)
          R.item('state/colormode').val = 'gradient'
        }
      }
    }
  }
}
