from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "resources" / "radar_frame.png"
SIZE = 512
CENTER = SIZE // 2


def main() -> None:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)

    # 半透明のレーダー盤面
    draw = ImageDraw.Draw(image)
    draw.ellipse((38, 38, 474, 474), fill=(4, 15, 24, 205))

    # シアンのグロー
    for radius in (205, 150, 96):
        glow_draw.ellipse(
            (CENTER - radius, CENTER - radius, CENTER + radius, CENTER + radius),
            outline=(0, 220, 255, 150),
            width=5,
        )
    glow_draw.line((CENTER, 55, CENTER, 457), fill=(0, 220, 255, 115), width=4)
    glow_draw.line((55, CENTER, 457, CENTER), fill=(0, 220, 255, 115), width=4)
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(8)))

    # 盤面グリッド
    draw = ImageDraw.Draw(image)
    for radius, alpha in ((205, 210), (150, 125), (96, 105)):
        draw.ellipse(
            (CENTER - radius, CENTER - radius, CENTER + radius, CENTER + radius),
            outline=(24, 217, 242, alpha),
            width=2,
        )
    draw.line((CENTER, 52, CENTER, 460), fill=(30, 196, 220, 125), width=2)
    draw.line((52, CENTER, 460, CENTER), fill=(30, 196, 220, 125), width=2)
    draw.line((112, 112, 400, 400), fill=(30, 196, 220, 45), width=1)
    draw.line((400, 112, 112, 400), fill=(30, 196, 220, 45), width=1)

    # 銀色のメカ外枠
    draw.ellipse((23, 23, 489, 489), outline=(34, 46, 57, 255), width=22)
    draw.ellipse((31, 31, 481, 481), outline=(156, 173, 184, 255), width=7)
    draw.ellipse((43, 43, 469, 469), outline=(17, 30, 39, 255), width=10)
    draw.arc((25, 25, 487, 487), 205, 335, fill=(0, 229, 255, 255), width=6)
    draw.arc((25, 25, 487, 487), 22, 150, fill=(255, 117, 24, 255), width=6)

    # 外周のメカノッチ
    for x, y, w, h in (
        (224, 8, 64, 30), (224, 474, 64, 30),
        (8, 224, 30, 64), (474, 224, 30, 64),
        (62, 62, 42, 19), (408, 62, 42, 19),
        (62, 431, 42, 19), (408, 431, 42, 19),
    ):
        draw.rounded_rectangle((x, y, x + w, y + h), radius=5,
                               fill=(49, 62, 72, 255), outline=(184, 197, 205, 255), width=3)

    # プレイヤー固定マーカー（機首方向＝上）
    draw.polygon(((CENTER, 224), (CENTER - 12, 248), (CENTER, 243), (CENTER + 12, 248)),
                 fill=(0, 235, 255, 255))
    draw.ellipse((CENTER - 4, CENTER - 4, CENTER + 4, CENTER + 4), fill=(255, 255, 255, 255))
    image.save(OUTPUT)


if __name__ == "__main__":
    main()
