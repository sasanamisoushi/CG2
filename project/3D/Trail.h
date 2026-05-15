#pragma once
#include <vector>
#include <deque>
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include "3D/Model.h"

class Trail {
public:
    // maxPoints: 軌跡の長さ（何フレーム分の過去座標を保持するか）
    void Initialize(int maxPoints = 30);

    // 毎フレーム、ミサイル（発生源）の現在座標を渡して更新する
    void Update(const Vector3 &currentPos);

    // カメラの情報を元に、常にカメラの方を向く「板ポリゴンの頂点リスト」を生成する
    std::vector<VertexData> GenerateVertices(Camera *camera, float thickness);

private:
    // 過去の座標の履歴（追加・削除が早い deque を使います）
    std::deque<Vector3> points_;
    int maxPoints_ = 30;
};

