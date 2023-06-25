/* global Item, ZclFrame */
/* eslint-disable no-var */

const status = ZclFrame.at(1) << 8 | ZclFrame.at(0)
const offset = ZclFrame.at(3) << 8 | ZclFrame.at(2)
// const totalLength = ZclFrame.at(7) << 8 | ZclFrame.at(6)
// const length = ZclFrame.at(10) // ostring length
var o = 11 // start of ostring data bytes
var s = ''

if (status === 0x0000) {
  if (offset === 0x0000) {
    if (ZclFrame.at(o++) === 0x0A) { // unknown
      o += 10 // fixed L
      if (ZclFrame.at(o++) === 0x0A) { // modelid
        o += ZclFrame.at(o) + 1
        if (ZclFrame.at(o++) === 0x12) { // manufacturername
          o += ZclFrame.at(o) + 1
          if (ZclFrame.at(o) === 0x42) { // productname
            s = 'offset: 0x' + ('00000000' + (Number(o) - 11).toString(16)).slice(-8)
          }
        }
      }
    }
  } else {
    if (ZclFrame.at(o++) === 0x42) { // T: productname
      for (var l = ZclFrame.at(o++); l > 0; l--) {
        s += String.fromCharCode(ZclFrame.at(o++))
      }
      Item.val = s
    }
  }
}
