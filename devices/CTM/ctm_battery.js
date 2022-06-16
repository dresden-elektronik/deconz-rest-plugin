const vmin = 27;
const vmax = 31;
let bat = Attr.val;

if      (bat > vmax) { bat = vmax; }
else if (bat < vmin) { bat = vmin; }

bat = ((bat - vmin) /(vmax - vmin)) * 100;

if      (bat > 100) { bat = 100; }
else if (bat <= 0)  { bat = 1; } // ?

Item.val = bat;