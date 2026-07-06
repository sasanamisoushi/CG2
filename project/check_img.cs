using System;
using System.Drawing;

class Program {
    static void Main() {
        try {
            using (Bitmap bmp = new Bitmap(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png")) {
                Console.WriteLine("Image size: " + bmp.Width + "x" + bmp.Height);
                int minX = bmp.Width, minY = bmp.Height, maxX = 0, maxY = 0;
                for (int y = 0; y < bmp.Height; y++) {
                    for (int x = 0; x < bmp.Width; x++) {
                        if (bmp.GetPixel(x, y).A > 0) {
                            if (x < minX) minX = x;
                            if (y < minY) minY = y;
                            if (x > maxX) maxX = x;
                            if (y > maxY) maxY = y;
                        }
                    }
                }
                Console.WriteLine(string.Format("Bounding box: {0},{1} to {2},{3}", minX, minY, maxX, maxY));
            }
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
