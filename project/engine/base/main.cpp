#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <dbghelp.h>
#include <strsafe.h>
#include "MyMath.h"
#include <numbers>
#include <xaudio2.h>
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "SrvManager.h"


#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"Dbghelp.lib")



#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception) {

	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	return EXCEPTION_EXECUTE_HANDLER;
}

std::wstring ConvertString(const std::string &str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring &str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

void Log(std::ostream &os, const std::string &message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}



//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージを処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}



//チャンクヘッダ
struct ChunkHeader {
	char id[4];  //チャンク毎のID
	int32_t size; //チャンクサイズ
};

//RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; //"RIFF"
	char type[4];   //"WAVE"
};

//FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; //"fmt"
	WAVEFORMATEX fmt;  //波形フォーマット
};

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE *pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};

SoundData SoundLoadWave(const char *filename) {
	//HRESULT result;

	//ファイル入力ストリームのインスタンス
	std::ifstream file;
	//wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイルオープン失敗を検出する
	assert(file.is_open());

	//RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	//ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	//タイプかWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char *)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);

	//Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	//JUNKチャンクを検出
	if (strncmp(data.id, "JUNK", 4) == 0) {
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再度読み込み
		file.read((char *)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	//Dataチャンクのデータ部
	char *pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	//Waveファイルを閉じる
	file.close();

	//returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE *>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

//音声データ解放
void SoundUnload(SoundData *soundData) {
	//バッファのメモリ
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void SoundPlayerWave(IXAudio2 *xAudio2, const SoundData &soundData) {
	HRESULT result;

	//波形ファーマットを元にSourceVoiceの生成
	IXAudio2SourceVoice *pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	//波形データ
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}

struct Window {
	HINSTANCE hInstance;
	HWND hwnd;

};

//全キーの入力状態を取得する
BYTE keys[256] = {};
BYTE preKey[256] = {};


bool GetKey(uint8_t key) {
	return keys[key] & 0x80;
}

bool LetKey(uint8_t key) {
	return !(keys[key] & 0x80);
}

bool GetKeyMoment(uint8_t key) {
	return (keys[key] & 0x80) && !(preKey[key] & 0x80);
}

bool LetKeyMoment(uint8_t key) {
	return !(keys[key] & 0x80) && (preKey[key] & 0x80);
}
//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	D3DResourceLeakChecker leakCheck;

	//ポインタ
	WinApp *winApp = nullptr;
	DirectXCommon *dxCommon = nullptr;
	SpriteCommon *spriteCommon = nullptr;

#pragma region 基盤システムの初期化

	//windowsAppの初期化
	winApp = new WinApp();
	winApp->Initialize();

	// DirectXCommon の生成と初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);
	dxCommon->GetCommandList();
	dxCommon->InitializeImGui();

	SrvManager *srvManager = nullptr;
	//SRVマネージャの初期化
	srvManager = new SrvManager();
	srvManager->Initialize(dxCommon);

	//テクスチャマネージャの初期化
	TextureManager::GetInstance()->Initialiaze(dxCommon,srvManager);

	//3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon);

	//スプライト共通部の初期化
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	Object3dCommon *object3dCommon = nullptr;
	//3Dオブジェクト共通部の初期化
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

	//テクスチャマネージャの初期化
	//TextureManager::GetInstance()->Initialiaze(dxCommon, srvManager);

#pragma endregion 基盤システムの初期化
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	Sprite *sprite = new Sprite();
	sprite->Initialize(spriteCommon, "resources/uvChecker.png");

	Camera *camera = new Camera;
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	object3dCommon->SetDefaultCamera(camera);

#pragma region 最初のシーンの初期化

	/*ModelCommon *modelCommon= new ModelCommon();
	modelCommon->Initialize(dxCommon);*/

	

	/*Model *model = new Model();
	model->Initialize(modelCommon);*/

	//3Dオブジェトの初期化
	//Object3d *object3d = new Object3d();
	//object3d->Initialize(object3dCommon);

	//.objファイルからモデルを読み込み
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// 描画する物体のリスト
	std::vector<Object3d *> objects;

	// 1つ目のオブジェクト（plane）
	Object3d *objA = new Object3d();
	objA->Initialize(object3dCommon);
	//初期化済みの3Dオブジェトにモデルを紐づける
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f, 0.0f, 0.0f }; // 左に配置
	objects.push_back(objA);

	// 2つ目のオブジェクト
	Object3d *objB = new Object3d();
	objB->Initialize(object3dCommon);
	//初期化済みの3Dオブジェトにモデルを紐づける
	objB->SetModel("plane.obj");
	objB->transform.translate = { 2.0f, 0.0f, 0.0f }; // 右に配置
	objects.push_back(objB);

#pragma endregion 最初のシーンの終了


	/*std::vector<Sprite *>sprites;
	for (uint32_t i = 0; i < 5; ++i) {
		Sprite *sprite = new Sprite();
		sprite->Initialize(spriteCommon, "resources/uvChecker.png");


		Vector2 pos = sprite->GetPosition();
		pos = { 10.0f + i * 200.0f,100.0f };
		sprite->SetPosition(pos);
		Vector2 size = sprite->GetSize();
		size.x = 100.0f;
		size.y = 100.0f;
		sprite->SetSize(size);

		sprites.push_back(sprite);
	}*/




	//誰も捕捉しなかった場合に補足する関数の登録
	SetUnhandledExceptionFilter(ExportDump);

	//ログの出力用のフォルダ
	std::filesystem::create_directory("logs");
	//現在時刻を取得
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//コンマを削り秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間に変更
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日時分秒の文字に変換
	std::string dateSting = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateSting + "log";
	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);




#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif 


	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	Microsoft::WRL::ComPtr<IXAudio2>xAudio2;
	IXAudio2MasteringVoice *masterVoice;

	HRESULT result;

	//XAudioエンジンのインスタンスの生成
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	//マスターボイスの生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	//音声読み込み
	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");

	//音声再生
	SoundPlayerWave(xAudio2.Get(), soundData1);

	Window w;
	w.hInstance = GetModuleHandle(nullptr);

	//--------------入力デバイス--------------
	Input *input = nullptr;


	input = new Input();
	input->Initialize(winApp);

	// 自作した数学関数の使用
	MyMath math;

	//分裂数
	const uint32_t kSubdivision = 16;

	//経度分割1つ分の角度
	const float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

	//緯度分割1つ分の角度
	const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

	//必要な頂点数
	const uint32_t vertexCount = (kSubdivision + 1) * (kSubdivision + 1);

	//インデックス数
	const uint32_t indexCount = kSubdivision * kSubdivision * 6;

	

	//マテリアル用のリソースを作る。今回はcolor１つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> matetialResource = dxCommon->CreateBufferResource(sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialData = nullptr;
	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = math.MakeIdentity4x4();


	//bool useMonsterBall = true;

	//07_01 トリガー処理
	bool GetKey(uint8_t key);
	bool LetKey(uint8_t key);
	bool GetKeyMoment(uint8_t key);
	bool LetKeyMoment(uint8_t key);

	//メインループ

	MSG msg{};
	while (msg.message != WM_QUIT) {

		//Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			//ゲームループを抜ける
			break;
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		//Update

		//キーボード情報の取得開始
	/*	keyboard->Acquire();
		keyboard->GetDeviceState(sizeof(keys), keys);*/



		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("Hit 0\n");
		}

		//Transformの更新
		//transform.rotate.y += 0.03f;
		//Matrix4x4 worldMatrix = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);


		////cameraMatrix
		//Matrix4x4 cameraMatrix = math.MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

		////viewMatrix
		//Matrix4x4 viewMatrix = math.Inverse(cameraMatrix);

		////projectionMatrix
		//Matrix4x4 projectionMatrix = math.MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClinentHeight), 0.1f, 100.0f);

		//カメラの更新
		camera->Update();

		for (Object3d *object3d : objects) {
			object3d->Update();
		}

		////worldViewProjectionMatrix
		//Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));
		//wvpData->WVP = worldViewProjectionMatrix;
		//wvpData->World = worldMatrix;

		////Plane用のworldViewProjectionMatrixを作る
		//Matrix4x4 worldMatrixPlane = math.MakeAffineMatrix(transformPlane.scale, transformPlane.rotate, transformPlane.translate);
		//Matrix4x4 worldViewProjectionMatrixPlane = math.Multiply(worldMatrixPlane, math.Multiply(viewMatrix, projectionMatrix));
		//wvpDataPlane->WVP = worldViewProjectionMatrixPlane;
		//wvpDataPlane->World = worldMatrixPlane;


		////Sphere用のworldViewProjectionMatrixを作る
		//Matrix4x4 worldMatrixSphere = math.MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
		//Matrix4x4 worldViewProjectionMatrixSphere = math.Multiply(worldMatrixSphere, math.Multiply(viewMatrix, projectionMatrix));
		//wvpDataSphere->WVP = worldViewProjectionMatrixSphere;
		//wvpDataSphere->World = worldMatrixSphere;

		//現在の座標を変数で受ける
		Vector2 position = sprite->GetPosition();
		//座標を変更する
		position = Vector2{ 200.0f,200.0f };
		//変更を反映する
		sprite->SetPosition(position);

		Vector2 size = sprite->GetSize();
		size.x = 300.0f;
		size.y = 300.0f;
		sprite->SetSize(size);

		////角度を変化させるテスト
		//float rotation = sprite->GetRotation();
		//rotation += 0.01f;
		////変更を反映する
		//sprite->SetRotation(rotation);

		////色を変化させるテスト
		//Vector4 color = sprite->GetColor();
		//color.x += 0.01f;
		//if (color.x > 1.0f) {
		//	color.x -= 1.0f;
		//}
		//sprite->SetColor(color);

		////サイズを変更させるテスト
		//Vector2 size = sprite->GetSize();
		//size.x += 0.1f;
		//size.y += 0.1f;
		//sprite->SetSize(size);

		/*const int kSwitchInterval = 60;
		static int frameCounter = 0;
		frameCounter++;*/
		//for (uint32_t i = 0; i < sprites.size(); ++i) {
			//Sprite *sprite = sprites[i];

			//// 偶数番目のスプライト i (0, 2, 4...) を対象とする
			//if (i % 2 == 0) {

			//	// 1秒ごとに画像を切り替える
			//	if ((frameCounter / kSwitchInterval) % 2 == 0) {
			//		sprite->textureReplacement("resources/uvChecker.png");
			//	} else {
			//		sprite->textureReplacement("resources/monsterBall.png");
			//	}
			//}
		sprite->Update();
		//}



		////uvTranslate用の行列
		//Matrix4x4 uvTransformMatrix = math.MkeScaleMatrix(uvTransformSprite.scale);
		//uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeRotateZMatrix(uvTransformSprite.rotate.z));
		//uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeTranslateMatrix(uvTransformSprite.translate));
		//materialDataSprite->uvTransform = uvTransformMatrix;


		//開発用UIの処理
		/*ImGui::ShowDemoWindow();


		ImGui::Begin("Object3d Controller");
		ImGui::Text("Model Transform");*/
		/*ImGui::DragFloat3("Translate", &object3d->transform.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate", &object3d->transform.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &object3d->transform.scale.x, 0.01f);*/
	

		
		// カメラも操作したい場合
		/*ImGui::Separator();
		ImGui::Text("Camera Transform");*/
		/*ImGui::DragFloat3("Cam Pos", &object3d->cameraTransform.translate.x, 0.01f);
		ImGui::DragFloat3("Cam Rot", &object3d->cameraTransform.rotate.x, 0.01f);
		*/

		//ImGui::End();
		////カラー
		//ImGui::Begin("MaterialColor");
		//ImGui::ColorEdit4("color", &materialData->color.x);
		//ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
		//ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
		//ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);

		/*if (ImGui::CollapsingHeader("Sphere")) {
			ImGui::ColorEdit4("color", &materialDataSphere->color.x);
			ImGui::DragFloat3("SphereTranslate", &transformSphere.translate.x);
			ImGui::DragFloat3("SphereScale", &transformSphere.scale.x);
			ImGui::DragFloat3("SphereRotate", &transformSphere.rotate.x);

		}*/

	/*	if (ImGui::CollapsingHeader("plane")) {


			ImGui::SliderAngle("planeRotateX", &transformPlane.rotate.x);
			ImGui::SliderAngle("planeRotateY", &transformPlane.rotate.y);
			ImGui::SliderAngle("planeRotateZ", &transformPlane.rotate.z);

		}*/
		//ImGui::Checkbox("useMonsterBall", &useMonsterBall);

		//ImGui::DragFloat4("Lightcolor", &directionLightData->color.x);
		//ImGui::SliderFloat3("LightDirection", &directionLightData->direction.x, -1.0f, 1.0f);
		//ImGui::DragFloat("LightIntensity", &directionLightData->intensity);
		/*if (ImGui::CollapsingHeader("Sprite")) {
			ImGui::DragFloat3("SpriteTranslate", &transformSprite.translate.x);
			ImGui::DragFloat3("SpriteScale", &transformSprite.scale.x);
			ImGui::DragFloat3("SpriteRotate", &transformSprite.rotate.x);
		}
		ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
		ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
		ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
		ImGui::End();*/

		//ゲーム処理

		//Draw

		//ImGuiの内部コマンドを生成
		//ImGui::Render();



		//描画前処理
		dxCommon->PreDraw();

		
		//3Dオブジェトの描画準備
		object3dCommon->SetCommonDrawSettings();
		//3Dオブジェクトの描画
		for (Object3d *object3d : objects) {
			object3d->Draw();
		}
		//model->Draw();


		//Spriteの描画基準
		spriteCommon->SetCommonPipelineState();
		//スプライト描画
		//for (Sprite *sprite : sprites) {
		sprite->Draw();
		//}

		

		//dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
		//dxCommon->GetCommandList()->IASetIndexBuffer(nullptr);


		////形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えて置けばよい
		//dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		////マテリアルCBufferの場所を設定
		//dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

		////TransformationMatrixCbufferの場所を設定
		//dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourcePlane->GetGPUVirtualAddress());

		//dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionLightResource->GetGPUVirtualAddress());

		////SRVのDescriptorの先頭を設定
		//dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

		//描画
		//dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

		

		

		////Shereの描画
		//dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
		//dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewShere);

		////マテリアルCBufferの場所を設定
		//dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress());

		// 描画
		//dxCommon->GetCommandList()->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);

		//実際のcommandListのImGuiの描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());

		//描画後処理
		dxCommon->PostDraw();
	}

	//出力ウィンドへの文字 
	Log(logStream, "Hello,DirectX!\n");
	Log(logStream, ConvertString(std::format(L"windth:{}\n", WinApp::kClientWidth)));



	//文字列を格納する
	std::string str0{ "STRING" };

	//整数を文字列にする
	std::string str1{ std::to_string(10) };

	//WindowaAPIの終了処理
	winApp->Finalize();

	//テクスチャマネージャの終了
	TextureManager::GetInstance()->Finalize();

	//３Dモデルマネージャの初期化
	ModelManager::GetInstance()->Finalize();

	// 解放処理
	CloseHandle(dxCommon->GetFenceEvent()); 

	delete input;
	delete winApp;
	delete dxCommon;
	delete object3dCommon;
	delete srvManager;
	for (Object3d *object3d : objects) {
		delete object3d;
	}
	delete spriteCommon;
	/*delete model;
	delete modelCommon;*/
	//for (Sprite *sprite : sprites) {
	delete sprite;
	/*}
	sprites.clear();*/

	//xAudio2解放
	xAudio2.Reset();

	//音声データ解放
	SoundUnload(&soundData1);


	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	return 0;
}