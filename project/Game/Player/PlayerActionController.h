#pragma once

class Player;

// 被弾・撃破・必殺技照準・強化状態といった、プレイヤーの行動状態を担当する。
class PlayerActionController {
public:
    void Destroy(Player& player) const;
    void TakeDamage(Player& player, int damage) const;
    void SetSpecialAttackActive(Player& player, bool active) const;
    void SetSongActive(Player& player, bool active) const;
};
