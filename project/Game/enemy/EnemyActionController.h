#pragma once

class Enemy;

// 敵の被弾・撃破・追跡開始に伴う状態遷移を担当する。
class EnemyActionController {
public:
    void Destroy(Enemy& enemy) const;
    void TakeDamage(Enemy& enemy, int damage) const;
    void StartChasing(Enemy& enemy) const;
};
