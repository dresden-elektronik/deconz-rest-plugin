/* global Attr, R */

const vmin = 27
const vmax = 30
const v = Math.max(vmin, Math.min(Attr.val, vmax))
const bat = Math.round(((v - vmin) / (vmax - vmin)) * 100)
R.Item('config/battery').val = Math.max(0, Math.min(bat, 100))
