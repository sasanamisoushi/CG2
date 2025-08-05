#include "DebugCamera.h"
#include <corecrt_math.h>

void DebugCamera::Initialize() {
}

void DebugCamera::Update() {

	//入力によるカメラの移動や回転

	//前進
	if (ForwardMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { 0,0,speed };

		//移動ベクトルを角度分だけ回転させる
		move = {
			sinf(rotation_.y) * speed,
			0.0f,
			cosf(rotation_.y) * speed
		};

		//移動ベクトル分だけ座標を加算する
		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	//後退
	if (BackwardMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { 0,0,speed };

		//移動ベクトルを角度分だけ回転させる
		move = {
			sinf(rotation_.y) * speed,
			0.0f,
			cosf(rotation_.y) * speed
		};

		//移動ベクトル分だけ座標を加算する
		translation_.x -= move.x;
		translation_.y -= move.y;
		translation_.z -= move.z;
	}

	//上
	if (UpMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { 0,speed,0 };

		//移動ベクトルを角度分だけ回転させる
		/*move = {
			sinf(rotation_.y) * speed,
			0.0f,
			cosf(rotation_.y) * speed
		};*/

		//移動ベクトル分だけ座標を加算する
		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	//下
	if (DownMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { 0,speed,0 };

		//移動ベクトルを角度分だけ回転させる
		/*move = {
			sinf(rotation_.y) * speed,
			0.0f,
			cosf(rotation_.y) * speed
		};*/

		//移動ベクトル分だけ座標を加算する
		translation_.x -= move.x;
		translation_.y -= move.y;
		translation_.z -= move.z;
	}

	//左
	if (LeftMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { speed ,0,0 };

		//移動ベクトルを角度分だけ回転させる
		move = {
			cosf(rotation_.y) * speed,
			0.0f,
			-sinf(rotation_.y) * speed
		};

		//移動ベクトル分だけ座標を加算する
		translation_.x += -move.x;
		translation_.y += -move.y;
		translation_.z += -move.z;
	}

	//右
	if (RightMove) {
		const float speed = 2.0f;

		//カメラ移動ベクトル
		Vector3 move = { speed,0,0 };



		//移動ベクトルを角度分だけ回転させる
		move = {
			cosf(rotation_.y) * speed,
			0.0f,
			-sinf(rotation_.y) * speed
		};

		//移動ベクトル分だけ座標を加算する
		translation_.x += move.x;
		translation_.y += move.y;
		translation_.z += move.z;
	}

	// マウス座標の取得と保存（静的変数で前フレームとの比較用）
	static int lastMouseX = 0;
	static int lastMouseY = 0;

	int mouseX, mouseY;
	math_.GetMousePosition(&mouseX, &mouseY); // ← 使用している入力ライブラリに応じて実装してください

	int dx = mouseX - lastMouseX;
	int dy = mouseY - lastMouseY;

	// 回転感度（調整可）
	const float rotationSpeed = 0.01f;

	// マウス右ボタンが押されている場合に回転
	if (math_.IsMouseRightButtonDown()) {
		// Y軸回り（横回転）← マウスの左右移動
		rotation_.y += dx * rotationSpeed;

		// X軸回り（縦回転）← マウスの上下移動
		rotation_.x += dy * rotationSpeed;

		// X軸回転の制限（真上/真下を向きすぎないように）
		if (rotation_.x > 1.57f) rotation_.x = 1.57f;
		if (rotation_.x < -1.57f) rotation_.x = -1.57f;
	}

	// マウス座標の更新
	lastMouseX = mouseX;
	lastMouseY = mouseY;

	//ビュー行列の更新
	//角度から回転行列を計算する
	Matrix4x4 rotX = math_.MakeRoteXMatrix(rotation_.x);
	Matrix4x4 rotY = math_.MakeRotateYMatrix(rotation_.y);
	Matrix4x4 rotZ = math_.MakeRotateZMatrix(rotation_.z);
	Matrix4x4 rotationMatrix = math_.Multiply(rotZ, math_.Multiply(rotX, rotY));


	//座標から平行移動行列を計算する
	Matrix4x4 translationMatrix = math_.MakeTranslateMatrix(translation_);

	//回転行列と平行移動行列からワールド行列を計算する
	Matrix4x4 worldMatrix = math_.Multiply(rotationMatrix, translationMatrix);

	//ワールド行列の逆行列をビュー行列に代入する


}
