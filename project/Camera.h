#pragma once
#include "MyMath.h"
#include <WinApp.h>

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

class Camera {
public:

	//更新
	void Update();

	//コンストラクタ
	Camera();


	//setter
	void SetRotate(const Vector3 &rotate) { this->transform.rotate=rotate; }
	void SetTranslate(const Vector3 &translate) { this->transform.translate = translate; }
	void SetFovY(const float &horizontal) { this->horizontal_ = horizontal; }
	void SetAspectRatio(const float &aspectRatio) { this->aspectRatio_ = aspectRatio; }
	void SetNearClip(const float &nearClip) { this->nearClipRange_ = nearClip; }
	void SetFarClip(const float &farClip) { this->furClipRange_ = farClip; }

	//getter
	const Matrix4x4 &GetWorldMatrix() const { return worldMatrix; }
	const Matrix4x4 &GetViewMatrix()const { return viewMatrix; }
	const Matrix4x4 &GetProjectionMatrix()const { return projectionMatrix;}
	const Matrix4x4 &GetViewProjectionMatrix()const { return viewProjectionMatrix; }
	const Vector3 &GetRotate()const { return transform.rotate; }
	const Vector3 &GetTranslate()const { return transform.translate; }

private:
	
	Transform transform;
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;

	MyMath *math_=nullptr;

	Matrix4x4 projectionMatrix;
	float horizontal_= 0.45f;  //水平方向視野角
	float aspectRatio_ = float(WinApp::kClientWidth / WinApp::kClinentHeight); //アスペクト比
	float nearClipRange_ = 0.1f; //ニアクリップ距離
	float furClipRange_ = 100.0f; //ファークリップ距離

	//合成行列
	Matrix4x4 viewProjectionMatrix;
};