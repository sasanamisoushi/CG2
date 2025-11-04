#pragma once
#include <Windows.h>

class WinApp {
public: //静的メンバ関数

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:

	//初期化
	void Initialize();

	//更新
	void Update();

	//getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHinstance() const { return wc.hInstance; }

public: //定数
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClinentHeight = 720;

private:

	//ウィンドウハンドル
	HWND hwnd = nullptr;

	//ウインドウクラスの設定
	WNDCLASS wc{};

};

