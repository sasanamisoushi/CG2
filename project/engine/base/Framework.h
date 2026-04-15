#pragma once
#include "WinApp.h"
#include "engine/Graphics/DirectXCommon.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Input/Input.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"

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
	std::unique_ptr<ImGuiManager> imGuiManager;
};

