/* global Attr, Item */
const v = Attr.val
Item.val = (v & 0x000000ff).toString() + '.' +
           ((v & 0x0000ff00) >> 8).toString() + '.' +
           ((v & 0x00ff0000) >> 16).toString() + '.' +
           ((v & 0xff000000) >> 24).toString()
