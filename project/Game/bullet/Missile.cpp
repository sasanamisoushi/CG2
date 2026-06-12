#include "Missile.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include <cmath>

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

void Missile::Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type) {
	type_ = type; // タイプを保存

	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel("Sphere");

	// タイプによって見た目を変える
	if (type_ == MissileType::Normal) {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 通常弾は黄色
		object_->SetScale({ 0.3f, 0.3f, 0.3f }); // 通常弾は小さめ
		collisionRadius_ = 0.3f;
	} else {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // ミサイルは赤
		object_->SetScale({ 0.5f, 0.5f, 0.5f });
		collisionRadius_ = 0.5f;
	}



	// プレイヤーから渡された初期座標と速度をセット
	position_ = position;
	velocity_ = velocity;

	lifeTimer_ = (type_ == MissileType::MissileWithTrail) ? kHomingLifeTime : kLifeTime;
	isDead_ = false;

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


	// ホーミング（追尾）処理
	if (type_ == MissileType::MissileWithTrail && enemy) {
		const Vector3 targetPos = enemy->GetPosition();
		const Vector3 desiredDirection = NormalizeOr(Subtract(targetPos, position_), NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f }));
		const Vector3 currentDirection = NormalizeOr(velocity_, desiredDirection);
		const Vector3 homingDirection = BlendDirection(currentDirection, desiredDirection, 0.085f);
		const float homingSpeed = 0.75f;
		velocity_ = Scale(homingDirection, homingSpeed);
	} else if (type_ == MissileType::MissileWithTrail) {
		velocity_ = Scale(NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f }), 0.75f);
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

	// 座標をモデルに適用して更新
	object_->SetTranslate(position_);
	object_->Update();

	// ミサイルタイプのみトレイルを更新
	if (type_ == MissileType::MissileWithTrail && trail_ && trailObject_) {
		trail_->Update(position_);
		std::vector<VertexData> trailVertices = trail_->GenerateVertices(camera, 0.5f);
		if (trailObject_->GetModel()) {
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
