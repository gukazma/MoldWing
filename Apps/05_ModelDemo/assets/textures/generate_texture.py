#!/usr/bin/env python3
"""
Generate a UV test texture for the cube model
"""
from PIL import Image, ImageDraw, ImageFont
import numpy as np

# Create a 512x512 image
width, height = 512, 512
img = Image.new('RGB', (width, height), color='white')
draw = ImageDraw.Draw(img)

# Draw a colorful checkerboard pattern
colors = [
    (255, 100, 100),  # Red
    (100, 255, 100),  # Green
    (100, 100, 255),  # Blue
    (255, 255, 100),  # Yellow
]

checker_size = 128
for i in range(4):
    for j in range(4):
        x = (j % 4) * checker_size
        y = (i % 4) * checker_size
        color_idx = (i + j) % 4
        draw.rectangle([x, y, x + checker_size, y + checker_size], fill=colors[color_idx])

# Draw grid lines
for i in range(5):
    pos = i * checker_size
    draw.line([(pos, 0), (pos, height)], fill='black', width=3)
    draw.line([(0, pos), (width, pos)], fill='black', width=3)

# Add border
draw.rectangle([0, 0, width-1, height-1], outline='black', width=5)

# Save the texture
img.save('cube_texture.png')
print("Texture generated: cube_texture.png")
