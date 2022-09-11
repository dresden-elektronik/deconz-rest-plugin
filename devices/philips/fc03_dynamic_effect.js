/* global Item, ZclFrame */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x0002) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  const i = status === 0 ? 4 : 3
  if (dt === 0x41) {
    const len = ZclFrame.at(i)
    if (len >= 4) {
      const mode = ZclFrame.at(i + 2) << 8 | ZclFrame.at(i + 1)
      if (mode === 0x00ab && len === 10) {
        const effect = ZclFrame.at(i + 10) << 8 | ZclFrame.at(i + 9)
        switch (effect) {
          case 0x8001:
            Item.val = 'candle'
            break
          case 0x8002:
            Item.val = 'fireplace'
            break
          default:
            Item.val = 'none'
            break
        }
      } else {
        Item.val = 'none'
      }
    }
  }
}
