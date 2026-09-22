#pragma once
#include <Windows.h>
#include<wrl.h>
#include <array>
#include <cstdint>
#include <string>

#include <cassert>
#define DIRECTINPUT_VERSION    0x0800//DirectInputのバージョン指定
#include <dinput.h>
#include <Xinput.h>
#include"WinApp.h"



#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib, "xinput9_1_0.lib")

// プレイヤー操作はキーボードとコントローラーで同じアクションへ集約する。
// 画面上の設定変更も、ここで定義したバインドを直接更新する。
enum class PlayerAction : uint8_t {
	MoveForward,
	MoveBackward,
	MoveUp,
	MoveDown,
	TurnLeft,
	TurnRight,
	PitchUp,
	PitchDown,
	RollLeft,
	RollRight,
	Guard,
	TransformFighter,
	TransformGerwalk,
	TransformBattroid,
	DodgeLeft,
	DodgeRight,
	Melee,
	NormalFire,
	HomingFire,
	LockToggle,
	LockRelease,
	ReloadNormal,
	ReloadHoming,
	SpecialAttack,
	Song,
	Count,
};

enum class ControllerInput : uint8_t {
	None,
	A, B, X, Y,
	LeftBumper, RightBumper,
	Back, Start,
	LeftThumb, RightThumb,
	DPadUp, DPadDown, DPadLeft, DPadRight,
	LeftTrigger, RightTrigger,
	LeftStickUp, LeftStickDown, LeftStickLeft, LeftStickRight,
	RightStickUp, RightStickDown, RightStickLeft, RightStickRight,
	Count,
};

struct PlayerActionBinding {
	BYTE keyboardKey = 0;
	ControllerInput controllerInput = ControllerInput::None;
};

class Input {
public:

	//namespace省略
	template<class T>using ComPtr = Microsoft::WRL::ComPtr<T>;

	// シングルトンインスタンスの取得
	static Input *GetInstance();


	//初期化
	void Initialize(WinApp* winApp);

	//更新
	void Update();

	//キー押下をチェック
	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

	// プレイヤー用のアクション入力。キーボードとXInputコントローラーのどちらでも反応する。
	bool PushAction(PlayerAction action) const;
	bool TriggerAction(PlayerAction action) const;
	const PlayerActionBinding& GetActionBinding(PlayerAction action) const;
	void SetKeyboardBinding(PlayerAction action, BYTE key);
	void SetControllerBinding(PlayerAction action, ControllerInput input);
	void ResetPlayerActionBindings();
	void SavePlayerActionBindings() const;
	bool IsControllerConnected() const { return isControllerConnected_; }

	static constexpr size_t GetPlayerActionCount() { return static_cast<size_t>(PlayerAction::Count); }
	static constexpr size_t GetControllerInputCount() { return static_cast<size_t>(ControllerInput::Count); }
	static const char* GetPlayerActionName(PlayerAction action);
	static const char* GetControllerInputName(ControllerInput input);
	static std::string GetKeyboardKeyName(BYTE key);

	// ===========================
	// マウス入力 ゲッター
	// ===========================
	// マウスボタン押しっぱなし (0=左, 1=右, 2=中)
	bool PushMouseButton(int button) const;
	// マウスボタンを押した瞬間
	bool TriggerMouseButton(int button) const;
	// マウスの移動量 (フレームごとの差分: X/Y)
	long GetMouseDeltaX() const { return mouseState_.lX; }
	long GetMouseDeltaY() const { return mouseState_.lY; }
	// マウスホイールの回転量（上=正, 下=負）
	long GetMouseWheel() const { return mouseState_.lZ; }
	void SetMouseCursorClipEnabled(bool enabled);
	void SetMouseCursorClipRect(float minX, float minY, float maxX, float maxY);
	void ClearMouseCursorClipRect();



private:

	// コンストラクタを隠蔽・コピー禁止にする
	Input() = default;
	~Input() = default;
	Input(const Input &) = delete;
	Input &operator=(const Input &) = delete;

	//キーボードのデバイス
	ComPtr < IDirectInputDevice8> keyboard;

	//DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput;

	//全キーの状態
	BYTE key[256] = {};

	//前回の全キーの状態
	BYTE keyPre[256] = {};

	std::array<PlayerActionBinding, static_cast<size_t>(PlayerAction::Count)> playerActionBindings_{};
	XINPUT_STATE controllerState_{};
	XINPUT_STATE controllerStatePre_{};
	bool isControllerConnected_ = false;

	bool IsControllerInputPressed(ControllerInput input, bool previousState) const;
	void LoadPlayerActionBindings();

	//WindowsAPI
	WinApp *winApp_ = nullptr;

	// ===========================
	// マウス入力 メンバー
	// ===========================
	ComPtr<IDirectInputDevice8> mouse_;
	DIMOUSESTATE mouseState_ = {};
	DIMOUSESTATE mouseStatePre_ = {};
	bool isMouseCursorClipEnabled_ = false;
	bool isMouseCursorClipped_ = false;
	bool hasMouseCursorClipRect_ = false;
	RECT mouseCursorClipRect_ = {};

	void ApplyMouseCursorClip();
	bool GetClientMouseCursorClipRect(RECT& rect) const;
};


