#include "EnemyBulletManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h" // 霑ｽ蜉

namespace {
    constexpr float kEnemyBulletScale = 0.2f;
    constexpr float kEnemyBulletRadius = 0.2f;
    constexpr float kPlayerHitRadius = 0.4f;
}

void EnemyBulletManager::Initialize() {
    bullets_.clear();
    bullets_.resize(kMaxBullets);
    for (auto &bullet : bullets_) {
        bullet.object = std::make_unique<Object3d>();
        bullet.object->Initialize(Object3dCommon::GetInstance());
        bullet.object->SetModel("EnemyBox"); // 閾ｪ讖溘ｄ謨ｵ縺ｨ蜷後§繝｢繝・Ν繧剃ｽｿ縺・屓縺・
        bullet.object->SetScale({ kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale }); // 蟆上＆縺上☆繧・
        if (bullet.object->GetModel()) {
            bullet.object->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 襍､濶ｲ
        }
        bullet.isDead = true; // 譛蛻昴・髱槭い繧ｯ繝・ぅ繝悶↓險ｭ螳・
    }
}

void EnemyBulletManager::Update(Player *player, std::vector<Vector3> &hitPositions, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) continue; // 髱槭い繧ｯ繝・ぅ繝悶↑蠑ｾ縺ｯ繧ｹ繧ｭ繝・・

        // 1. 遘ｻ蜍募・逅・
        bullet.position.x += bullet.velocity.x;
        bullet.position.y += bullet.velocity.y;
        bullet.position.z += bullet.velocity.z;

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();

        // 2. 蟇ｿ蜻ｽ繝√ぉ繝・け
        bullet.lifeTimer--;
        if (bullet.lifeTimer <= 0) {
            bullet.isDead = true;
            continue;
        }

        // 3. 髫懷ｮｳ迚ｩ縺ｨ縺ｮ蠖薙◆繧雁愛螳・
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = bullet.position;
        bulletSphere.radius = kEnemyBulletRadius;

        for (const auto& obstacle : obstacles) {
            if (obstacle->IsStageBounds()) {
                continue;
            }

            OBB obsOBB = obstacle->GetOBB();
            if (MyMath::IsCollision(bulletSphere, obsOBB)) {
                bullet.isDead = true;
                hitObstacle = true;
                break;
            }
        }

        if (hitObstacle) {
            continue; // 髫懷ｮｳ迚ｩ縺ｫ蠖薙◆縺｣縺溘ｉ莉･髯阪・蛻､螳壹・繧ｹ繧ｭ繝・・
        }

        // 繝励Ξ繧､繝､繝ｼ縺ｮ蠎ｧ讓吶ｒ蜿門ｾ・
        Vector3 playerPos = player->GetPosition();

        if (!player->IsDead()) {
            float dx = playerPos.x - bullet.position.x;
            float dy = playerPos.y - bullet.position.y;
            float dz = playerPos.z - bullet.position.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            // 蜊雁ｾ・m莉･蜀・↑繧峨ヲ繝・ヨ
            if (distSq < kPlayerHitRadius * kPlayerHitRadius) {
                bullet.isDead = true; // 蠑ｾ繧呈ｶ医☆

                // 繝励Ξ繧､繝､繝ｼ縺ｫ繝繝｡繝ｼ繧ｸ繧剃ｸ弱∴繧具ｼ亥偵☆・会ｼ・
                player->TakeDamage(1);

                // 蠖薙◆縺｣縺溷ｴ謇・郁・讖溘・蠎ｧ讓呻ｼ峨ｒ辷・匱繝ｪ繧ｹ繝医↓蝣ｱ蜻奇ｼ・
                hitPositions.push_back(playerPos);
            }
        }
    }
}

void EnemyBulletManager::Draw() {
    for (auto &bullet : bullets_) {
        if (!bullet.isDead) { // 繧｢繧ｯ繝・ぅ繝悶↑蠑ｾ縺ｮ縺ｿ謠冗判
            bullet.object->Draw();
        }
    }
}

void EnemyBulletManager::Shoot(const Vector3 &position, const Vector3 &velocity) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 菴ｿ逕ｨ蜿ｯ閭ｽ縺ｪ蠑ｾ繧呈､懃ｴ｢
            bullet.position = position;
            bullet.velocity = velocity;
            bullet.lifeTimer = 120;
            bullet.isDead = false;

            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break; // 逋ｺ蟆・＠縺溘・縺ｧ邨ゆｺ・
        }
    }
}

