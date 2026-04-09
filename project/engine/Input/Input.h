#pragma once
#include <Windows.h>
#include<wrl.h>

#include <cassert>
#define DIRECTINPUT_VERSION    0x0800//DirectInputのバージョン指定
#include <dinput.h>
#include"WinApp.h"



#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

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

	//WindowsAPI
	WinApp *winApp_ = nullptr;

};

