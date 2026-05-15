#pragma once
#include <list>
#include "Collider.h"
class CollisionManager {
public:
    // リストをリセットする（毎フレームの最初などに呼ぶ）
    void Clear();

    // 当たり判定のリストに登録する（毎フレーム、オブジェクトのUpdate内で呼ぶ）
    void AddCollider(Collider *collider);

    // すべての当たり判定をチェックする
    void CheckAllCollisions();

private:
    // 登録されたすべてのコライダー
    std::list<Collider *> colliders_;
};

