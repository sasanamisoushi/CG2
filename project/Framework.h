#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"

class Framework {
public:

	//初期化
	virtual void Initialize();

	//終了
	virtual void Finalize();

	//更新
	virtual void Update();

	//描画
	virtual void Draw() = 0;

	//終了チェック
	virtual bool IsEndRequest() { return endRequest_; }

	//実行
	void Run();

	virtual ~Framework() = default;

protected:
	bool endRequest_ = false;

	//基盤系
	std::unique_ptr<WinApp> winApp;
	std::unique_ptr<DirectXCommon> dxCommon;
	std::unique_ptr<ImGuiManager> imGuiManager;
	std::unique_ptr<Input> input;

	//描画共通系
	std::unique_ptr<SpriteCommon> spriteCommon;
	std::unique_ptr<Object3dCommon> object3dCommon;

};

