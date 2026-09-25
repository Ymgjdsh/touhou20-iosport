"""Build iOS icon sizes from the supplied character artwork."""
from pathlib import Path
import sys

from PIL import Image, ImageEnhance, ImageFilter


def main(source: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    artwork = Image.open(source).convert("RGB")
    width, height = artwork.size
    side = min(width, height)
    left = (width - side) // 2
    # Keep the face, hat, hands and wand legible at home-screen icon sizes.
    top = max(0, min(height - side, round(height * 0.045)))
    icon = artwork.crop((left, top, left + side, top + side))
    icon = icon.resize((1024, 1024), Image.Resampling.LANCZOS)
    icon = ImageEnhance.Contrast(icon).enhance(1.12)
    icon = icon.filter(ImageFilter.UnsharpMask(radius=2.2, percent=170, threshold=3))
    sizes = {
        "TH20Icon120.png": 120,
        "TH20Icon180.png": 180,
        "TH20Icon76.png": 76,
        "TH20Icon152.png": 152,
        "TH20Icon167.png": 167,
        "TH20Icon1024.png": 1024,
    }
    for name, size in sizes.items():
        result = icon.resize((size, size), Image.Resampling.LANCZOS)
        if size < 1024:
            result = result.filter(ImageFilter.UnsharpMask(radius=0.6, percent=110, threshold=2))
        result.save(destination / name, optimize=True)


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
