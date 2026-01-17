#include "Camera.h"



void Camera::Update() {

	worldMatrix= math_->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	
	//ビュー行列
	viewMatrix = math_->Inverse(worldMatrix);

	//プロジェクション行列
	projectionMatrix = math_->MakePerspectiveFovMatrix(horizontal_, aspectRatio_, nearClipRange_, furClipRange_);

	viewProjectionMatrix = math_->Multiply(viewMatrix, projectionMatrix);
}

Camera::Camera()
	:transform({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, horizontal_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClinentHeight))
	, nearClipRange_(0.1f)
	, furClipRange_(100.0f)
	, worldMatrix(math_->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate))
	, projectionMatrix(math_->Inverse(worldMatrix))
	, viewProjectionMatrix(math_->Multiply(viewMatrix, projectionMatrix)) {
}
