const max = 50.0;
let speed = Attr.val;

speed = (speed <= max) ? speed : 0;
speed = speed / max * 100.0;

Item.val = speed;