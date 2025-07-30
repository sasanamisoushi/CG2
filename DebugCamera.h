#pragma once
#include "Math.h"

class DebugCamera {

	//X,Y,Z軸周りのローカル回転角度
	Vector3 rotation_ = { 0,0,0 };

	//ローカル座標
	Vector3 translation_ = { 0,0,-50 };

	//ビュー行列
	Matrix4x4 MakeViewMatrix(const Vector3 &eye, const Vector3 &target, const Vector3 &up);

public:

	void Initialize();

	void Update();
};

