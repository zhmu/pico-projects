#!/usr/bin/env python3

import sys
from PIL import Image

LCD_HEIGHT = 48
LCD_WIDTH = 84

output = [ 0 ] * int((LCD_WIDTH * LCD_HEIGHT) / 8)

if len(sys.argv) < 2:
    print('usage: {} image.png [invert]'.format(sys.argv[0]))
    quit()

# having the invert parameter inverts the matching color
match_value = len(sys.argv) == 2

with Image.open(sys.argv[1]) as im:
    im = im.convert(mode='1')
    for y in range(im.height):
        for x in range(im.width):
            v = im.getpixel((x, y))
            if bool(v) == match_value:
                output[x + int(y / 8) * LCD_WIDTH] |= 1 << (y % 8)

print('constexpr std::array<uint8_t, {}> image{{{{'.format(len(output)))
s = '    '
for n, v in enumerate(output):
    s += '0x{:02x}'.format(v)
    if n != len(output) - 1:
        s += ', '
    if n % 8 == 7:
        s += '\n    '
print(s)
print('}};')
