using System;
using System.Drawing;
using System.Drawing.Imaging;

class Program {
    static void RestoreAndMakeTransparent(string path) {
        string backupPath = path + ".cropped.png";
        if (!System.IO.File.Exists(backupPath)) {
            Console.WriteLine("Backup not found: " + backupPath);
            return;
        }

        using (Bitmap bmp = new Bitmap(backupPath)) {
            Color bgColor = bmp.GetPixel(0, 0); // Assume top-left is background
            
            using (Bitmap result = new Bitmap(bmp.Width, bmp.Height, PixelFormat.Format32bppArgb)) {
                for (int y = 0; y < bmp.Height; y++) {
                    for (int x = 0; x < bmp.Width; x++) {
                        Color c = bmp.GetPixel(x, y);
                        // Make transparent if it matches background closely
                        if (Math.Abs(c.R - bgColor.R) <= 20 && Math.Abs(c.G - bgColor.G) <= 20 && Math.Abs(c.B - bgColor.B) <= 20) {
                            result.SetPixel(x, y, Color.Transparent);
                        } else {
                            result.SetPixel(x, y, c);
                        }
                    }
                }
                result.Save(path, ImageFormat.Png);
            }
        }
        Console.WriteLine("Restored and made transparent: " + path);
    }
    static void Main() {
        try {
            RestoreAndMakeTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png");
            RestoreAndMakeTransparent(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\aim_cursor.png");
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
