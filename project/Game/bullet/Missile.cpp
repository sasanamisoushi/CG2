#include "Missile.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	float LengthSq(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3 &value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3 &a, const Vector3 &b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Subtract(const Vector3 &a, const Vector3 &b) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	Vector3 Scale(const Vector3 &value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	Vector3 NormalizeOr(const Vector3 &value, const Vector3 &fallback) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return fallback;
		}
		return Scale(value, 1.0f / length);
	}

	Vector3 BlendDirection(const Vector3 &from, const Vector3 &to, float blend) {
		return NormalizeOr(Add(Scale(from, 1.0f - blend), Scale(to, blend)), to);
	}
}

void Missile::Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning) {
	type_ = type; // タイプを保存
	tuning_ = tuning;

	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel("Sphere");

	// タイプによって見た目を変える
	if (type_ == MissileType::Normal) {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 通常弾は黄色
	} else {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // ミサイルは赤
	}

	const float visualScale = (std::max)(0.01f, tuning_.scale);
	object_->SetScale({ visualScale, visualScale, visualScale });
	collisionRadius_ = (std::max)(0.01f, tuning_.collisionRadius);



	// プレイヤーから渡された初期座標と速度をセット
	position_ = position;
	velocity_ = velocity;

	lifeTimer_ = (std::max)(1, tuning_.lifeTime);
	isDead_ = false;

	// うねり用パラメータのランダム初期化
	phaseOffset_ = (float)(rand() % 360) * 0.0174532925f;
	waveSign_ = (rand() % 2 == 0) ? 1.0f : -1.0f;
	spiralSpeed_ = 0.12f + (float)(rand() % 8) * 0.01f;

	// ミサイルタイプのみトレイルを準備
	if (type_ == MissileType::MissileWithTrail) {
		trail_ = std::make_unique<Trail>();
		trail_->Initialize(60);
		trailObject_ = std::make_unique<Object3d>();
		trailObject_->Initialize(Object3dCommon::GetInstance());
		trailObject_->SetModel("SmokeTrail");
	}
}

void Missile::Update(Camera *camera, Enemy *enemy) {
	if (isDead_) return;


	// ホーミング（追尾）処理と板野サーカス風うねり軌道
	if (type_ == MissileType::MissileWithTrail) {
		Vector3 targetPos = position_;
		bool hasTarget = false;

		if (enemy) {
			targetPos = enemy->GetPosition();
			hasTarget = true;
		}

		if (hasTarget) {
			// ターゲットへの方向
			Vector3 toTarget = Subtract(targetPos, position_);
			float distToTarget = Length(toTarget);
			Vector3 desiredDirection = NormalizeOr(toTarget, { 0.0f, 0.0f, 1.0f });
			Vector3 currentDirection = NormalizeOr(velocity_, desiredDirection);
			
			// 基本のホーミングブレンド
			Vector3 homingDirection = BlendDirection(currentDirection, desiredDirection, std::clamp(tuning_.homingStrength, 0.0f, 1.0f));

			// 螺旋揺らぎ（板野サーカス効果）の計算
			// 進行方向に垂直な基準軸を作成
			Vector3 rightVec = NormalizeOr(MyMath::Cross(homingDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) {
				rightVec = { 1.0f, 0.0f, 0.0f };
			}
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, homingDirection), { 0.0f, 1.0f, 0.0f });

			// 時間経過による螺旋回転
			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			// ターゲットに近づくにつれて揺らぎを減衰（収束）
			float fade = std::clamp(distToTarget / 40.0f, 0.0f, 1.0f);
			if (lifeTimer_ < 60) {
				fade *= (lifeTimer_ / 60.0f);
			}

			// 揺らぎの大きさ（振幅）
			float amplitude = 1.3f * fade; 
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));

			// 最終的な進行ベクトル
			Vector3 finalDirection = NormalizeOr(Add(homingDirection, wave), homingDirection);
			float homingSpeed = (std::max)(0.01f, tuning_.speed);
			velocity_ = Scale(finalDirection, homingSpeed);
		} else {
			// ターゲットがいない場合でも少しうねりながら直進する
			Vector3 currentDirection = NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f });
			Vector3 rightVec = NormalizeOr(MyMath::Cross(currentDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) rightVec = { 1.0f, 0.0f, 0.0f };
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, currentDirection), { 0.0f, 1.0f, 0.0f });

			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			float amplitude = 0.5f; // 非ターゲット時は弱めのうねり
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));
			
			Vector3 finalDirection = NormalizeOr(Add(currentDirection, wave), currentDirection);
			velocity_ = Scale(finalDirection, (std::max)(0.01f, tuning_.speed));
		}
	}
	// 基本の移動処理
	position_.x += velocity_.x;
	position_.y += velocity_.y;
	position_.z += velocity_.z;

	// 寿命の処理
	lifeTimer_--;
	if (lifeTimer_ <= 0) {
		isDead_ = true; // 寿命が尽きたら死ぬフラグを立てる
	}

	UpdateModel();

	// ミサイルタイプのみトレイルを更新
	if (type_ == MissileType::MissileWithTrail && trail_ && trailObject_) {
		trail_->Update(position_);
		std::vector<VertexData> trailVertices = trail_->GenerateVertices(camera, (std::max)(0.01f, tuning_.trailWidth));
		if (trailObject_->GetModel()) {
			trailObject_->GetModel()->UpdateTrailVertices(trailVertices);
		}
		trailObject_->Update();
	}
}

void Missile::UpdateModel(Camera *camera) {
	if (object_) {
		object_->SetTranslate(position_);
		object_->Update();
	}
	if (type_ == MissileType::MissileWithTrail && trailObject_) {
		if (camera && trail_ && trailObject_->GetModel()) {
			std::vector<VertexData> trailVertices = trail_->GenerateVertices(camera, (std::max)(0.01f, tuning_.trailWidth));
			trailObject_->GetModel()->UpdateTrailVertices(trailVertices);
		}
		trailObject_->Update();
	}
}

void Missile::Draw() {
	if (isDead_) return;

	if (object_) {
		object_->Draw();
	}

	// ミサイルタイプのみトレイルを描画
	if (type_ == MissileType::MissileWithTrail && trailObject_) {
		trailObject_->Draw();
	}
}

void Missile::OnCollision() {
	isDead_ = true;
}
