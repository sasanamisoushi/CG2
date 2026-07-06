using System;
using System.Drawing;
using System.Drawing.Imaging;

class Program {
    static void DrawLockOnReticle(string path) {
        using (Bitmap bmp = new Bitmap(128, 128, PixelFormat.Format32bppArgb)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                g.Clear(Color.Transparent);
                
                // Yellow lock-on corners like Macross
                using (Pen pen = new Pen(Color.FromArgb(255, 255, 255, 0), 6)) { // Yellow, 6px thick
                    // Top-Left
                    g.DrawLine(pen, 4, 4, 32, 4);
                    g.DrawLine(pen, 4, 4, 4, 32);
                    // Top-Right
                    g.DrawLine(pen, 124, 4, 96, 4);
                    g.DrawLine(pen, 124, 4, 124, 32);
                    // Bottom-Left
                    g.DrawLine(pen, 4, 124, 32, 124);
                    g.DrawLine(pen, 4, 124, 4, 96);
                    // Bottom-Right
                    g.DrawLine(pen, 124, 124, 96, 124);
                    g.DrawLine(pen, 124, 124, 124, 96);
                }

                // Add a small inner crosshair/brackets in green
                using (Pen innerPen = new Pen(Color.FromArgb(255, 0, 255, 0), 3)) {
                    g.DrawLine(innerPen, 64, 48, 64, 56);
                    g.DrawLine(innerPen, 64, 80, 64, 72);
                    g.DrawLine(innerPen, 48, 64, 56, 64);
                    g.DrawLine(innerPen, 80, 64, 72, 64);
                }
            }
            bmp.Save(path, ImageFormat.Png);
        }
        Console.WriteLine("Created: " + path);
    }

    static void DrawAimCursor(string path) {
        using (Bitmap bmp = new Bitmap(128, 128, PixelFormat.Format32bppArgb)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                g.Clear(Color.Transparent);
                
                // Green circle and crosshair
                using (Pen pen = new Pen(Color.FromArgb(255, 50, 255, 50), 4)) {
                    g.DrawEllipse(pen, 16, 16, 96, 96);
                    g.DrawLine(pen, 64, 0, 64, 16);
                    g.DrawLine(pen, 64, 128, 64, 112);
                    g.DrawLine(pen, 0, 64, 16, 64);
                    g.DrawLine(pen, 128, 64, 112, 64);
                }
                using (SolidBrush brush = new SolidBrush(Color.FromArgb(255, 255, 0, 0))) {
                    g.FillEllipse(brush, 60, 60, 8, 8); // Red center dot
                }
            }
            bmp.Save(path, ImageFormat.Png);
        }
        Console.WriteLine("Created: " + path);
    }

    static void Main() {
        try {
            DrawLockOnReticle(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\lock_on_reticle.png");
            DrawAimCursor(@"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\aim_cursor.png");
        } catch (Exception ex) {
            Console.WriteLine(ex.Message);
        }
    }
}
