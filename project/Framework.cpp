#include "Framework.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "AudioManager.h"

void Framework::Initialize() {

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif 

	//誰も捕捉しなかった場合に補足する関数の登録
	SetUnhandledExceptionFilter(WinApp::ExportDump);

	//---------基盤システムの初期化---------
	// windowsAppの初期化
	winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	// DirectXCommon の生成と初期化
	dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());
	dxCommon->GetCommandList();

	// SRVマネージャの初期化
	SrvManager::GetInstance()->Initialize(dxCommon.get());

	// ImGuiManagerの生成と初期化
	imGuiManager = std::make_unique<ImGuiManager>();
	HWND hwnd = winApp->GetHwnd();
	ID3D12Device *device = dxCommon->GetDevice();
	int numFramesInFlight = 2; //バックバッファの数
	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	ID3D12DescriptorHeap *srvHeap = SrvManager::GetInstance()->GetDescriptorHeap();

	imGuiManager->Initialize(hwnd, device, numFramesInFlight, rtvFormat, srvHeap);

	// テクスチャマネージャの初期化
	TextureManager::GetInstance()->Initialize(dxCommon.get(), SrvManager::GetInstance());

	// 3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon.get());

	// オーディオマネージャーの初期化
	AudioManager::GetInstance()->Initialize();

	// 入力デバイス
	input = std::make_unique<Input>();
	input->Initialize(winApp.get());

	//---------共通部の初期化---------
	//スプライト共通部の初期化
	spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dxCommon.get());

	//3Dオブジェクト共通部の初期化
	object3dCommon = std::make_unique<Object3dCommon>();
	object3dCommon->Initialize(dxCommon.get());

}

void Framework::Update() {
	// 毎フレーム必ず行う基盤の更新処理
	if (winApp->ProcessMessage()) {
		endRequest_ = true;
	}
	input->Update();
}

void Framework::Finalize() {
	AudioManager::GetInstance()->Finalize();

	//テクスチャマネージャの終了
	TextureManager::GetInstance()->Finalize();
}

void Framework::Run() {

	//ゲームの初期化
	Initialize();

	while (true) {//ゲームループ
		//毎フレーム更新
		Update();

		//終了リクエストが来たら抜ける
		if (IsEndRequest()) {
			break;
		}

		//描画
		Draw();

	}
	//ゲームの終了
	Finalize();
}
