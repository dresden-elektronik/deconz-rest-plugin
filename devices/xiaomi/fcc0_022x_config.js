/* global R, ZclFrame */

const attrid = ZclFrame.at(1) << 8 | ZclFrame.at(0)
if (attrid === 0x0223 || attrid === 0x0224 || attrid === 0x0225) {
  const status = ZclFrame.at(2)
  const dt = status === 0 ? ZclFrame.at(3) : status
  let i = status === 0 ? 4 : 3
  if (dt === 0x41) {
    const len = ZclFrame.at(i)
	const size = ZclFrame.payloadSize
    i++
	  const icon = ZclFrame.at(i)
	  i++
  
	  let text = ''
	  for (; i < size; i++ ) {
		  text += String.fromCharCode(ZclFrame.at(i))
	  }
	  if (text.length === 0) {
	  	text = 'null'
	  }
	  if (Item.name === 'config/switch1_text' || Item.name === 'config/switch2_text' || Item.name === 'config/switch3_text') {
	  	Item.val = text
	  } else if (Item.name === 'config/switch1_icon' || Item.name === 'config/switch2_icon' || Item.name === 'config/switch3_icon') {
	  	Item.val = icon
	  }
  }
}
