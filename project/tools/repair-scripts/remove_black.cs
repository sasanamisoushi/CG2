using System;
using System.Drawing;
using System.Drawing.Imaging;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\boundary_alert.png";
        Bitmap bmp = new Bitmap(path);
        Bitmap res = new Bitmap(bmp.Width, bmp.Height, PixelFormat.Format32bppArgb);
        for(int y=0; y<bmp.Height; y++) {
            for(int x=0; x<bmp.Width; x++) {
                Color c = bmp.GetPixel(x, y);
                if (c.R < 20 && c.G < 20 && c.B < 20) {
                    res.SetPixel(x, y, Color.FromArgb(0, c.R, c.G, c.B));
                } else {
                    res.SetPixel(x, y, c);
                }
            }
        }
        bmp.Dispose();
        res.Save(path, ImageFormat.Png);
        res.Dispose();
        Console.WriteLine("Done");
    }
}
