#include "Missile.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	constexpr bool kEnableMissileTrails = true;
	constexpr int kHomingForwardLaunchFrames = 30;

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
	type_ = type;
	tuning_ = tuning;

	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel("Sphere");

	if (type_ == MissileType::Normal) {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
	} else {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	const float visualScale = (std::max)(0.01f, tuning_.scale);
	object_->SetScale({ visualScale, visualScale, visualScale });
	collisionRadius_ = (std::max)(0.01f, tuning_.collisionRadius);

	position_ = position;
	velocity_ = velocity;

	if (type_ == MissileType::MissileWithTrail) {
		Vector3 forward = NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f });
		Vector3 right = NormalizeOr(MyMath::Cross(forward, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
		Vector3 up = NormalizeOr(MyMath::Cross(right, forward), { 0.0f, 1.0f, 0.0f });
		float spreadX = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
		float spreadY = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
		velocity_ = Add(velocity_, Scale(right, spreadX * 1.5f));
		velocity_ = Add(velocity_, Scale(up, spreadY * 1.5f));
	}

	lifeTimer_ = (std::max)(1, tuning_.lifeTime);
	elapsedFrames_ = 0;
	isDead_ = false;

	phaseOffset_ = (float)(rand() % 360) * 0.0174532925f;
	waveSign_ = (rand() % 2 == 0) ? 1.0f : -1.0f;
	spiralSpeed_ = 0.12f + (float)(rand() % 8) * 0.01f;

	if (kEnableMissileTrails && type_ == MissileType::MissileWithTrail) {
		trail_ = std::make_unique<Trail>();
		trail_->Initialize(120);
		trailObject_ = std::make_unique<Object3d>();
		trailObject_->Initialize(Object3dCommon::GetInstance());
		trailObject_->SetModel("SmokeTrail");
	}
}

void Missile::Update(Camera *camera, Enemy *enemy) {
	if (isDead_) return;

	if (type_ == MissileType::MissileWithTrail && elapsedFrames_ >= kHomingForwardLaunchFrames) {
		Vector3 targetPos = position_;
		bool hasTarget = false;

		if (enemy) {
			targetPos = enemy->GetPosition();
			hasTarget = true;
		}

		if (hasTarget) {
			Vector3 toTarget = Subtract(targetPos, position_);
			float distToTarget = Length(toTarget);
			Vector3 desiredDirection = NormalizeOr(toTarget, { 0.0f, 0.0f, 1.0f });
			Vector3 currentDirection = NormalizeOr(velocity_, desiredDirection);
			
			float actualHomingStrength = tuning_.homingStrength;
			if (distToTarget < 30.0f) {
				actualHomingStrength = (std::max)(actualHomingStrength, 1.0f - (distToTarget / 30.0f));
			}

			Vector3 homingDirection = BlendDirection(currentDirection, desiredDirection, std::clamp(actualHomingStrength, 0.0f, 1.0f));

			Vector3 rightVec = NormalizeOr(MyMath::Cross(homingDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) {
				rightVec = { 1.0f, 0.0f, 0.0f };
			}
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, homingDirection), { 0.0f, 1.0f, 0.0f });

			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			float fade = std::clamp(distToTarget / 40.0f, 0.0f, 1.0f);
			if (lifeTimer_ < 60) {
				fade *= (lifeTimer_ / 60.0f);
			}

			float amplitude = 1.0f * fade; 
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));

			Vector3 finalDirection = NormalizeOr(Add(homingDirection, wave), homingDirection);
			float homingSpeed = (std::max)(0.01f, tuning_.speed * 1.5f);
			velocity_ = Scale(finalDirection, homingSpeed);
		} else {
			Vector3 currentDirection = NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f });
			Vector3 rightVec = NormalizeOr(MyMath::Cross(currentDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) rightVec = { 1.0f, 0.0f, 0.0f };
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, currentDirection), { 0.0f, 1.0f, 0.0f });

			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			float amplitude = 0.5f;
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));
			
			Vector3 finalDirection = NormalizeOr(Add(currentDirection, wave), currentDirection);
			velocity_ = Scale(finalDirection, (std::max)(0.01f, tuning_.speed * 1.5f));
		}
	}
	position_.x += velocity_.x;
	position_.y += velocity_.y;
	position_.z += velocity_.z;
	elapsedFrames_++;

	lifeTimer_--;
	if (lifeTimer_ <= 0) {
		isDead_ = true;
	}

	UpdateModel();

	if (kEnableMissileTrails && type_ == MissileType::MissileWithTrail && trail_ && trailObject_) {
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
	if (kEnableMissileTrails && type_ == MissileType::MissileWithTrail && trailObject_) {
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

	if (kEnableMissileTrails && type_ == MissileType::MissileWithTrail && trailObject_) {
		trailObject_->Draw();
	}
}

void Missile::OnCollision() {
	isDead_ = true;
}
