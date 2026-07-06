using System;
using System.Drawing;
using System.Drawing.Imaging;

class Program {
    static void CropAndMakeTransparent(string path) {
        using (Bitmap bmp = new Bitmap(path)) {
            Color bgColor = bmp.GetPixel(0, 0); // Assume top-left pixel is the background color
            int minX = bmp.Width, minY = bmp.Height, maxX = 0, maxY = 0;
            
            // First pass: find bounding box of non-background pixels
            for (int y = 0; y < bmp.Height; y++) {
                for (int x = 0; x < bmp.Width; x++) {
                    Color c = bmp.GetPixel(x, y);
                    // Check if it's not the background color (allow small tolerance)
                    if (Math.Abs(c.R - bgColor.R) > 5 || Math.Abs(c.G - bgColor.G) > 5 || Math.Abs(c.B - bgColor.B) > 5) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }
            
            if (minX <= maxX && minY <= maxY) {
                int width = maxX - minX + 1;
                int height = maxY - minY + 1;
                int size = Math.Max(width, height);
                int padding = 10;
                size += padding * 2;
                
                int cx = minX + width / 2;
                int cy = minY + height / 2;
                
                Rectangle cropRect = new Rectangle(cx - size / 2, cy - size / 2, size, size);
                // Clamp
                if (cropRect.X < 0) cropRect.X = 0;
                if (cropRect.Y < 0) cropRect.Y = 0;
                if (cropRect.Right > bmp.Width) cropRect.Width = bmp.Width - cropRect.X;
                if (cropRect.Bottom > bmp.Height) cropRect.Height = bmp.Height - cropRect.Y;

                Console.WriteLine(String.Format("Cropping {0} to {1}, {2}, {3}x{4}", path, cropRect.X, cropRect.Y, cropRect.Width, cropRect.Height));
                
                using (Bitmap cropped = new Bitmap(cropRect.Width, cropRect.Height, PixelFormat.Format32bppArgb)) {
                    for (int y = 0; y < cropRect.Height; y++) {
                        for (int x = 0; x < cropRect.Width; x++) {
                            Color c = bmp.GetPixel(cropRect.X + x, cropRect.Y + y);
                            if (Math.Abs(c.R - bgColor.R) <= 10 && Math.Abs(c.G - bgColor.G) <= 10 && Math.Abs(c.B - bgColor.B) <= 10) {
                                cropped.SetPixel(x, y, Color.Transparent);
                            } else {
                                cropped.SetPixel(x, y, c);
                            }
                        }
                    }
                    cropped.Save(path, ImageFormat.Png); // Overwrite original
                }
            } else {
                Console.WriteLine("No non-background pixels found in " + path);
            }
        }
    }
    static void Main() {
        try {
            CropAndMakeTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png");
            CropAndMakeTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\aim_cursor.png");
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
