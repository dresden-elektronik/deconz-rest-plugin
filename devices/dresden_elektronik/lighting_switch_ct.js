var i = R.item("state/buttonevent");
if (ZclFrame.cmd == 0x4b) {
	var mode = ZclFrame.at(0);
	var rate = ZclFrame.at(1);
	var button;
	if (mode == 1) button = 3000;
	else if (mode == 3) button = 4000;
	else {
		button = i.val >> 2;
		button = ((button << 2) / 1000) * 1000;
	}

	if (mode == 0) button += 3;
	else if (rate == 0xfe) button += 2;
	else button += 1;

	i.val = button;
	R.item("attr/mode").val = 3;
}
