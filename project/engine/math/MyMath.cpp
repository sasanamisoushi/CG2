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
			// 行と列を入れ替えて代入する
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
	// ※引数のaxisは Normalize() されている前提です
	return { axis.x * s, axis.y * s, axis.z * s, std::cos(halfAngle) };
}

Vector3 MyMath::RotateVector(const Vector3 &v, const Quaternion &q) {
	// 既に実装されている関数を組み合わせて、超シンプルに実現！
	Matrix4x4 rotMatrix = MakeRotateMatrix(q);
	return Transform(v, rotMatrix);
}

Quaternion MyMath::Slerp(const Quaternion &q0, const Quaternion &q1, float t) {
	float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;

	// 内積がマイナスなら、片方を反転して最短経路を通るようにする
	Quaternion q1_ = q1;
	if (dot < 0.0f) {
		q1_ = { -q1.x, -q1.y, -q1.z, -q1.w };
		dot = -dot;
	}

	// 角度が非常に近い場合は、通常の線形補間（Lerp）で済ませる
	if (dot > 0.9995f) {
		Quaternion result = {
			q0.x + t * (q1_.x - q0.x),
			q0.y + t * (q1_.y - q0.y),
			q0.z + t * (q1_.z - q0.z),
			q0.w + t * (q1_.w - q0.w)
		};
		// 正規化
		float mag = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
		return { result.x / mag, result.y / mag, result.z / mag, result.w / mag };
	}

	// 球面線形補間の計算
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
	// 1. ビュープロジェクション行列を掛けてクリップ座標系へ変換
	float w = worldPos.x * viewProjectionMatrix.m[0][3] + worldPos.y * viewProjectionMatrix.m[1][3] + worldPos.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];

	Vector3 clipPos = {
		worldPos.x * viewProjectionMatrix.m[0][0] + worldPos.y * viewProjectionMatrix.m[1][0] + worldPos.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0],
		worldPos.x * viewProjectionMatrix.m[0][1] + worldPos.y * viewProjectionMatrix.m[1][1] + worldPos.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1],
		worldPos.x * viewProjectionMatrix.m[0][2] + worldPos.y * viewProjectionMatrix.m[1][2] + worldPos.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2]
	};

	// 2. 透視投影分割（Wで割って正規化デバイス座標 NDC へ）
	if (w != 0.0f) {
		clipPos.x /= w;
		clipPos.y /= w;
		clipPos.z /= w;
	}

	// 3. NDC座標（-1.0 ～ 1.0）をスクリーン座標（0 ～ Width/Height）に変換
	// Y軸は画面の下に行くほどプラスになるので反転させる
	float screenX = (clipPos.x + 1.0f) * (screenWidth / 2.0f);
	float screenY = (1.0f - clipPos.y) * (screenHeight / 2.0f);

	// Zには深度値（0～1）をそのまま入れて返す（カメラの後ろにあるかの判定に使える）
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
	// 2つのOBBの中心間の距離ベクトル
	Vector3 distance = {
		obb2.center.x - obb1.center.x,
		obb2.center.y - obb1.center.y,
		obb2.center.z - obb1.center.z
	};

	// テストすべき15本の分離軸を格納する配列
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
			// 外積が0ベクトル（2つの軸がほぼ平行）の場合はエラーになるので弾く
			if (Dot(cross, cross) > 0.00001f) {
				axes[axisCount++] = Normalize(cross);
			}
		}
	}

	// 15本の軸すべてに対して、投影して隙間があるかチェックする
	for (int i = 0; i < axisCount; ++i) {
		const Vector3 &axis = axes[i];

		// 各OBBの「軸に対する投影半径」を計算
		// obb.size.x は幅の半分、size.yは高さの半分...
		float rA = obb1.size.x * std::abs(Dot(obb1.orientations[0], axis)) +
			obb1.size.y * std::abs(Dot(obb1.orientations[1], axis)) +
			obb1.size.z * std::abs(Dot(obb1.orientations[2], axis));

		float rB = obb2.size.x * std::abs(Dot(obb2.orientations[0], axis)) +
			obb2.size.y * std::abs(Dot(obb2.orientations[1], axis)) +
			obb2.size.z * std::abs(Dot(obb2.orientations[2], axis));

		// 中心間距離ベクトルを軸に投影した長さ
		float distanceProjected = std::abs(Dot(distance, axis));

		// 隙間が見つかった！(当たっていない) -> 即座に判定終了
		if (distanceProjected > rA + rB) {
			return false;
		}
	}

	// 15本すべての軸で隙間がなかった -> 衝突している！
	return true;
}

bool MyMath::IsCollision(const Sphere &sphere, const OBB &obb) {
	// 1. 球の中心をOBBのローカル空間（OBBから見た相対座標）に変換する
	Vector3 localSpherePos = {
		sphere.center.x - obb.center.x,
		sphere.center.y - obb.center.y,
		sphere.center.z - obb.center.z
	};

	Vector3 closestPoint = { 0, 0, 0 };

	// 2. OBBの各軸(X, Y, Z)に対して、球の中心の投影位置をクランプ（箱の範囲内に収める）する
	for (int i = 0; i < 3; ++i) {
		// 球の中心をOBBのi番目の軸に投影
		float dist = Dot(localSpherePos, obb.orientations[i]);

		// OBBのサイズの範囲内に制限する
		float axisSize = (i == 0) ? obb.size.x : (i == 1) ? obb.size.y : obb.size.z;
		dist = std::max(-axisSize, std::min(dist, axisSize));

		// 最も近い点を構築
		closestPoint.x += dist * obb.orientations[i].x;
		closestPoint.y += dist * obb.orientations[i].y;
		closestPoint.z += dist * obb.orientations[i].z;
	}

	// 3. 最も近い点と、実際の球の中心との距離の2乗を計算
	float dx = localSpherePos.x - closestPoint.x;
	float dy = localSpherePos.y - closestPoint.y;
	float dz = localSpherePos.z - closestPoint.z;
	float distanceSq = dx * dx + dy * dy + dz * dz;

	// 距離の2乗が、球の半径の2乗より小さければ衝突！
	return distanceSq <= (sphere.radius * sphere.radius);
}

