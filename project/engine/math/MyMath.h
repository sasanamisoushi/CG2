#pragma once


struct Vector2 {
	float x;
	float y;

	// 加算（+=）
	Vector2 &operator+=(const Vector2 &rhs) {
		this->x += rhs.x;
		this->y += rhs.y;
		return *this;
	}

	// 加算（+）
	Vector2 operator+(const Vector2 &rhs) const {
		return Vector2{ this->x + rhs.x, this->y + rhs.y };
	}
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

	Matrix4x4 operator*(const Matrix4x4 &other) const {
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] =
					m[row][0] * other.m[0][col] +
					m[row][1] * other.m[1][col] +
					m[row][2] * other.m[2][col] +
					m[row][3] * other.m[3][col];
			}
		}
		return result;
	}
};

struct Matrix3x3 {
	float m[3][3];
};



class MyMath {
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
};

