
const cmd = ZclFrame.cmd;
const pl0 = ZclFrame.at(0);
let btnev = 0;
let angle = 0;
if (ClusterId === 6) {
	if (cmd === 253) {
		if (pl0 === 0) btnev = 1002;
		else if (pl0 === 1) btnev = 1004;
	}
	else if (cmd === 2 && pl0 === 0) btnev = 1002;
	else if (cmd === 252 && pl0 === 0) btnev = 1030;
	else if (cmd === 252 && pl0 === 1) btnev = 1031;
} else if (ClusterId === 8) {
	if (cmd === 2) {
		angle = ZclFrame.at(1);
		if (pl0 === 0) btnev = 1030;
		else if (pl0 === 1) { btnev = 1031; angle = -angle; }
	}
}

if (btnev) {
	R.item('state/buttonevent').val = btnev;
}

if (angle) {
	let a = R.item('state/angle').val;
	a += angle;
	if (a > 360) a -= 360;
	else if (a < 0) a = 360 + a;
	R.item('state/angle').val = a;
}

if (btnev || angle) // for state/lastupdated
	Item.val = !Item.val;
