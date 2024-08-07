var v = Attr.val;
Item.val = String((v & 192) >> 6) + '.' + String((v & 48) >> 4) + '.' + String(v & 15);
