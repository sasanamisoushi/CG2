#pragma once

#include "3D/Model.h"
#include "3D/primitive.h"
#include "3D/Skybox.h"
#include "3D/Trail.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include <memory>

// 背景・プリミティブ・デバッグ描画など、環境オブジェクトの描画を管理するクラス
class EnvironmentRenderer {
public:
	EnvironmentRenderer();
	~EnvironmentRenderer() = default;

	void Initialize();
	void Update(Camera* camera);
	void Draw();

	// スカイボックス取得
	Skybox* GetSkybox() const { return skybox_.get(); }

	// プリミティブ取得・設定
	void SetShowNormalRing(bool show) { showNormalRing_ = show; }
	void SetShowPartialRing(bool show) { showPartialRing_ = show; }
	void SetShowCylinder(bool show) { showCylinder_ = show; }

	// パーティクルマネージャー
	ParticleManager* GetParticleManager() const { return particleManager_.get(); }
	ParticleEmitter* GetParticleEmitter() const { return particleEmitter_.get(); }

	// 軌跡(Trail)
	Trail* GetMissileTrail() const { return missileTrail_.get(); }

private:
	std::unique_ptr<Skybox> skybox_;

	// パーティクル
	std::unique_ptr<ParticleManager> particleManager_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;

	// プリミティブ（リングやシリンダー等）
	std::unique_ptr<Primitive> myPlane_;
	std::unique_ptr<Primitive> boundaryAlertPlane_;
	std::unique_ptr<Primitive> myShere_;
	std::unique_ptr<Primitive> myBox_;
	std::unique_ptr<Primitive> myRing_;
	std::unique_ptr<Primitive> myPartialRing_;
	std::unique_ptr<Primitive> myCylinder_;

	bool showNormalRing_ = false;
	bool showPartialRing_ = false;
	bool showCylinder_ = false;

	// Trail
	std::unique_ptr<Trail> missileTrail_;
	std::unique_ptr<Object3d> trailObject_;
};
