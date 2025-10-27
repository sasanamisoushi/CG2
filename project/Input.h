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

	//template<typename T>using ComPtr=Microsoft::WRL

	//初期化
	void Initialize(HINSTANCE hInstance,HWND hwnd);

	//更新
	void Update();

};

