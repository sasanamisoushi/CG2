#include "MyMath.h"
#include <corecrt_math.h>
#include <cmath>
#include <algorithm>

//平行移動
Matrix4x4 MyMath::MakeTranslateMatrix(const Vector3 &translate) {
	Matrix4x4 result = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		translate.x, translate.y, translate.z, 1
	};
	return result;
}

//拡大縮小
Matrix4x4 MyMath::MkeScaleMatrix(const Vector3 &scale) {
	Matrix4x4 result = {
		scale.x, 0, 0, 0,
		0, scale.y, 0, 0,
		0, 0, scale.z, 0,
		0, 0, 0, 1 };
	return result;
}


//x軸回転行列
Matrix4x4 MyMath::MakeRoteXMatrix(float radian) {
	Matrix4x4 result = {
		1,0,0,0,
		0,cosf(radian),sinf(radian),0,
		0,-sinf(radian),cosf(radian),0,
		0,0,0,1,
	};
	return result;
}

//Y軸回転行列
Matrix4x4 MyMath::MakeRotateYMatrix(float radian) {
	Matrix4x4 result = {
		cosf(radian),0,-sinf(radian),0,
		0,1,0,0,
		sinf(radian),0,cosf(radian),0,
		0,0,0,1,
	};
	return result;
}

//Z軸回転行列
Matrix4x4 MyMath::MakeRotateZMatrix(float radian) {
	Matrix4x4 result = {
		cosf(radian),sinf(radian),0,0,
		-sinf(radian),cosf(radian),0,0,
		0,0,1,0,
		0,0,0,1,
	};
	return result;
}

//積
Matrix4x4 MyMath::Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2) {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

//アフィン変換
Matrix4x4 MyMath::MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) {
	Matrix4x4 rot = Multiply(Multiply(MakeRoteXMatrix(rotate.x), MakeRotateYMatrix(rotate.y)), MakeRotateZMatrix(rotate.z));
	Matrix4x4 result = Multiply(Multiply(MkeScaleMatrix(scale), rot), MakeTranslateMatrix(translate));

	return result;
}

// クォータニオンから回転行列を作成
Matrix4x4 MyMath::MakeRotateMatrix(const Quaternion& q) {
	Matrix4x4 result = MakeIdentity4x4();
	float xx = q.x * q.x;
	float yy = q.y * q.y;
	float zz = q.z * q.z;
	float xy = q.x * q.y;
	float xz = q.x * q.z;
	float yz = q.y * q.z;
	float wx = q.w * q.x;
	float wy = q.w * q.y;
	float wz = q.w * q.z;

	result.m[0][0] = 1.0f - 2.0f * (yy + zz);
	result.m[0][1] = 2.0f * (xy + wz);
	result.m[0][2] = 2.0f * (xz - wy);

	result.m[1][0] = 2.0f * (xy - wz);
	result.m[1][1] = 1.0f - 2.0f * (xx + zz);
	result.m[1][2] = 2.0f * (yz + wx);

	result.m[2][0] = 2.0f * (xz + wy);
	result.m[2][1] = 2.0f * (yz - wx);
	result.m[2][2] = 1.0f - 2.0f * (xx + yy);

	return result;
}

// クォータニオンによるアフィン変換行列
Matrix4x4 MyMath::MakeAffineMatrix(const Vector3 &scale, const Quaternion &rotate, const Vector3 &translate) {
	Matrix4x4 rot = MakeRotateMatrix(rotate);
	Matrix4x4 result = Multiply(Multiply(MkeScaleMatrix(scale), rot), MakeTranslateMatrix(translate));

	return result;
}



//座標変換
Vector3 MyMath::Transform(const Vector3 &vector, const Matrix4x4 &matrix) {
	float x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
	float y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
	float z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];

	if (w != 0.0f) {
		x /= w;
		y /= w;
		z /= w;
	}

	return { x, y, z };
}

//遠近投影行列
Matrix4x4 MyMath::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ) {
	Matrix4x4 result = {};

	float f = 1.0f / tanf(fovY / 2.0f);

	result.m[0][0] = f / aspectRatio;
	result.m[1][1] = f;
	result.m[2][2] = farZ / (farZ - nearZ);
	result.m[2][3] = 1.0f;
	result.m[3][2] = -nearZ * farZ / (farZ - nearZ);
	result.m[3][3] = 0.0f;

	return result;
}

//正規化デバイス座標
Matrix4x4 MyMath::MakeViewportMatrix(float width, float height) {
	Matrix4x4 result = {};

	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f; // Y軸は上下逆
	result.m[2][2] = 1.0f;
	result.m[3][0] = width / 2.0f;
	result.m[3][1] = height / 2.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

//単位行列
Matrix4x4 MyMath::MakeIdentity4x4() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) {
		result.m[i][i] = 1.0f;
	}
	return result;
}


//逆行列
Matrix4x4 MyMath::Inverse(const Matrix4x4 &m) {
	Matrix4x4 result = {};
	float determinant =
		m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2] -
		m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2] -
		m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2] +
		m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2] +
		m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2] -
		m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2] -
		m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0] +
		m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];


	if (determinant == 0.0f) {
		return result;
	}

	float InDit = 1.0f / determinant;

	result.m[0][0] = InDit * (
		m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] -
		m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]);
	result.m[0][1] = InDit * (
		-m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2] +
		m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2]);
	result.m[0][2] = InDit * (
		m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] -
		m.m[0][3] * m.m[1][2] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]);
	result.m[0][3] = InDit * (
		-m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2] +
		m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2]);
	result.m[1][0] = InDit * (
		-m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2] +
		m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2]);
	result.m[1][1] = InDit * (
		m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] -
		m.m[0][3] * m.m[2][2] * m.m[3][0] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]);
	result.m[1][2] = InDit * (
		-m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2] +
		m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2]);
	result.m[1][3] = InDit * (
		m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] -
		m.m[0][3] * m.m[1][2] * m.m[2][0] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]);
	result.m[2][0] = InDit * (
		m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] -
		m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]);
	result.m[2][1] = InDit * (
		-m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1] +
		m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1]);
	result.m[2][2] = InDit * (
		m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] -
		m.m[0][3] * m.m[1][1] * m.m[3][0] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]);
	result.m[2][3] = InDit * (
		-m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1] +
		m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1]);
	result.m[3][0] = InDit * (
		-m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1] +
		m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1]);
	result.m[3][1] = InDit * (
		m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] -
		m.m[0][2] * m.m[2][1] * m.m[3][0] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]);
	result.m[3][2] = InDit * (
		-m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1] +
		m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1]);
	result.m[3][3] = InDit * (
		m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] -
		m.m[0][2] * m.m[1][1] * m.m[2][0] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]);
	return result;
}

Matrix4x4 MyMath::Transpose(const Matrix4x4 &m) {
	Matrix4x4 result{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.m[row][col] = m.m[col][row];
		}
	}
	return result;
}

Matrix4x4 MyMath::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};

	float width = right - left;
	float height = top - bottom;
	float depth = farClip - nearClip;

	result.m[0][0] = 2.0f / width;
	result.m[1][1] = 2.0f / height;
	result.m[2][2] = 1.0f / depth;
	result.m[3][0] = -(right + left) / width;
	result.m[3][1] = -(bottom + top) / height;
	result.m[3][2] = -nearClip / depth;
	result.m[3][3] = 1.0f;

	return result;
}

Vector3 MyMath::Normalize(const Vector3 &v) {
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length == 0.0f) {
		return { 0.0f, 0.0f, 0.0f }; 
	}
	return {
		v.x / length,
		v.y / length,
		v.z / length
	};
}

Quaternion MyMath::Normalize(const Quaternion &q) {
	float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (len != 0.0f) {
		return { q.x / len, q.y / len, q.z / len, q.w / len };
	}
	return q;
}

Quaternion MyMath::Multiply(const Quaternion &lhs, const Quaternion &rhs) {
	Quaternion result;
	result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
	result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
	result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
	result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
	return result;
}

Quaternion MyMath::MakeAxisAngle(const Vector3 &axis, float angle) {
	float halfAngle = angle * 0.5f;
	float s = std::sin(halfAngle);
	return { axis.x * s, axis.y * s, axis.z * s, std::cos(halfAngle) };
}

Vector3 MyMath::RotateVector(const Vector3 &v, const Quaternion &q) {
	Matrix4x4 rotMatrix = MakeRotateMatrix(q);
	return Transform(v, rotMatrix);
}

Quaternion MyMath::Slerp(const Quaternion &q0, const Quaternion &q1, float t) {
	float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;

	Quaternion q1_ = q1;
	if (dot < 0.0f) {
		q1_ = { -q1.x, -q1.y, -q1.z, -q1.w };
		dot = -dot;
	}

	if (dot > 0.9995f) {
		Quaternion result = {
			q0.x + t * (q1_.x - q0.x),
			q0.y + t * (q1_.y - q0.y),
			q0.z + t * (q1_.z - q0.z),
			q0.w + t * (q1_.w - q0.w)
		};
		float mag = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
		return { result.x / mag, result.y / mag, result.z / mag, result.w / mag };
	}

	float theta_0 = std::acos(dot);
	float theta = theta_0 * t;
	float sin_theta = std::sin(theta);
	float sin_theta_0 = std::sin(theta_0);

	float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
	float s1 = sin_theta / sin_theta_0;

	return {
		q0.x * s0 + q1_.x * s1,
		q0.y * s0 + q1_.y * s1,
		q0.z * s0 + q1_.z * s1,
		q0.w * s0 + q1_.w * s1
	};
}

// 3D座標から2Dスクリーン座標への変換
Vector3 MyMath::WorldToScreen(const Vector3 &worldPos, const Matrix4x4 &viewProjectionMatrix, float screenWidth, float screenHeight) {
	float w = worldPos.x * viewProjectionMatrix.m[0][3] + worldPos.y * viewProjectionMatrix.m[1][3] + worldPos.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];

	Vector3 clipPos = {
		worldPos.x * viewProjectionMatrix.m[0][0] + worldPos.y * viewProjectionMatrix.m[1][0] + worldPos.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0],
		worldPos.x * viewProjectionMatrix.m[0][1] + worldPos.y * viewProjectionMatrix.m[1][1] + worldPos.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1],
		worldPos.x * viewProjectionMatrix.m[0][2] + worldPos.y * viewProjectionMatrix.m[1][2] + worldPos.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2]
	};

	if (w != 0.0f) {
		clipPos.x /= w;
		clipPos.y /= w;
		clipPos.z /= w;
	}

	float screenX = (clipPos.x + 1.0f) * (screenWidth / 2.0f);
	float screenY = (1.0f - clipPos.y) * (screenHeight / 2.0f);

	return { screenX, screenY, clipPos.z };
}

float MyMath::Dot(const Vector3 &a, const Vector3 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 MyMath::Cross(const Vector3 &a, const Vector3 &b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

bool MyMath::IsCollision(const OBB &obb1, const OBB &obb2) {
	Vector3 distance = {
		obb2.center.x - obb1.center.x,
		obb2.center.y - obb1.center.y,
		obb2.center.z - obb1.center.z
	};

	Vector3 axes[15];
	int axisCount = 0;

	// 1〜3: obb1のローカル軸
	axes[axisCount++] = obb1.orientations[0];
	axes[axisCount++] = obb1.orientations[1];
	axes[axisCount++] = obb1.orientations[2];

	// 4〜6: obb2のローカル軸
	axes[axisCount++] = obb2.orientations[0];
	axes[axisCount++] = obb2.orientations[1];
	axes[axisCount++] = obb2.orientations[2];

	// 7〜15: obb1の各軸とobb2の各軸の外積（クロス積）
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			Vector3 cross = Cross(obb1.orientations[i], obb2.orientations[j]);
			if (Dot(cross, cross) > 0.00001f) {
				axes[axisCount++] = Normalize(cross);
			}
		}
	}

	// 15本の軸すべてに対して、投影して隙間があるかチェックする
	for (int i = 0; i < axisCount; ++i) {
		const Vector3 &axis = axes[i];

		float rA = obb1.size.x * std::abs(Dot(obb1.orientations[0], axis)) +
			obb1.size.y * std::abs(Dot(obb1.orientations[1], axis)) +
			obb1.size.z * std::abs(Dot(obb1.orientations[2], axis));

		float rB = obb2.size.x * std::abs(Dot(obb2.orientations[0], axis)) +
			obb2.size.y * std::abs(Dot(obb2.orientations[1], axis)) +
			obb2.size.z * std::abs(Dot(obb2.orientations[2], axis));

		float distanceProjected = std::abs(Dot(distance, axis));

		if (distanceProjected > rA + rB) {
			return false;
		}
	}

	return true;
}

bool MyMath::IsCollision(const Sphere &sphere, const OBB &obb) {
	Vector3 localSpherePos = {
		sphere.center.x - obb.center.x,
		sphere.center.y - obb.center.y,
		sphere.center.z - obb.center.z
	};

	Vector3 closestPoint = { 0, 0, 0 };

	for (int i = 0; i < 3; ++i) {
		float dist = Dot(localSpherePos, obb.orientations[i]);
		float axisSize = (i == 0) ? obb.size.x : (i == 1) ? obb.size.y : obb.size.z;
		dist = std::max(-axisSize, std::min(dist, axisSize));

		closestPoint.x += dist * obb.orientations[i].x;
		closestPoint.y += dist * obb.orientations[i].y;
		closestPoint.z += dist * obb.orientations[i].z;
	}

	float dx = localSpherePos.x - closestPoint.x;
	float dy = localSpherePos.y - closestPoint.y;
	float dz = localSpherePos.z - closestPoint.z;
	float distanceSq = dx * dx + dy * dy + dz * dz;

	return distanceSq <= (sphere.radius * sphere.radius);
}

float MyMath::Length(const Vector3& v) {
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vector3 MyMath::Subtract(const Vector3& v1, const Vector3& v2) {
	return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

Vector3 MyMath::Add(const Vector3& v1, const Vector3& v2) {
	return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

Vector3 MyMath::Multiply(float scalar, const Vector3& v) {
	return { scalar * v.x, scalar * v.y, scalar * v.z };
}

Vector3 MyMath::ClosestPointOnTriangle(const Vector3& point, const Triangle& triangle) {
	Vector3 ab = MyMath::Subtract(triangle.p[1], triangle.p[0]);
	Vector3 ac = MyMath::Subtract(triangle.p[2], triangle.p[0]);
	Vector3 ap = MyMath::Subtract(point, triangle.p[0]);

	float d1 = MyMath::Dot(ab, ap);
	float d2 = MyMath::Dot(ac, ap);
	if (d1 <= 0.0f && d2 <= 0.0f) return triangle.p[0];

	Vector3 bp = MyMath::Subtract(point, triangle.p[1]);
	float d3 = MyMath::Dot(ab, bp);
	float d4 = MyMath::Dot(ac, bp);
	if (d3 >= 0.0f && d4 <= d3) return triangle.p[1];

	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		float v = d1 / (d1 - d3);
		return MyMath::Add(triangle.p[0], MyMath::Multiply(v, ab));
	}

	Vector3 cp = MyMath::Subtract(point, triangle.p[2]);
	float d5 = MyMath::Dot(ab, cp);
	float d6 = MyMath::Dot(ac, cp);
	if (d6 >= 0.0f && d5 <= d6) return triangle.p[2];

	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		float w = d2 / (d2 - d6);
		return MyMath::Add(triangle.p[0], MyMath::Multiply(w, ac));
	}

	float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return MyMath::Add(triangle.p[1], MyMath::Multiply(w, MyMath::Subtract(triangle.p[2], triangle.p[1])));
	}

	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;
	return MyMath::Add(triangle.p[0], MyMath::Add(MyMath::Multiply(v, ab), MyMath::Multiply(w, ac)));
}

bool MyMath::IsCollision(const Sphere& sphere, const Triangle& triangle, Vector3& outPushOut) {
	Vector3 closest = MyMath::ClosestPointOnTriangle(sphere.center, triangle);
	Vector3 diff = MyMath::Subtract(sphere.center, closest);
	float distSq = MyMath::Dot(diff, diff);

	if (distSq <= sphere.radius * sphere.radius) {
		if (distSq > 0.00001f) {
			float dist = std::sqrt(distSq);
			Vector3 pushDir = MyMath::Multiply(1.0f / dist, diff);
			outPushOut = MyMath::Multiply(sphere.radius - dist, pushDir);
		} else {
			// 球の中心が三角形の面上に完全にめり込んでいる場合は、法線方向に押し出す
			Vector3 ab = MyMath::Subtract(triangle.p[1], triangle.p[0]);
			Vector3 ac = MyMath::Subtract(triangle.p[2], triangle.p[0]);
			Vector3 normal = MyMath::Normalize(MyMath::Cross(ab, ac));
			outPushOut = MyMath::Multiply(sphere.radius, normal);
		}
		return true;
	}
	outPushOut = { 0, 0, 0 };
	return false;
}

void MyMath::ExtractFrustumPlanes(const Matrix4x4& vpMatrix, Vector4 outPlanes[6]) {
	outPlanes[0] = { vpMatrix.m[0][3] + vpMatrix.m[0][0], vpMatrix.m[1][3] + vpMatrix.m[1][0], vpMatrix.m[2][3] + vpMatrix.m[2][0], vpMatrix.m[3][3] + vpMatrix.m[3][0] };
	outPlanes[1] = { vpMatrix.m[0][3] - vpMatrix.m[0][0], vpMatrix.m[1][3] - vpMatrix.m[1][0], vpMatrix.m[2][3] - vpMatrix.m[2][0], vpMatrix.m[3][3] - vpMatrix.m[3][0] };
	outPlanes[2] = { vpMatrix.m[0][3] + vpMatrix.m[0][1], vpMatrix.m[1][3] + vpMatrix.m[1][1], vpMatrix.m[2][3] + vpMatrix.m[2][1], vpMatrix.m[3][3] + vpMatrix.m[3][1] };
	outPlanes[3] = { vpMatrix.m[0][3] - vpMatrix.m[0][1], vpMatrix.m[1][3] - vpMatrix.m[1][1], vpMatrix.m[2][3] - vpMatrix.m[2][1], vpMatrix.m[3][3] - vpMatrix.m[3][1] };
	outPlanes[4] = { vpMatrix.m[0][2], vpMatrix.m[1][2], vpMatrix.m[2][2], vpMatrix.m[3][2] };
	outPlanes[5] = { vpMatrix.m[0][3] - vpMatrix.m[0][2], vpMatrix.m[1][3] - vpMatrix.m[1][2], vpMatrix.m[2][3] - vpMatrix.m[2][2], vpMatrix.m[3][3] - vpMatrix.m[3][2] };

	for (int i = 0; i < 6; ++i) {
		float length = std::sqrt(outPlanes[i].x * outPlanes[i].x + outPlanes[i].y * outPlanes[i].y + outPlanes[i].z * outPlanes[i].z);
		if (length > 0.0f) {
			outPlanes[i].x /= length;
			outPlanes[i].y /= length;
			outPlanes[i].z /= length;
			outPlanes[i].w /= length;
		}
	}
}

bool MyMath::IsInFrustum(const Sphere& sphere, const Vector4 planes[6]) {
	for (int i = 0; i < 6; ++i) {
		float dist = planes[i].x * sphere.center.x + planes[i].y * sphere.center.y + planes[i].z * sphere.center.z + planes[i].w;
		if (dist < -sphere.radius) {
			return false;
		}
	}
	return true;
}
