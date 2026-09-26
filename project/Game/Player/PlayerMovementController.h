#pragma once

class Player;

// プレイヤーの入力解釈、姿勢制御、移動、回避、近接攻撃だけを担当する。
// 描画・衝突・カメラ・アニメーションの更新は Player 側の別責務として残す。
class PlayerMovementController {
public:
    void Update(Player& player, bool rotationLocked) const;
};
