var v = Math.round(Attr.val * 100);

if (v > 0) { R.item('state/gesture').val = 7 }  // Rotate clockwise
else { R.item('state/gesture').val = 8 };       // Rotate counter-clockwise
Item.val = v;