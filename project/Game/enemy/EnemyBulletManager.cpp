#include "EnemyBulletManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h" // 追加
#include <algorithm>

namespace {
    constexpr float kEnemyBulletScale = 0.2f;
    constexpr float kEnemyBulletRadius = 0.2f;
}

void EnemyBulletManager::Initialize() {
    bullets_.clear();
    bullets_.resize(kMaxBullets);
    for (auto &bullet : bullets_) {
        bullet.object = std::make_unique<Object3d>();
        bullet.object->Initialize(Object3dCommon::GetInstance());
        bullet.object->SetModel("EnemyBox"); // 自機や敵と同じモデルを使い回し
        bullet.object->SetScale({ kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale }); // 小さくする
        if (bullet.object->GetModel()) {
            bullet.object->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 赤色
        }
        bullet.isDead = true; // 最初は非アクティブに設定
        bullet.position = { -9999.0f, -9999.0f, -9999.0f }; // 中心(0,0,0)に残らないよう画面外に退避
        bullet.elapsedFrames = 0;
        bullet.phaseOffset = 0.0f;
        bullet.waveSign = 1.0f;
        bullet.spiralSpeed = 0.12f;
        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
}

void EnemyBulletManager::Update(Player *player, std::vector<Vector3> &hitPositions, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) {
            bullet.position = { -9999.0f, -9999.0f, -9999.0f };
            if (bullet.object) {
                bullet.object->SetTranslate(bullet.position);
                bullet.object->Update();
            }
            continue; // 非アクティブな弾はスキップ
        }

        // 1. 移動処理
        if (bullet.isHoming && player && !player->IsDead()) {
            bullet.elapsedFrames++;
            if (bullet.elapsedFrames >= 15) { // 15フレーム目から本格的に誘導開始
                Vector3 toPlayer = { player->GetPosition().x - bullet.position.x, player->GetPosition().y - bullet.position.y, player->GetPosition().z - bullet.position.z };
                float lenSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
                if (lenSq > 0.001f) {
                    float len = std::sqrt(lenSq);
                    Vector3 desiredDir = { toPlayer.x / len, toPlayer.y / len, toPlayer.z / len };
                    Vector3 currentDir = { bullet.velocity.x, bullet.velocity.y, bullet.velocity.z };
                    float currentSpeed = std::sqrt(currentDir.x * currentDir.x + currentDir.y * currentDir.y + currentDir.z * currentDir.z);
                    if (currentSpeed > 0.001f) {
                        currentDir = { currentDir.x / currentSpeed, currentDir.y / currentSpeed, currentDir.z / currentSpeed };
                    } else {
                        currentDir = desiredDir;
                    }

                    // 旋回強度。プレイヤーのミサイルを模倣
                    float actualHomingStrength = 0.04f; // やや緩めにして直線的になるのを防ぐ
                    if (len < 30.0f) {
                        // 近づくにつれて誘導を強くする
                        float t = len / 30.0f;
                        actualHomingStrength = (std::max)(actualHomingStrength, 1.0f - t);
                    }
                    actualHomingStrength = std::clamp(actualHomingStrength, 0.0f, 1.0f);

                    // ベクトルをブレンド
                    Vector3 homingDir = {
                        currentDir.x * (1.0f - actualHomingStrength) + desiredDir.x * actualHomingStrength,
                        currentDir.y * (1.0f - actualHomingStrength) + desiredDir.y * actualHomingStrength,
                        currentDir.z * (1.0f - actualHomingStrength) + desiredDir.z * actualHomingStrength
                    };
                    // 正規化
                    float homingLenSq = homingDir.x * homingDir.x + homingDir.y * homingDir.y + homingDir.z * homingDir.z;
                    if (homingLenSq > 0.0001f) {
                        float homingLen = std::sqrt(homingLenSq);
                        homingDir = { homingDir.x / homingLen, homingDir.y / homingLen, homingDir.z / homingLen };
                    }

                    // うねうね（スパイラル）用の直交ベクトルを計算
                    Vector3 rightVec = MyMath::Normalize(MyMath::Cross(homingDir, { 0.0f, 1.0f, 0.0f }));
                    if (rightVec.x * rightVec.x + rightVec.y * rightVec.y + rightVec.z * rightVec.z < 0.001f) {
                        rightVec = { 1.0f, 0.0f, 0.0f };
                    }
                    Vector3 upVec = MyMath::Normalize(MyMath::Cross(rightVec, homingDir));

                    float theta = (float)bullet.elapsedFrames * bullet.spiralSpeed + bullet.phaseOffset;
                    
                    // 距離や寿命に応じてうねりをブレンド
                    float fade = std::clamp(len / 40.0f, 0.0f, 1.0f);
                    if (bullet.lifeTimer < 60) {
                        fade *= ((float)bullet.lifeTimer / 60.0f);
                    }
                    float amplitude = 0.35f * fade; // うねりの強さ

                    Vector3 wave = {
                        rightVec.x * std::cos(theta) * amplitude + upVec.x * std::sin(theta) * bullet.waveSign * amplitude,
                        rightVec.y * std::cos(theta) * amplitude + upVec.y * std::sin(theta) * bullet.waveSign * amplitude,
                        rightVec.z * std::cos(theta) * amplitude + upVec.z * std::sin(theta) * bullet.waveSign * amplitude
                    };

                    // 進行方向とうねりを合成
                    Vector3 finalDir = {
                        homingDir.x * 2.0f + wave.x,
                        homingDir.y * 2.0f + wave.y,
                        homingDir.z * 2.0f + wave.z
                    };
                    float finalLenSq = finalDir.x * finalDir.x + finalDir.y * finalDir.y + finalDir.z * finalDir.z;
                    if (finalLenSq > 0.0001f) {
                        float finalLen = std::sqrt(finalLenSq);
                        finalDir = { finalDir.x / finalLen, finalDir.y / finalLen, finalDir.z / finalLen };
                    }

                    bullet.velocity.x = finalDir.x * bullet.speed;
                    bullet.velocity.y = finalDir.y * bullet.speed;
                    bullet.velocity.z = finalDir.z * bullet.speed;
                }
            }
        } else if (bullet.isHoming) {
            bullet.elapsedFrames++;
        }

        bullet.position.x += bullet.velocity.x;
        bullet.position.y += bullet.velocity.y;
        bullet.position.z += bullet.velocity.z;

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();

        // 2. 寿命チェック
        bullet.lifeTimer--;
        if (bullet.lifeTimer <= 0) {
            bullet.isDead = true;
            continue;
        }

        // 3. 障害物との当たり判定
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = bullet.position;
        bulletSphere.radius = bullet.collisionRadius;

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
            continue; // 障害物に当たったら以降の判定はスキップ
        }

        if (!player->IsDead()) {
            OBB playerOBB = player->GetOBB();
            if (MyMath::IsCollision(bulletSphere, playerOBB)) {
                bullet.isDead = true; // 弾を消す

                // プレイヤーにダメージを与える
                player->TakeDamage(bullet.damage);

                if (!player->IsDead()) {
                    hitPositions.push_back(player->GetPosition());
                }
            }
        }
    }
}

void EnemyBulletManager::UpdateModels() {
    for (auto &bullet : bullets_) {
        if (bullet.isDead || !bullet.object) {
            continue;
        }

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
}

void EnemyBulletManager::Draw() {
    for (auto &bullet : bullets_) {
        if (!bullet.isDead) { // アクティブな弾のみ描画
            bullet.object->Draw();
        }
    }
}

void EnemyBulletManager::Shoot(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale },
                    kEnemyBulletRadius, 1, 120, "EnemyBox");
}

void EnemyBulletManager::ShootHeavyCannon(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 1.2f, 1.2f, 2.8f }, 1.2f, 2, 240, "BossCannon");
}

void EnemyBulletManager::ShootBeam(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 3.5f, 3.5f, 14.0f }, 3.5f, 3, 150, "BossBeam");
}

void EnemyBulletManager::ShootMissile(const Vector3 &position, const Vector3 &velocity) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            
            // 初期の拡散方向を計算する
            float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
            float speed = (speedSq > 0.0001f) ? std::sqrt(speedSq) : 0.35f;
            Vector3 forward = { velocity.x / speed, velocity.y / speed, velocity.z / speed };
            
            Vector3 right = MyMath::Normalize(MyMath::Cross(forward, { 0.0f, 1.0f, 0.0f }));
            if (right.x * right.x + right.y * right.y + right.z * right.z < 0.001f) {
                right = { 1.0f, 0.0f, 0.0f };
            }
            Vector3 up = MyMath::Normalize(MyMath::Cross(right, forward));
            
            float spreadX = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
            float spreadY = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
            
            // 初速に対して、横・上への広がりを与える
            Vector3 initialVel = velocity;
            initialVel.x += right.x * spreadX * 0.12f + up.x * spreadY * 0.12f;
            initialVel.y += right.y * spreadX * 0.12f + up.y * spreadY * 0.12f;
            initialVel.z += right.z * spreadX * 0.12f + up.z * spreadY * 0.12f;
            
            bullet.velocity = initialVel;
            bullet.lifeTimer = 240; // 4秒
            bullet.collisionRadius = 0.5f;
            bullet.damage = 1;
            bullet.isHoming = true;
            
            float newSpeedSq = initialVel.x * initialVel.x + initialVel.y * initialVel.y + initialVel.z * initialVel.z;
            bullet.speed = (newSpeedSq > 0.0001f) ? std::sqrt(newSpeedSq) : 0.35f;
            
            bullet.elapsedFrames = 0;
            bullet.phaseOffset = (float)(rand() % 360) * 0.0174532925f;
            bullet.waveSign = (rand() % 2 == 0) ? 1.0f : -1.0f;
            bullet.spiralSpeed = 0.12f + (float)(rand() % 8) * 0.01f;
            
            bullet.isDead = false;

            bullet.object->SetModel("EnemyBox");
            bullet.object->SetScale({ 0.35f, 0.35f, 0.7f });
            if (bullet.object->GetModel()) {
                bullet.object->GetModel()->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f }); // オレンジ色
            }
            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break;
        }
    }
}

void EnemyBulletManager::ShootConfigured(const Vector3 &position, const Vector3 &velocity, const Vector3 &scale,
                                         float collisionRadius, int damage, int lifeTimer, const char *modelName) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            bullet.velocity = velocity;
            bullet.lifeTimer = lifeTimer;
            bullet.collisionRadius = collisionRadius;
            bullet.damage = damage;
            bullet.isHoming = false;
            bullet.elapsedFrames = 0;
            bullet.isDead = false;

            bullet.object->SetModel(modelName);
            bullet.object->SetScale(scale);
            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break; // 発射したので終了
        }
    }
}

