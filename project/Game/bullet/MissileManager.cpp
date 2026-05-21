#include "MissileManager.h"
#include "Game/enemy/Enemy.h"

void MissileManager::Initialize() {
	missiles_.clear();
}

void MissileManager::Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies, std::vector<Vector3> &hitPositions) {
    for (auto it = missiles_.begin(); it != missiles_.end(); ) {
        Missile *missile = it->get();

        // ==========================================
        // 1. 最も近い敵をサーチ（索敵）
        // ==========================================
        Enemy *targetEnemy = nullptr;
        float minDistSq = 99999999.0f; // 最小距離を保存する変数（最初は凄く大きな数）
        Vector3 mPos = missile->GetPosition();

        for (auto &enemy : enemies) {
            if (!enemy->IsDead()) {
                Vector3 ePos = enemy->GetPosition();
                float dx = ePos.x - mPos.x;
                float dy = ePos.y - mPos.y;
                float dz = ePos.z - mPos.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                // 今までの敵より近かったらターゲットを更新
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    targetEnemy = enemy.get();
                }
            }
        }

        // ==========================================
        // 2. ミサイルの更新（見つけたターゲットを渡す）
        // ==========================================
        missile->Update(camera, targetEnemy);

        // ==========================================
        // 3. 移動後の最新座標で当たり判定
        // ==========================================
        mPos = missile->GetPosition();
        for (auto &enemy : enemies) {
            if (!enemy->IsDead() && !missile->IsDead()) {
                Vector3 ePos = enemy->GetPosition();
                float dx = ePos.x - mPos.x;
                float dy = ePos.y - mPos.y;
                float dz = ePos.z - mPos.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                float radius = 1.0f; // 当たり判定の大きさ
                if (distSq <= radius * radius) {
                    missile->OnCollision();
                    enemy->OnCollision();

                    // 当たった場所（敵の中心）を爆発リストに報告！
                    hitPositions.push_back(ePos);
                }
            }
        }

        // ==========================================
        // 4. 寿命・衝突で死んでいたら削除
        // ==========================================
        if (missile->IsDead()) {
            it = missiles_.erase(it);
        } else {
            ++it;
        }
    }
}

void MissileManager::Draw() {
	for (const auto &missile : missiles_) {
		missile->Draw();
	}
}

void MissileManager::Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type) {
	// 新しいミサイルを生み出してリストに追加する
	auto newMissile = std::make_unique<Missile>();
	newMissile->Initialize(position, velocity, type);

	missiles_.push_back(std::move(newMissile));
}