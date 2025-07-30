#pragma once


struct Vector2 {
	float x;
	float y;
};

struct Vector3 {
	float x;
	float y;
	float z;
};

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct Matrix4x4 {
	float m[4][4];
};

struct Matrix3x3 {
	float m[3][3];
};

Vector3 operator-(const Vector3 &v1, const Vector3 &v2);

class Math {
public:

	//平行移動
	Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);

	//拡大縮小
	Matrix4x4 MkeScaleMatrix(const Vector3 &scale);

	//X軸回転行列
	Matrix4x4 MakeRoteXMatrix(float radian);

	// Y軸回転行列
	Matrix4x4 MakeRotateYMatrix(float radian);

	//Z軸回転行列
	Matrix4x4 MakeRotateZMatrix(float radian);

	// 積
	Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

	// アフィン変換行列
	Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate);

	//座標変換
	Vector3 Transform(const Vector3 &vector, const Matrix4x4 &matrix);

	//遠近投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ);

	//正規化デバイス座標
	Matrix4x4 MakeViewportMatrix(float width, float height);

	//単位行列
	Matrix4x4 MakeIdentity4x4();

	//逆行列
	Matrix4x4 Inverse(const Matrix4x4 &m);

	//平行投影行列
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	Vector3 Normalize(const Vector3 &v);

	

	Vector3 Cross(const Vector3 &v1, const Vector3 &v2);

	float Dot(const Vector3 &v1, const Vector3 &v2);

	Matrix4x4 MakeViewMatrix(const Vector3 &eye, const Vector3 &target, const Vector3 &up);

};

