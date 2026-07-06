using System;
using System.IO;
using System.Text.RegularExpressions;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.h";
        string content = File.ReadAllText(path);

        string search = @"    // 蠅・阜謗･霑題ｭｦ蜻顔畑繧ｲ繝・ち繝ｼ
    bool IsNearBoundary\(\) const { return isNearBoundary_; }
    float GetBoundaryWarningIntensity\(\) const { return boundaryWarningIntensity_; }
    Vector3 GetBoundaryAlertPosition\(\) const { return boundaryAlertPosition_; }
    Vector3 GetBoundaryAlertNormal\(\) const { return boundaryAlertNormal_; }";

        string replace = @"    // 蠅・阜謗･霑題ｭｦ蜻顔畑繧ｲ繝・ち繝ｼ
    struct BoundaryAlert {
        Vector3 position;
        Vector3 normal;
        float intensity;
    };
    bool IsNearBoundary() const { return isNearBoundary_; }
    const std::vector<BoundaryAlert>& GetBoundaryAlerts() const { return boundaryAlerts_; }";

        content = content.Replace(search, replace);

        string search2 = @"    // 蠅・阜謗･霑題ｭｦ蜻顔畑
    bool isNearBoundary_ = false;
    float boundaryWarningIntensity_ = 0.0f;
    Vector3 boundaryAlertPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 boundaryAlertNormal_ = {0.0f, 0.0f, 1.0f};";

        string replace2 = @"    // 蠅・阜謗･霑題ｭｦ蜻顔畑
    bool isNearBoundary_ = false;
    std::vector<BoundaryAlert> boundaryAlerts_;";

        content = content.Replace(search2, replace2);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done");
    }
}
