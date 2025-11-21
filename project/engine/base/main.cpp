#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <fstream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include<dxgidebug.h>
#include <dxcapi.h>
#include "externals/DirectXTex/d3dx12.h"
#include <vector>
#include "MyMath.h"
#include <numbers>
#include<sstream>
#include <wrl.h>
#include <xaudio2.h>
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"


#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"xaudio2.lib")

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);





struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct  VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal; //05_03で追加
};


struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct MaterialData {
	std::string textureFilePath;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color; //ライトの色
	Vector3 direction;
	float intensity;
};

struct ModelData {
	std::vector<VertexData>vertices;
	MaterialData material;
};

//Transform変更
Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
Transform cameraTransform{ { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-10.0f } };
Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
Transform transformPlane{ {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f, 0.0f, 0.0f} };
Transform transformSphere{ {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f, 0.0f, 0.0f} };

Transform uvTransformSprite{
	{1.0f,1.0f,1.0f},
	{0.0f,0.0f,0.0f},
	{0.0f,0.0f,0.0f},
};



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

MaterialData LoadMaterialtemplateFile(const std::string &directoryPath, const std::string &filename) {

	//構築するMaterialData
	MaterialData materialData;
	//ファイルから読んだ1行を格納するもの
	std::string line;
	//ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	//とりあえず開けなかったら止める
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連続してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納する

	std::ifstream file(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open()); //とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を読む


		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			//面は三角形限定
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのindexは「位置/uv/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');  //区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}

				//要素へのindexから、実際の要素の値を取得して頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				VertexData vertex = { position,texcord,normal };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcord,normal };
			}

			//頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			//matrialTemplateLidraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在されるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialtemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
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

	//スプライト共通部の初期化
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

#pragma endregion 基盤システムの初期化

	

	Sprite *sprite = new Sprite();
	sprite->Initialize(spriteCommon);
	
	
	

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

	//モデル読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");

	//頂点リソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	//頂点バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	////リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点3つのサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//1頂点当たりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//頂点リソースデータを書き込む
	VertexData *vertexData = nullptr;

	//書き込む為のアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

	//頂点データをリソースにコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	//マテリアル用のリソースを作る。今回はcolor１つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> matetialResource = dxCommon->CreateBufferResource( sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialData = nullptr;
	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = math.MakeIdentity4x4();


	
	//平行光源用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	//マテリアルにデータを書き込む
	DirectionalLight *directionLightData = nullptr;
	//書き込むためのアドレスを取得
	directionLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionLightData));

	directionLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionLightData->direction = { 1.0f,0.0f,0.0f };
	directionLightData->intensity = 1.0f;

	directionLightData->direction = math.Normalize(directionLightData->direction);



	//頂点リソースを作成
	Microsoft::WRL::ComPtr < ID3D12Resource> vertexResourceSphere = dxCommon->CreateBufferResource(sizeof(VertexData) * vertexCount);

	//頂点バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};

	////リソースの先頭のアドレスから使う
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点3つのサイズ
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * vertexCount;
	//1頂点当たりのサイズ
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	//頂点リソースデータを書き込む
	VertexData *vertexDataSphere = nullptr;

	//書き込む為のアドレスを取得
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&vertexDataSphere));

	//スフィア用のインデックス
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSphere = dxCommon->CreateBufferResource(sizeof(uint32_t) * indexCount);


	for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
		float latAngle = -std::numbers::pi_v<float> / 2.0f + kLatEvery * lat;

		for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
			float lonAngle = lon * kLonEvery;

			uint32_t index = lat * (kSubdivision + 1) + lon;

			float x = std::cosf(latAngle) * std::cosf(lonAngle);
			float y = std::sinf(latAngle);
			float z = std::cosf(latAngle) * std::sinf(lonAngle);

			vertexDataSphere[index].position = { x, y, z, 1.0f };
			vertexDataSphere[index].texcoord = {
				float(lon) / float(kSubdivision),
				1.0f - float(lat) / float(kSubdivision)
			};
			vertexDataSphere[index].normal = { x, y, z };
		}
	}

	vertexResourceSphere->Unmap(0, nullptr);



	// インデックスデータ書き込み
	uint32_t *indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&indexDataSphere));

	uint32_t idx = 0;
	for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
		for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
			uint32_t current = lat * (kSubdivision + 1) + lon;
			uint32_t next = current + (kSubdivision + 1);

			// 三角形1
			indexDataSphere[idx++] = current;
			indexDataSphere[idx++] = next;
			indexDataSphere[idx++] = current + 1;

			// 三角形2
			indexDataSphere[idx++] = current + 1;
			indexDataSphere[idx++] = next;
			indexDataSphere[idx++] = next + 1;
		}
	}

	indexResourceSphere->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewShere{};
	//リソースの先頭のアドレスから使う
	indexBufferViewShere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();

	indexBufferViewShere.SizeInBytes = sizeof(uint32_t) * indexCount;

	indexBufferViewShere.Format = DXGI_FORMAT_R32_UINT;

	//マテリアル用のリソースを作る。今回はcolor１つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> matetialResourceSphere = dxCommon->CreateBufferResource(sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialDataSphere = nullptr;
	//書き込むためのアドレスを取得
	matetialResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&materialDataSphere));
	//白で設定
	materialDataSphere->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSphere->enableLighting = true;

	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	//データを書(き込む
	TransformationMatrix *wvpData = nullptr;
	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->WVP = math.MakeIdentity4x4();


	//Plane用のWVPリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourcePlane = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix *wvpDataPlane = nullptr;
	wvpResourcePlane->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataPlane));
	wvpDataPlane->WVP = math.MakeIdentity4x4();


	// スフィア用のWVPリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSphere = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix *wvpDataSphere = nullptr;
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataSphere));
	wvpDataSphere->WVP = math.MakeIdentity4x4();


	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = dxCommon->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata &metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = dxCommon->CreateTextureResource( metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = dxCommon->UploadTextureData(textureResource2, mipImages2);

	//2枚目のmetDateを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 2);

	//SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);



	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = dxCommon->LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = dxCommon->CreateTextureResource( metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon->UploadTextureData(textureResource, mipImages);


	//metDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	//先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	textureSrvHandleCPU = dxCommon->GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 1);
	textureSrvHandleGPU = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 1);

	//SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);


	//DepthStencilTextureをウィンドウのサイズで作成
	dxCommon->depthBufferGeneration(WinApp::kClientWidth, WinApp::kClinentHeight);


	
	bool useMonsterBall = true;

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
			Matrix4x4 worldMatrix = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);


			//cameraMatrix
			Matrix4x4 cameraMatrix = math.MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

			//viewMatrix
			Matrix4x4 viewMatrix = math.Inverse(cameraMatrix);

			//projectionMatrix
			Matrix4x4 projectionMatrix = math.MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClinentHeight), 0.1f, 100.0f);


			//worldViewProjectionMatrix
			Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			//Plane用のworldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixPlane = math.MakeAffineMatrix(transformPlane.scale, transformPlane.rotate, transformPlane.translate);
			Matrix4x4 worldViewProjectionMatrixPlane = math.Multiply(worldMatrixPlane, math.Multiply(viewMatrix, projectionMatrix));
			wvpDataPlane->WVP = worldViewProjectionMatrixPlane;
			wvpDataPlane->World = worldMatrixPlane;


			//Sphere用のworldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSphere = math.MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			Matrix4x4 worldViewProjectionMatrixSphere = math.Multiply(worldMatrixSphere, math.Multiply(viewMatrix, projectionMatrix));
			wvpDataSphere->WVP = worldViewProjectionMatrixSphere;
			wvpDataSphere->World = worldMatrixSphere;

			////現在の座標を変数で受ける
			//Vector2 position = sprite->GetPosition();
			////座標を変更する
			//position += Vector2{ 0.1f,0.1f };
			////変更を反映する
			//sprite->SetPosition(position);
			
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

			//サイズを変更させるテスト
			Vector2 size = sprite->GetSize();
			size.x += 0.1f;
			size.y += 0.1f;
			sprite->SetSize(size);


			sprite->Update();

			////uvTranslate用の行列
			//Matrix4x4 uvTransformMatrix = math.MkeScaleMatrix(uvTransformSprite.scale);
			//uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeRotateZMatrix(uvTransformSprite.rotate.z));
			//uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeTranslateMatrix(uvTransformSprite.translate));
			//materialDataSprite->uvTransform = uvTransformMatrix;


			//開発用UIの処理
			ImGui::ShowDemoWindow();

			//カラー
			ImGui::Begin("MaterialColor");
			ImGui::ColorEdit4("color", &materialData->color.x);
			ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
			ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
			ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);

			if (ImGui::CollapsingHeader("Sphere")) {
				ImGui::ColorEdit4("color", &materialDataSphere->color.x);
				ImGui::DragFloat3("SphereTranslate", &transformSphere.translate.x);
				ImGui::DragFloat3("SphereScale", &transformSphere.scale.x);
				ImGui::DragFloat3("SphereRotate", &transformSphere.rotate.x);
				
			}

			if (ImGui::CollapsingHeader("plane")) {


				ImGui::SliderAngle("planeRotateX", &transformPlane.rotate.x);
				ImGui::SliderAngle("planeRotateY", &transformPlane.rotate.y);
				ImGui::SliderAngle("planeRotateZ", &transformPlane.rotate.z);

			}
			//ImGui::Checkbox("useMonsterBall", &useMonsterBall);

			ImGui::DragFloat4("Lightcolor", &directionLightData->color.x);
			ImGui::SliderFloat3("LightDirection", &directionLightData->direction.x, -1.0f, 1.0f);
			ImGui::DragFloat("LightIntensity", &directionLightData->intensity);
			if (ImGui::CollapsingHeader("Sprite")) {
				ImGui::DragFloat3("SpriteTranslate", &transformSprite.translate.x);
				ImGui::DragFloat3("SpriteScale", &transformSprite.scale.x);
				ImGui::DragFloat3("SpriteRotate", &transformSprite.rotate.x);
			}
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			ImGui::End();

			//ゲーム処理

			//Draw

			//ImGuiの内部コマンドを生成
			ImGui::Render();

			

			//描画前処理
			dxCommon->PreDraw();

			//Spriteの描画基準
			spriteCommon->SetCommonPipelineState();

			
			dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
			dxCommon->GetCommandList()->IASetIndexBuffer(nullptr);


			//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えて置けばよい
			dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//マテリアルCBufferの場所を設定
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

			//TransformationMatrixCbufferの場所を設定
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourcePlane->GetGPUVirtualAddress());

			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionLightResource->GetGPUVirtualAddress());

			//SRVのDescriptorの先頭を設定
			dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

			//描画
			//dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

			//スプライト描画
			sprite->Draw();

			

			//Shereの描画
			dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
			dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewShere);

			//マテリアルCBufferの場所を設定
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress());

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

	// 解放処理
	CloseHandle(dxCommon->GetFenceEvent());

	delete input;
	delete winApp;
	delete dxCommon;

	delete spriteCommon;
	delete sprite;

	//xAudio2解放
	xAudio2.Reset();

	//音声データ解放
	SoundUnload(&soundData1);


	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	return 0;
}