let s = "";
for (var i = 0; i < 3; i++) {
  s += Number(ZclFrame.at(5 + i)).toString(16);
  s += (i < 2) ? '.' : '';
}
Item.val = s;