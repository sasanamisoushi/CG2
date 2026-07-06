using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp";
        string content = File.ReadAllText(path);

        string search = @"			float warningThreshold = 50.0f; // 50 units away starts the warning
			float minDistanceToWall = 99999.0f;
			Vector3 closestNormal = {0.0f, 0.0f, 1.0f};

			for (int axis = 0; axis < 3; ++axis) {
				const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - playerProjection[axis]);
				
				// 警告判定用の距離計算
				if (axis != 1) { // Y軸（天井・床）は警告から除外する
					float distPositive = limit - localDistance[axis];
					if (distPositive < minDistanceToWall) {
						minDistanceToWall = distPositive;
						closestNormal = obsOBB.orientations[axis];
					}
					
					float distNegative = limit + localDistance[axis];
					if (distNegative < minDistanceToWall) {
						minDistanceToWall = distNegative;
						closestNormal = {-obsOBB.orientations[axis].x, -obsOBB.orientations[axis].y, -obsOBB.orientations[axis].z};
					}
				}

				if (localDistance[axis] > limit) {";

        string replace = @"			float warningThreshold = 50.0f; // 50 units away starts the warning
			boundaryAlerts_.clear();

			for (int axis = 0; axis < 3; ++axis) {
				const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - playerProjection[axis]);
				
				// 警告判定用の距離計算
				float distPositive = limit - localDistance[axis];
				if (distPositive < warningThreshold) {
					BoundaryAlert alert;
					alert.normal = obsOBB.orientations[axis];
					alert.position = {
						playerOBB.center.x + alert.normal.x * distPositive,
						playerOBB.center.y + alert.normal.y * distPositive,
						playerOBB.center.z + alert.normal.z * distPositive
					};
					alert.intensity = 1.0f - (std::max)(0.0f, distPositive) / warningThreshold;
					boundaryAlerts_.push_back(alert);
				}
				
				float distNegative = limit + localDistance[axis];
				if (distNegative < warningThreshold) {
					BoundaryAlert alert;
					alert.normal = {-obsOBB.orientations[axis].x, -obsOBB.orientations[axis].y, -obsOBB.orientations[axis].z};
					alert.position = {
						playerOBB.center.x + alert.normal.x * distNegative,
						playerOBB.center.y + alert.normal.y * distNegative,
						playerOBB.center.z + alert.normal.z * distNegative
					};
					alert.intensity = 1.0f - (std::max)(0.0f, distNegative) / warningThreshold;
					boundaryAlerts_.push_back(alert);
				}

				if (localDistance[axis] > limit) {";

        content = content.Replace(search, replace);

        string search2 = @"			// 警告フラグの更新
			if (minDistanceToWall < warningThreshold) {
				isNearBoundary_ = true;
				boundaryWarningIntensity_ = 1.0f - (std::max)(0.0f, minDistanceToWall) / warningThreshold;
				boundaryAlertNormal_ = closestNormal;
				
				// プレイヤーの位置から壁の方向（closestNormal）へ距離分進んだ位置
				boundaryAlertPosition_ = {
					playerOBB.center.x + closestNormal.x * minDistanceToWall,
					playerOBB.center.y + closestNormal.y * minDistanceToWall,
					playerOBB.center.z + closestNormal.z * minDistanceToWall
				};
			}";

        string replace2 = @"			// 警告フラグの更新
			isNearBoundary_ = !boundaryAlerts_.empty();";

        content = content.Replace(search2, replace2);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done");
    }
}
