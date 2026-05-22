#include "Camera.h"



void Camera::Update() {
	int32_t clientWidth = WinApp::GetClientWidth();
	int32_t clientHeight = WinApp::GetClientHeight();
	if (clientWidth > 0 && clientHeight > 0) {
		aspectRatio_ = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
	}

	// フラグによってアフィン変換の計算を分ける！
	if (useQuaternion_) {
		// クォータニオンモードがONなら、クォータニオン版のアフィン変換を使う
		worldMatrix = math_->MakeAffineMatrix(transform.scale, quaternion_, transform.translate);
	} else {
		// そうでなければ、今まで通りのオイラー角（Vector3）のアフィン変換を使う
		worldMatrix = math_->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	}
	
	//ビュー行列
	viewMatrix = math_->Inverse(worldMatrix);

	//プロジェクション行列
	projectionMatrix = math_->MakePerspectiveFovMatrix(horizontal_, aspectRatio_, nearClipRange_, furClipRange_);

	viewProjectionMatrix = math_->Multiply(viewMatrix, projectionMatrix);
}

Camera::Camera()
	:transform({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, horizontal_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClipRange_(0.1f)
	, furClipRange_(100.0f) {

	Update();
}
