/* global Attr, Item */

const vmin = 27
const vmax = 30
const v = Math.max(vmin, Math.min(Attr.val, vmax))
const bat = ((v - vmin) / (vmax - vmin)) * 100
Item.val = Math.max(0, Math.min(bat, 100))
