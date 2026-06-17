#include "Input.h"


Input *Input::GetInstance() {
	static Input instance;
	return &instance;
}

void Input::Initialize(WinApp* winApp) {

	//WinAppのインスタンスを記録
	winApp_ = winApp;

	HRESULT result;



	//DirectInputの初期化
	result = DirectInput8Create(
		winApp->GetHinstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void **)&directInput, nullptr);
	assert(SUCCEEDED(result));

	//キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	//入力データの形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);//標準形式
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// ===========================
	// マウスデバイスの初期化
	// ===========================
	result = directInput->CreateDevice(GUID_SysMouse, &mouse_, NULL);
	if (SUCCEEDED(result)) {
		result = mouse_->SetDataFormat(&c_dfDIMouse);        // 標準マウス形式
		if (SUCCEEDED(result)) {
			// DISCL_NONEXCLUSIVE: 他のアプリとマウスを共有する（ImGuiと共存できる）
			result = mouse_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
	}
}

void Input::Update() {

	//前回のキー入力を保持
	memcpy(keyPre, key, sizeof(key));

	//キーボード情報の取得開始
	keyboard->Acquire();
	//全キーの入力情報を取得する
	keyboard->GetDeviceState(sizeof(key), key);

	// ===========================
	// マウス入力の更新
	// ===========================
	if (mouse_) {
		// 前フレームの状態を保存
		mouseStatePre_ = mouseState_;

		// マウス情報の取得
		mouse_->Acquire();
		HRESULT hr = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);
		if (FAILED(hr)) {
			// 取得失敗（フォーカス喪失など）は差分をゼロにする
			mouseState_.lX = 0;
			mouseState_.lY = 0;
			mouseState_.lZ = 0;
		}
	}

}

bool Input::PushKey(BYTE keyNumber) {

	//指定キーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}

	return false;
}

bool Input::TriggerKey(BYTE keyNumber) {

	//前回のキー入力を保持
	if (!keyPre[keyNumber] && key[keyNumber]) {
		return true;
	}


	return false;
}

bool Input::PushMouseButton(int button) const {
	if (button < 0 || button > 3) return false;
	return (mouseState_.rgbButtons[button] & 0x80) != 0;
}

bool Input::TriggerMouseButton(int button) const {
	if (button < 0 || button > 3) return false;
	return ((mouseState_.rgbButtons[button] & 0x80) != 0) &&
	       ((mouseStatePre_.rgbButtons[button] & 0x80) == 0);
}
