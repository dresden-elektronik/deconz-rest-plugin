var arr = [];
var hex;
let str = Attr.val;

for (var i = 0; i < str.length; i++)
{
    hex = str.charCodeAt(i).toString(16);
    if (hex.length < 2) { hex = "0" + hex.toUpperCase(); }
    arr.push(hex);
}

var res = arr.join("");
Item.val = res.toString(16);