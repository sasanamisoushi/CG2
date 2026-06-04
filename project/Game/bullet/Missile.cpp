#include "Missile.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"

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

	lifeTimer_ = kLifeTime;
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
		Vector3 targetPos = enemy->GetPosition();

		// 敵への方向ベクトルを計算（ターゲット座標 － 今の座標）
		Vector3 toTarget = {
			targetPos.x - position_.x,
			targetPos.y - position_.y,
			targetPos.z - position_.z
		};

		// ベクトルを正規化（長さを1にして「純粋な方向」だけのデータにする）
		toTarget = MyMath::Normalize(toTarget);

		// 今の速度ベクトルに、敵への方向を少しだけ足す（引っ張る）
		float turnRate = 0.05f; // 旋回力（大きいほど急カーブする）
		velocity_.x += toTarget.x * turnRate;
		velocity_.y += toTarget.y * turnRate;
		velocity_.z += toTarget.z * turnRate;

		// 速度を足したままだと加速し続けてしまうので、長さを1に戻してスピードを掛け直す
		velocity_ = MyMath::Normalize(velocity_);
		float speed = 1.0f; // B弾のスピード
		velocity_.x *= speed;
		velocity_.y *= speed;
		velocity_.z *= speed;
	}

	// 基本の移動処理
	position_.x += velocity_.x;
	position_.y += velocity_.y;
	position_.z += velocity_.z;

	// 螺旋軌道（ぐねぐね）の処理
	if (type_ == MissileType::MissileWithTrail) {
		float time = 120.0f - lifeTimer_;
		position_.x += std::cos(time * 0.3f) * 0.2f;
		position_.y += std::sin(time * 0.3f) * 0.2f;
	}

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
