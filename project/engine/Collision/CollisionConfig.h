#pragma once
#include <cstdint>

// 当たり判定の属性（自分自身が何者か）
// ビットシフト（0b0001, 0b0010...）で定義することで、複数の属性を組み合わせられます
const uint32_t kCollisionAttributePlayer = 0b0001; // プレイヤー
const uint32_t kCollisionAttributeEnemy = 0b0010; // 敵
const uint32_t kCollisionAttributePlayerBullet = 0b0100; // プレイヤーの弾
const uint32_t kCollisionAttributeEnemyBullet = 0b1000; // 敵の弾
