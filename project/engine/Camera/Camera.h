#pragma once
#include "MyMath.h"
#include <WinApp.h>
#include <memory>

struct EulerTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct QuaternionTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};

class Camera {
public:

	//更新
	virtual void Update();

	//コンストラクタ
	Camera();

	// 仮想デストラクタを追加する（継承の安全のため）
	virtual ~Camera() = default;


	//setter
	void SetRotate(const Vector3 &rotate) { this->transform.rotate=rotate; }
	void SetTranslate(const Vector3 &translate) { this->transform.translate = translate; }
	void SetFovY(const float &horizontal) { this->horizontal_ = horizontal; }
	void SetAspectRatio(const float &aspectRatio) { this->aspectRatio_ = aspectRatio; }
	void SetNearClip(const float &nearClip) { this->nearClipRange_ = nearClip; }
	void SetFarClip(const float &farClip) { this->furClipRange_ = farClip; }
	void SetQuaternion(const Quaternion &q) {
		this->quaternion_ = q;
		this->useQuaternion_ = true; // フラグをONにする
	}
	//getter
	const Matrix4x4 &GetWorldMatrix() const { return worldMatrix; }
	const Matrix4x4 &GetViewMatrix()const { return viewMatrix; }
	const Matrix4x4 &GetProjectionMatrix()const { return projectionMatrix;}
	const Matrix4x4 &GetViewProjectionMatrix()const { return viewProjectionMatrix; }
	const Vector3 &GetRotate()const { return transform.rotate; }
	const Vector3 &GetTranslate()const { return transform.translate; }

private:
	
	EulerTransform transform;

	// クォータニオン用の変数と、切り替えフラグ
	Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; 
	bool useQuaternion_ = false;

	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;

	std::unique_ptr<MyMath> math_ = std::make_unique<MyMath>();

	Matrix4x4 projectionMatrix;
	float horizontal_= 0.45f;  //水平方向視野角
	float aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight); //アスペクト比
	float nearClipRange_ = 0.1f; //ニアクリップ距離
	float furClipRange_ = 100.0f; //ファークリップ距離

	//合成行列
	Matrix4x4 viewProjectionMatrix;
};
