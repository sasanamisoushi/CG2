using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

class Program {
    static void CropCenterAndTransparent(string path) {
        string tmpPath = path + ".tmp.png";
        using (Bitmap bmp = new Bitmap(path)) {
            int cx = bmp.Width / 2;
            int cy = bmp.Height / 2;
            int size = 512;
            
            Rectangle cropRect = new Rectangle(cx - size / 2, cy - size / 2, size, size);
            
            // Just in case it's smaller
            if (cropRect.X < 0) cropRect.X = 0;
            if (cropRect.Y < 0) cropRect.Y = 0;
            if (cropRect.Width > bmp.Width) cropRect.Width = bmp.Width;
            if (cropRect.Height > bmp.Height) cropRect.Height = bmp.Height;

            Color bgColor = bmp.GetPixel(cropRect.X, cropRect.Y); // Top-left of the cropped region
            
            using (Bitmap cropped = new Bitmap(cropRect.Width, cropRect.Height, PixelFormat.Format32bppArgb)) {
                for (int y = 0; y < cropRect.Height; y++) {
                    for (int x = 0; x < cropRect.Width; x++) {
                        Color c = bmp.GetPixel(cropRect.X + x, cropRect.Y + y);
                        // Make transparent if it matches background closely
                        if (Math.Abs(c.R - bgColor.R) <= 20 && Math.Abs(c.G - bgColor.G) <= 20 && Math.Abs(c.B - bgColor.B) <= 20) {
                            cropped.SetPixel(x, y, Color.Transparent);
                        } else {
                            cropped.SetPixel(x, y, c);
                        }
                    }
                }
                cropped.Save(tmpPath, ImageFormat.Png);
            }
        }
        File.Delete(path);
        File.Move(tmpPath, path);
        Console.WriteLine("Cropped and replaced: " + path);
    }
    static void Main() {
        try {
            CropCenterAndTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png");
            CropCenterAndTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\aim_cursor.png");
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
