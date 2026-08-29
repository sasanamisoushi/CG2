from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


OUTPUT_PATH = Path(__file__).resolve().parents[1] / "resources" / "missile_lock_on_reticle.png"
SIZE = 256


def draw_reticle(draw, offset, width, color):
    cx = SIZE // 2 + offset[0]
    cy = SIZE // 2 + offset[1]
    outer = 108
    inner = 78
    points = [
        (cx, cy - outer),
        (cx + outer, cy),
        (cx, cy + outer),
        (cx - outer, cy),
    ]
    inner_points = [
        (cx, cy - inner),
        (cx + inner, cy),
        (cx, cy + inner),
        (cx - inner, cy),
    ]
    draw.line(points + [points[0]], fill=color, width=width, joint="curve")
    draw.line(inner_points + [inner_points[0]], fill=color, width=width, joint="curve")

    gap = 42
    tick = 25
    draw.line((cx - gap, cy - outer, cx - gap - tick, cy - outer), fill=color, width=width)
    draw.line((cx + gap, cy + outer, cx + gap + tick, cy + outer), fill=color, width=width)
    draw.line((cx + outer, cy - gap, cx + outer, cy - gap - tick), fill=color, width=width)
    draw.line((cx - outer, cy + gap, cx - outer, cy + gap + tick), fill=color, width=width)


image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
glow_draw = ImageDraw.Draw(glow)
draw_reticle(glow_draw, (0, 0), 10, (255, 70, 18, 180))
glow = glow.filter(ImageFilter.GaussianBlur(8))
image.alpha_composite(glow)

draw = ImageDraw.Draw(image)
draw_reticle(draw, (0, 0), 4, (255, 220, 110, 255))
draw_reticle(draw, (0, 0), 2, (255, 74, 22, 255))

# Missile cue: three compact chevrons at the bottom of the reticle.
for index in range(3):
    y = 178 + index * 13
    draw.line((116 - index * 7, y, 128, y + 9), fill=(255, 220, 110, 230), width=3)
    draw.line((128, y + 9, 140 + index * 7, y), fill=(255, 220, 110, 230), width=3)

image.save(OUTPUT_PATH)
