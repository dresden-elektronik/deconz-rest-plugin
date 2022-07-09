let s = "0.0.0_";
let v = 0;

if (Attr.dataType == 33)
{
    v = (Attr.val & 0x00ff);
}
else if (Attr.dataType == 35)
{
    v = (Attr.val & 0x000000ff);
}

for (i = 0; i < (v.toString().length % 4); i++)
{
    s += "0";
}

Item.val = s + v;