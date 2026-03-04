#include "D3DResourceLeakChecker.h"
#include "Game.h"

#pragma comment(lib,"Dbghelp.lib")

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	D3DResourceLeakChecker leakCheck;

	// ゲーム本体の生成
	std::unique_ptr<Game> game = std::make_unique<Game>();

	//初期化
	game->Run();

	return 0;
}