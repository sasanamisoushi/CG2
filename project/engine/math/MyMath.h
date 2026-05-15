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

struct Quaternion {
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

// 当たり判定用の球体
struct Sphere {
	Vector3 center; // 中心座標
	float radius;   // 半径
};

// 当たり判定用のOBB（有向境界ボックス）
struct OBB {
	Vector3 center;          // 中心座標
	Vector3 orientations[3]; // ローカルのX, Y, Z軸（機体の右・上・前など。正規化済み必須）
	Vector3 size;            // 中心から各面の距離（幅の半分, 高さの半分, 奥行きの半分）
};

class MyMath {
public:

	//平行移動
	static Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);

	//拡大縮小
	static Matrix4x4 MkeScaleMatrix(const Vector3 &scale);

	//X軸回転行列
	static Matrix4x4 MakeRoteXMatrix(float radian);

	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);

	//Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// 積
	static Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

	// アフィン変換行列
	static Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate);

	// クォータニオンから回転行列を作成
	static Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

	// クォータニオンによるアフィン変換行列
	static Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Quaternion &rotate, const Vector3 &translate);

	//座標変換
	static Vector3 Transform(const Vector3 &vector, const Matrix4x4 &matrix);

	//遠近投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ);

	//正規化デバイス座標
	static Matrix4x4 MakeViewportMatrix(float width, float height);

	//単位行列
	static Matrix4x4 MakeIdentity4x4();

	//逆行列
	static Matrix4x4 Inverse(const Matrix4x4 &m);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4 &m);

	//平行投影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	static Vector3 Normalize(const Vector3 &v);

	// クォータニオン用の正規化関数
	static Quaternion Normalize(const Quaternion &q);

	// クォータニオンの積（回転の合成）
	static Quaternion Multiply(const Quaternion &lhs, const Quaternion &rhs);

	// 任意の軸（Axis）と角度（Angle）からクォータニオンを作る
	static Quaternion MakeAxisAngle(const Vector3 &axis, float angle);

	// クォータニオンを使ってベクトル（座標）を回転させる
	static Vector3 RotateVector(const Vector3 &v, const Quaternion &q);

	// クォータニオンの球面線形補間（滑らかな回転）
	static Quaternion Slerp(const Quaternion &q0, const Quaternion &q1, float t);

	// 3Dワールド座標から2Dスクリーン座標への変換（ロックオンUI等に使用）
	static Vector3 WorldToScreen(const Vector3 &worldPos, const Matrix4x4 &viewProjectionMatrix, float screenWidth, float screenHeight);

	// ベクトルの内積（Dot）と外積（Cross）
	static float Dot(const Vector3 &a, const Vector3 &b);
	static Vector3 Cross(const Vector3 &a, const Vector3 &b);

	// 当たり判定関数
	// 1. OBB同士の判定（分離軸定理）
	static bool IsCollision(const OBB &obb1, const OBB &obb2);

	// 2. 球とOBBの判定（ミサイル vs 機体 などで大活躍します！）
	static bool IsCollision(const Sphere &sphere, const OBB &obb);
};

