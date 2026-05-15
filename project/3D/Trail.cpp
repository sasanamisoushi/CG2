#include "Trail.h"

void Trail::Initialize(int maxPoints) {
	maxPoints_ = maxPoints;
	points_.clear();
}

void Trail::Update(const Vector3 &currentPos) {
    // 最新の座標を先頭に追加
    points_.push_front(currentPos);

    // 履歴が上限を超えたら、一番古い尻尾の座標を消す
    if (points_.size() > maxPoints_) {
        points_.pop_back();
    }
}

std::vector<VertexData> Trail::GenerateVertices(Camera *camera, float thickness) {
    std::vector<VertexData> vertices;

    // 点が2つ未満なら線を引けないので空を返す
    if (points_.size() < 2) return vertices;

    // カメラの現在座標を取得
    Vector3 camPos = camera->GetTranslate();

    // 各ポイントに対して、左右の頂点を2つずつ作る
    for (size_t i = 0; i < points_.size(); ++i) {
        Vector3 dir = { 0, 0, 0 };

        // 1. 軌跡の「進行方向（dir）」を計算する
        if (i < points_.size() - 1) {
            // 次の点へのベクトル
            dir = { points_[i].x - points_[i + 1].x, points_[i].y - points_[i + 1].y, points_[i].z - points_[i + 1].z };
        } else {
            // 最後の点は1つ前の方向をそのまま使う
            dir = { points_[i - 1].x - points_[i].x, points_[i - 1].y - points_[i].y, points_[i - 1].z - points_[i].z };
        }
        dir = MyMath::Normalize(dir);

        // 2. その点から「カメラに向かう方向（toCam）」を計算する
        Vector3 toCam = { camPos.x - points_[i].x, camPos.y - points_[i].y, camPos.z - points_[i].z };
        toCam = MyMath::Normalize(toCam);

        // 3. 外積（Cross）を使って、進行方向とカメラ方向の両方に垂直な「左右の軸（right）」を作る！
        // これにより、どんなにぐねぐね曲がっても常にカメラの方を向くリボンになります。
        Vector3 right = MyMath::Cross(dir, toCam);
        right = MyMath::Normalize(right);

        // 4. 尻尾にいくほど細くなるようにする（先頭は1.0、尻尾は0.0）
        float progress = 1.0f - ((float)i / (points_.size() - 1));
        float currentThickness = thickness * progress;

        // 左右の頂点座標
        Vector3 leftPos = {
            points_[i].x - right.x * currentThickness,
            points_[i].y - right.y * currentThickness,
            points_[i].z - right.z * currentThickness
        };
        Vector3 rightPos = {
            points_[i].x + right.x * currentThickness,
            points_[i].y + right.y * currentThickness,
            points_[i].z + right.z * currentThickness
        };

        // 5. 頂点データの生成（古いほど透明にするなどの色設定）
        VertexData vLeft, vRight;

        // 左側の頂点
        vLeft.position = { leftPos.x, leftPos.y, leftPos.z, 1.0f };
        vLeft.normal = { toCam.x, toCam.y, toCam.z, 0.0f }; // 法線はカメラの方向
        vLeft.texcoord = { 0.0f, 1.0f - progress, 0.0f, 0.0f }; // V方向にスクロールするようにUVを設定
        vLeft.color = { 1.0f, 1.0f, 1.0f, progress }; // 尻尾ほど透明（アルファ値）

        // 右側の頂点
        vRight.position = { rightPos.x, rightPos.y, rightPos.z, 1.0f };
        vRight.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vRight.texcoord = { 1.0f, 1.0f - progress, 0.0f, 0.0f };
        vRight.color = { 1.0f, 1.0f, 1.0f, progress };

        // リストに追加（TriangleStripで描画される前提の順番）
        vertices.push_back(vLeft);
        vertices.push_back(vRight);
    }

    return vertices;
}
