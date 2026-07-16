using System;
using System.Drawing;
using System.Drawing.Imaging;

class Program {
    static void CropImage(string path) {
        using (Bitmap bmp = new Bitmap(path)) {
            int minX = bmp.Width, minY = bmp.Height, maxX = 0, maxY = 0;
            for (int y = 0; y < bmp.Height; y++) {
                for (int x = 0; x < bmp.Width; x++) {
                    if (bmp.GetPixel(x, y).A > 0) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }
            if (minX <= maxX && minY <= maxY) {
                int padding = 0;
                minX = Math.Max(0, minX - padding);
                minY = Math.Max(0, minY - padding);
                maxX = Math.Min(bmp.Width - 1, maxX + padding);
                maxY = Math.Min(bmp.Height - 1, maxY + padding);
                int width = maxX - minX + 1;
                int height = maxY - minY + 1;
                int size = Math.Max(width, height);
                // Make it a square crop
                int cx = minX + width / 2;
                int cy = minY + height / 2;
                
                int cMinX = Math.Max(0, cx - size / 2);
                int cMinY = Math.Max(0, cy - size / 2);
                int cMaxX = Math.Min(bmp.Width - 1, cx + size / 2);
                int cMaxY = Math.Min(bmp.Height - 1, cy + size / 2);
                int sqWidth = cMaxX - cMinX + 1;
                int sqHeight = cMaxY - cMinY + 1;
                int sqSize = Math.Max(sqWidth, sqHeight);

                Rectangle cropRect = new Rectangle(cx - sqSize/2, cy - sqSize/2, sqSize, sqSize);
                // Clamp cropRect to bounds
                if (cropRect.X < 0) cropRect.X = 0;
                if (cropRect.Y < 0) cropRect.Y = 0;
                if (cropRect.Right > bmp.Width) cropRect.Width = bmp.Width - cropRect.X;
                if (cropRect.Bottom > bmp.Height) cropRect.Height = bmp.Height - cropRect.Y;

                Console.WriteLine(String.Format("Cropping {0} to {1}, {2}, {3}x{4}", path, cropRect.X, cropRect.Y, cropRect.Width, cropRect.Height));
                using (Bitmap cropped = bmp.Clone(cropRect, bmp.PixelFormat)) {
                    cropped.Save(path + ".cropped.png", ImageFormat.Png);
                }
            } else {
                Console.WriteLine("No non-transparent pixels found in " + path);
            }
        }
    }
    static void Main() {
        try {
            CropImage(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png");
            CropImage(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\aim_cursor.png");
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
