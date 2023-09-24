/* global Item, R, ZclFrame */

Item.val = true
R.item('state/dark').val = (ZclFrame.at(0) & 0x01) === 0x00
R.item('config/duration').val = Math.round((ZclFrame.at(1) | ZclFrame.at(2) << 8) / 10)
