/* global R, ZclFrame */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x0002) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  const i = status === 0 ? 4 : 3
  console.log(status === 0 ? 'read attribute reponse' : 'attribute report')
  if (dt === 0x41) {
    const len = ZclFrame.at(i)
    console.log('length', len)
    if (len >= 4) {
      const mode = ZclFrame.at(i + 2) << 8 | ZclFrame.at(i + 1)
      R.item('state/on').val = ZclFrame.at(i + 3) !== 0
      R.item('state/bri').val = ZclFrame.at(i + 4)
      if (mode === 0x000b && len === 8) {
        R.item('state/x').val = ZclFrame.at(i + 6) << 8 | ZclFrame.at(i + 5)
        R.item('state/y').val = ZclFrame.at(i + 8) << 8 | ZclFrame.at(i + 7)
        R.item('state/colormode').val = 'xy'
        if (R.item('state/dynamic_effect') != null) {
          R.item('state/dynamic_effect').val = 'none'
        }
      } else if (mode === 0x000f && len === 10) {
        R.item('state/ct').val = ZclFrame.at(i + 6) << 8 | ZclFrame.at(i + 5)
        R.item('state/x').val = ZclFrame.at(i + 8) << 8 | ZclFrame.at(i + 7)
        R.item('state/y').val = ZclFrame.at(i + 10) << 8 | ZclFrame.at(i + 9)
        R.item('state/colormode').val = 'ct'
        if (R.item('state/dynamic_effect') != null) {
          R.item('state/dynamic_effect').val = 'none'
        }
      } else if (mode === 0x00ab && len === 10) {
        R.item('state/x').val = ZclFrame.at(i + 6) << 8 | ZclFrame.at(i + 5)
        R.item('state/y').val = ZclFrame.at(i + 8) << 8 | ZclFrame.at(i + 7)
        R.item('state/colormode').val = 'xy'
        const effect = ZclFrame.at(i + 10) << 8 | ZclFrame.at(i + 9)
        switch (effect) {
          case 0x8001:
            R.item('state/dynamic_effect').val = 'candle'
            break
          case 0x8002:
            R.item('state/dynamic_effect').val = 'fireplace'
            break
          default:
            R.item('state/dynamic_effect').val = 'none'
            break
        }
      }
    }
  }
}
