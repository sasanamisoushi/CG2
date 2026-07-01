#include "EnvironmentRenderer.h"
#include "engine/Graphics/DirectXCommon.h"
#include "3D/Object3dCommon.h"

EnvironmentRenderer::EnvironmentRenderer() {
}

void EnvironmentRenderer::Initialize() {
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/SkyBox.dds");

	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Initialize(DirectXCommon::GetInstance());

	particleEmitter_ = std::make_unique<ParticleEmitter>("emitter1", Vector3{0.0f, 0.0f, 0.0f}, particleManager_.get());

	missileTrail_ = std::make_unique<Trail>();
	missileTrail_->Initialize(60);
	
	trailObject_ = std::make_unique<Object3d>();
	trailObject_->Initialize(Object3dCommon::GetInstance());
}

void EnvironmentRenderer::Update(Camera* camera) {
	particleManager_->Update(camera);
	particleEmitter_->Update();
}

void EnvironmentRenderer::Draw() {
	if (skybox_) {
		skybox_->Draw();
	}
	
	if (missileTrail_ && trailObject_) {
		// trailObject draws the trail
	}

	if (particleManager_) {
		particleManager_->Draw();
	}
}
