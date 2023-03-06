var s = "0.0.0_";
var v = (Attr.val & 0xFF).toString();
v = v.padStart(4, "0");
Item.val = s + v;
