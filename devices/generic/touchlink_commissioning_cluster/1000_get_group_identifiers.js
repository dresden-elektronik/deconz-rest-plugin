/* global Item, ZclFrame */
/* eslint-disable no-var */

// const total = ZclFrame.at(0)
const startIndex = ZclFrame.at(1)
if (startIndex === 0) {
  const count = ZclFrame.at(2)

  var list = (startIndex === 0) ? '' : Item.val
  for (var i = 1; i <= count; i++) {
    const groupId = ZclFrame.at(3 * i) | ZclFrame.at(3 * i + 1) << 8
    list += (i > 1 ? ',' : '') + groupId.toString()
  }
  Item.val = list
}
