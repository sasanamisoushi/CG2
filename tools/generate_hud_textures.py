from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


OUTPUT_DIR = Path(__file__).resolve().parents[1] / "project" / "resources"
FONT_PATH = Path("C:/Windows/Fonts/seguisb.ttf")


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_PATH), size)


def save_label(text: str, filename: str, color: tuple[int, int, int, int]) -> None:
    image = Image.new("RGBA", (256, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.text((8, 2), text, font=font(42), fill=color, stroke_width=2, stroke_fill=(0, 30, 60, 255))
    bbox = image.getbbox()
    if bbox:
        image = image.crop((max(0, bbox[0] - 4), max(0, bbox[1] - 4), min(image.width, bbox[2] + 4), min(image.height, bbox[3] + 4)))
    image.save(OUTPUT_DIR / filename)


def save_digit_atlas() -> None:
    cell_width = 64
    cell_height = 80
    glyphs = "0123456789/"
    image = Image.new("RGBA", (cell_width * len(glyphs), cell_height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    digit_font = font(60)
    for digit, text in enumerate(glyphs):
        bounds = draw.textbbox((0, 0), text, font=digit_font, stroke_width=2)
        width = bounds[2] - bounds[0]
        height = bounds[3] - bounds[1]
        x = digit * cell_width + (cell_width - width) / 2 - bounds[0]
        y = (cell_height - height) / 2 - bounds[1]
        draw.text((x, y), text, font=digit_font, fill=(225, 250, 255, 255), stroke_width=2, stroke_fill=(0, 70, 120, 255))
    image.save(OUTPUT_DIR / "hud_digits.png")


OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

# 画像生成したHUDフレームの透明余白を除去し、ゲーム内で扱いやすくする。
panel_path = OUTPUT_DIR / "hud_panel_frame.png"
if panel_path.exists():
    panel = Image.open(panel_path).convert("RGBA")
    panel_bbox = panel.getbbox()
    if panel_bbox:
        panel.crop(panel_bbox).save(panel_path)

save_digit_atlas()
save_label("HP", "hud_label_hp.png", (255, 90, 105, 255))
save_label("AMMO", "hud_label_ammo.png", (255, 220, 80, 255))
save_label("SP", "hud_label_sp.png", (80, 225, 255, 255))
