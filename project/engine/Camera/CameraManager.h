#pragma once
#include <vector>
#include <memory>
#include "Camera.h"

class CameraManager {
public:

	//初期化
	void Initialize();

	//更新 (アクティブなカメラのみ)
	void Update();

	//カメラ登録
	void AddCamera(std::shared_ptr<Camera> camera);

	//アクティブカメラ切り替え
	void SetActiveCamera(size_t index);

	//getter
	Camera *GetActiveCamera() const {
		if (cameras_.empty()) return nullptr;
		return cameras_[activeIndex_].get();
	}
	const Matrix4x4 &GetViewMatrix() const { return cameras_[activeIndex_]->GetViewMatrix(); }
	const Matrix4x4 &GetProjectionMatrix() const {return cameras_[activeIndex_]->GetProjectionMatrix();}
	const Matrix4x4 &GetViewProjectionMatrix() const {return cameras_[activeIndex_]->GetViewProjectionMatrix();}

private:
	std::vector<std::shared_ptr<Camera>> cameras_;

	size_t activeIndex_ = 0;

};

