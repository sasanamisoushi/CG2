#pragma once
#include <cstdint>
#include "MyMath.h"

// 当たり判定の形状（シェイプ）の種類
enum class ColliderShape {
    SPHERE, // 球体
    OBB     // 有向境界ボックス
};

class Collider {
public:
    // 衝突属性（自分は何者か。デフォルトは全てに当たる）
    uint32_t collisionAttribute_ = 0xFFFFFFFF;
    // 衝突マスク（誰と当たるか。デフォルトは全てと当たる）
    uint32_t collisionMask_ = 0xFFFFFFFF;

    virtual ~Collider() = default;

    // 衝突した瞬間に呼ばれる関数（派生クラスで具体的な処理を書く）
    virtual void OnCollision(Collider *other) = 0;

    // =====================================
       // 形状情報の設定と取得
       // =====================================

       // 自分の形状がどちらなのかを返す（デフォルトは球体にしておく）
    virtual ColliderShape GetShapeType() const { return ColliderShape::SPHERE; }

    // [Sphere用] の情報
    virtual Vector3 GetWorldPosition() const { return { 0,0,0 }; }
    virtual float GetRadius() const { return 1.0f; }

    // [OBB用] の情報（派生クラスでオーバーライドして使います）
    virtual OBB GetOBB() const { return OBB(); }
};

