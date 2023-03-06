/* global Item, R, ZclFrame */

if (ZclFrame.at(2) === 0x00) {
  Item.val = ZclFrame.at(0) * 1000 + ZclFrame.at(4)
  R.item('state/eventduration').val = ZclFrame.at(7) << 8 | ZclFrame.at(6)
}
