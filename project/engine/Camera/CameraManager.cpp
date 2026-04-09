#include "CameraManager.h"
#include <cassert>

void CameraManager::Initialize() {
	activeIndex_ = 0;
}

void CameraManager::Update() {
	if (cameras_.empty()) {
		return;
	}
	cameras_[activeIndex_]->Update();
}


void CameraManager::AddCamera(std::shared_ptr<Camera> camera) {
	cameras_.push_back(camera);
}

void CameraManager::SetActiveCamera(size_t index) {
	assert(index < cameras_.size());
	activeIndex_ = index;
}
