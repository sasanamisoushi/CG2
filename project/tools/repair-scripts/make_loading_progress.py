from pathlib import Path
import math

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "resources" / "loading_indicator.png"
SHEET = ROOT / "resources" / "loading_progress_sheet.png"
PREVIEW = ROOT / "resources" / "loading_progress_preview.gif"

FRAME_SIZE = 512
FRAME_COUNT = 8


def make_frame(source: Image.Image, progress: float) -> Image.Image:
    width, height = source.size
    result = source.copy()
    result_pixels = result.load()
    source_pixels = source.load()

    center_x = width * 0.5
    center_y = height * 0.42
    text_top = int(height * 0.78)
    inner_radius = width * 0.06
    outer_radius = width * 0.47
    limit = progress * math.tau

    for y in range(height):
        for x in range(width):
            if y >= text_top:
                continue

            dx = x - center_x
            dy = y - center_y
            radius = math.hypot(dx, dy)
            if inner_radius <= radius <= outer_radius:
                red, green, blue, alpha = source_pixels[x, y]
                brightness = max(red, green, blue)
                saturation = brightness - min(red, green, blue)
                is_emissive = brightness > 105 and saturation > 45

                # 銀色の装甲は一切変更しない。元画像のシアン／オレンジの
                # 発光溝だけを消灯色または点灯色へ置き換える。
                if not is_emissive:
                    continue

                angle = (math.atan2(dx, -dy) + math.tau) % math.tau
                if angle <= limit:
                    intensity = brightness / 255.0
                    result_pixels[x, y] = (
                        min(255, int(255 * intensity + 35)),
                        min(190, int(112 * intensity + 25)),
                        min(72, int(22 * intensity)),
                        alpha,
                    )
                else:
                    slot = int(brightness * 0.10)
                    result_pixels[x, y] = (slot + 7, slot + 5, slot + 3, alpha)

    return result.resize((FRAME_SIZE, FRAME_SIZE), Image.Resampling.LANCZOS)


def main() -> None:
    source = Image.open(SOURCE).convert("RGBA")
    frames = [make_frame(source, index / (FRAME_COUNT - 1)) for index in range(FRAME_COUNT)]

    sheet = Image.new("RGBA", (FRAME_SIZE * FRAME_COUNT, FRAME_SIZE), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        sheet.alpha_composite(frame, (index * FRAME_SIZE, 0))
    sheet.save(SHEET)

    preview_frames = frames + list(reversed(frames[1:-1]))
    preview_frames[0].save(
        PREVIEW,
        save_all=True,
        append_images=preview_frames[1:],
        duration=120,
        loop=0,
        disposal=2,
        transparency=0,
    )


if __name__ == "__main__":
    main()
