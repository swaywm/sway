#!/usr/bin/env python

from PIL import Image
import os
import sys

fi = Image.open(sys.argv[1])
image_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
size = int(sys.argv[2])

# Split into images ordered as follows:
# 012
# 7 3
# 654
fi.crop((0, 0, size, size)).save(os.path.join(image_dir, "0.png"))
fi.crop((size, 0, fi.width-size, size)).save(os.path.join(image_dir, "1.png"))
fi.crop((fi.width-size, 0, fi.width, size)).save(os.path.join(image_dir, "2.png"))
fi.crop((fi.width-size, size, fi.width, fi.height-size)).save(os.path.join(image_dir, "3.png"))
fi.crop((fi.width-size, fi.height-size, fi.width, fi.height)).save(os.path.join(image_dir, "4.png"))
fi.crop((size, fi.height-size, fi.width-size, fi.height)).save(os.path.join(image_dir, "5.png"))
fi.crop((0, fi.height-size, size, fi.height)).save(os.path.join(image_dir, "6.png"))
fi.crop((0, size, size, fi.height-size)).save(os.path.join(image_dir, "7.png"))
