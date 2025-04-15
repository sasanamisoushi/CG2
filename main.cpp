#include<Windows.h>

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//出力ウィンドへの文字 
	OutputDebugStringA("Hello,DirectX!\n");
	return 0;
}