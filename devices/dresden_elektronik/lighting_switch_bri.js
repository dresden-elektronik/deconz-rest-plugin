var i = R.item("state/buttonevent");
if (ZclFrame.cmd == 1) {
	if (ZclFrame.at(0) == 0)
		i.val = (SrcEp == 1) ? 1001 : 3001;
	else
		i.val = (SrcEp == 1) ? 2001 : 4001;
} else if (ZclFrame.cmd == 3) {
	var v = i.val >> 2;
	i.val = ((v << 2) / 1000) * 1000 + 3;
}
