#include "MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h" // 追加

void MissileManager::Initialize() {
	missiles_.clear();
}

void MissileManager::Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies, const std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<Vector3> &hitPositions, Enemy *preferredTarget) {
    for (auto it = missiles_.begin(); it != missiles_.end(); ) {
        Missile *missile = it->get();

        // ==========================================
        // 1. ロック対象がいれば優先し、なければ最も近い敵をサーチ（索敵）
        // ==========================================
        Enemy *targetEnemy = (preferredTarget && !preferredTarget->IsDead()) ? preferredTarget : nullptr;
        float minDistSq = 99999999.0f; // 最小距離を保存する変数（最初は凄く大きな数）
        Vector3 mPos = missile->GetPosition();

        if (!targetEnemy) {
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
        }

        // ==========================================
        // 2. ミサイルの更新（見つけたターゲットを渡す）
        // ==========================================
        missile->Update(camera, targetEnemy);

        // ==========================================
        // 3. 移動後の最新座標で当たり判定
        // ==========================================
        mPos = missile->GetPosition();

        // 3-1. 障害物との当たり判定
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = mPos;
        bulletSphere.radius = missile->GetCollisionRadius();

        for (const auto& obstacle : obstacles) {
            if (obstacle->IsStageBounds()) {
                continue;
            }

            OBB obsOBB = obstacle->GetOBB();
            if (MyMath::IsCollision(bulletSphere, obsOBB)) {
                missile->OnCollision(); // または直接 isDead = true; OnCollision() の中に dead にする処理がある想定
                hitObstacle = true;
                // 障害物に当たった場合も爆発させるかはお好みですが、ここでは要望の「消える」のみ実装。
                // 爆発させたい場合は hitPositions.push_back(mPos); を追加します。
                break;
            }
        }

        if (!hitObstacle) {
            for (auto &enemy : enemies) {
                if (!enemy->IsDead() && !missile->IsDead()) {
                    Vector3 ePos = enemy->GetPosition();
                    float dx = ePos.x - mPos.x;
                    float dy = ePos.y - mPos.y;
                    float dz = ePos.z - mPos.z;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    float radius = missile->GetCollisionRadius() + enemy->GetCollisionRadius();
                    if (distSq <= radius * radius) {
                        missile->OnCollision();
                        enemy->TakeDamage(1);

                        // 当たった場所（敵の中心）を爆発リストに報告！
                        hitPositions.push_back(ePos);
                    }
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
