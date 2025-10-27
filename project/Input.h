#pragma once
#include <Windows.h>
#include<wrl.h>

#include <cassert>
#define DIRECTINPUT_VERSION    0x0800//DirectInputのバージョン指定
#include <dinput.h>



#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

class Input {
public:

	//namespace省略
	template<class T>using ComPtr = Microsoft::WRL::ComPtr<T>;

	//初期化
	void Initialize(HINSTANCE hInstance,HWND hwnd);

	//更新
	void Update();

	//キー押下をチェック
	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

private:

	//キーボードのデバイス
	ComPtr < IDirectInputDevice8> keyboard;

	//DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput;

	//全キーの状態
	BYTE key[256] = {};

	//前回の全キーの状態
	BYTE keyPre[256] = {};

};

