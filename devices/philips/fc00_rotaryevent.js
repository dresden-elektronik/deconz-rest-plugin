/* global Item, R, ZclFrame */

if (ZclFrame.at(2) === 0x01) {
  Item.val = ZclFrame.at(4)
  R.item('state/expectedeventduration').val = ZclFrame.at(22) << 8 | ZclFrame.at(21)
  const v = ZclFrame.at(13) << 8 | ZclFrame.at(12)
  R.item('state/expectedrotation').val = v > 0x7FFF ? v - 0x10000 : v
}
