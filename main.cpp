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
#include "Math.h"
#include <numbers>
#include<sstream>
#include <wrl.h>
#include <xaudio2.h>
#define DIRECTINPUT_VERSION    0x0800//DirectInputのバージョン指定
#include <dinput.h>
#include <random>


#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {
		//リソースリークチェック
		Microsoft::WRL::ComPtr <IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);

		}
	}
};


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

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
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

struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

struct Emitter {
	Transform transform; //エミッタのTransform
	uint32_t count;      //発生源
	float frequency;     //発生頻度
	float frequencyTime; //頻度用時刻
};

//Transform変更
Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
Transform cameraTransform{ { 1.0f,1.0f,1.0f }, { std::numbers::pi_v<float> / 3.0f,std::numbers::pi_v<float>,0.0f }, { 0.0f,0.0f,-10.0f } };
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

//パーティクル
std::random_device seedGenerator;
std::mt19937 randomEngine(seedGenerator());
Particle MakeNewParticle(std::mt19937 &randomEngine, const Vector3 &translate) {

	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float>distTime(1.0f, 3.0f);
	Vector3 randomTranslate{ distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	Particle particle;
	particle.transform.scale = { 0.5f,0.5f,0.5f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	particle.transform.translate = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.color = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine),1.0f };
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	return particle;
}

Particle MakeNewCenterParticle(std::mt19937 &randomEngine, const Vector3 &center) {

	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);


	// 小さいランダムオフセット
	std::uniform_real_distribution<float> distPos(-0.1f, 0.1f);
	Vector3 randomOffset = {
		distPos(randomEngine),
		distPos(randomEngine),
		distPos(randomEngine)
	};

	Particle particle;

	// 位置を中心にする
	particle.transform.translate = center + randomOffset;
	//particle.transform.translate = center + randomTranslate;
	// scale調整
	particle.transform.scale = { 0.5f, 0.5f, 0.5f };

	// ランダム速度
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	particle.velocity = { dist(randomEngine), dist(randomEngine), dist(randomEngine) };

	// 寿命
	particle.lifeTime = 1.0f;
	particle.currentTime = 0.0f;

	// 色
	particle.color = { 1,1,1,1 };

	return particle;
}

//エミッターによってパーティクルを発生させる
std::list<Particle> CenterEmit(const Emitter &emitter, std::mt19937 &randomEngine) {
	std::list<Particle> centerParticles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		centerParticles.push_back(MakeNewCenterParticle(randomEngine, emitter.transform.translate));
	}
	return centerParticles;
}

std::list<Particle> Emit(const Emitter &emitter, std::mt19937 &randomEngine) {
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	}
	return particles;
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


IDxcBlob *CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	//Complerに使用するProfils
	const wchar_t *profile,
	//初期化で生成したものを３つ
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *CLSID_DxcCompiler,
	IDxcIncludeHandler *includeHandler,
	std::ostream &os
) {
	//これからシェーダーをコンパイルする旨をログに出す
	Log(os, ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));
	//hlslファイルを読む
	IDxcBlobEncoding *shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		//コンパイル対象のhlslファイル名
		filePath.c_str(),
		//エントリーポイントの指定。基本的にmain以外にはしない
		L"-E",L"main",
		//ShaderProfileの設定
		L"-T",profile,
		//デバック用の情報を埋め込む
		L"-Zi",L"-Qembed_debug",
		//最適化を外しておく
		L"-Od",
		//メモリレイアウトは行優先
		L"-Zpr",
	};
	//実際にShaderをコンパイルする
	IDxcResult *shaderResult = nullptr;
	hr = CLSID_DxcCompiler->Compile(
		//読み込んだファイル
		&shaderSourceBuffer,
		//コンパイルオプション
		arguments,
		//コンパイルオプションの数
		_countof(arguments),
		//includeが含まれた諸々
		includeHandler,
		//コンパイル結果
		IID_PPV_ARGS(&shaderResult)
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8 *shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(os, shaderError->GetStringPointer());
		//警告・エラーダメゼッタイ
		assert(false);
	}

	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob *shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したらログを出す
	Log(os, ConvertString(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));
	//もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, size_t sizeInBytes) {


	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};

	//バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;   //リソースのサイズ。今回はVector4を3頂点分

	//バッファの場合はこれらほ１にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;

	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));


	return vertexResource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHesp(
	const Microsoft::WRL::ComPtr<ID3D12Device> &device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible
) {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}


DirectX::ScratchImage LoadTexture(const std::string &filePath) {
	//テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミップマップ付きのデータを返す
	return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, const DirectX::TexMetadata &metadata) {

	//metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
[[nodiscard]]
UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource> &texture, const DirectX::ScratchImage &mipImages, const Microsoft::WRL::ComPtr<ID3D12Device> &device, const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &commandList) {
	// IntermediateResource(中間リソース)
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);

	// データ転送をコマンドに積む
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	// Textureへの転送は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, int32_t width, int32_t height) {
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}


D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
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



	//comの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

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



	WNDCLASS wc{};
	//ウインドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウインドウクラス名
	wc.lpszClassName = L"CG2";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウインドウクラスを登録する
	RegisterClass(&wc);

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClinentHeight = 720;

	//ウインドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClinentHeight };

	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);





	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"LE2C_16_ササナミ_ソウシ",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	//ウインドウを表示する
	ShowWindow(hwnd, SW_SHOW);



#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif 





	//IDXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {

		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		//ソフトウェアアダプタ出なければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {

			//採用したアダプタの情報をログに出力
			Log(logStream, ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}

		useAdapter = nullptr;
	}

	//適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);


	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};

	const char *featureLevelStrings[] = { "12.2","12.1","12.0" };

	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		//採用したアダプターでデバイス生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		//指定した機能レベルでデバイスを生成できたか確認
		if (SUCCEEDED(hr)) {
			//生成できたのでログ出力を行ってループを抜ける
			Log(logStream, std::format("FeatureLevel:{}\n", featureLevelStrings[i]));
			break;
		}
	}

	//デバイスの生成が上手くいかなかったので起動できない
	assert(device != nullptr);
	Log(logStream, "Complete create D3D12Device!!!\n");


	//DescriptorSIzeを取得しておく
	const uint32_t desriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t desriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t desriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		//ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

	}
#endif

	//コマンドキューを生成
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));

	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドアロケータを生成する
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	//コマンドアロケータを生成がうまくいかなっかたので起動できない
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	//画面の幅
	swapChainDesc.Width = kClientWidth;
	//画面の高さ
	swapChainDesc.Height = kClinentHeight;
	//色の形式
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//マルチサンプルしない
	swapChainDesc.SampleDesc.Count = 1;
	//描画ターゲットとして利用する
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//ダブルバッファ
	swapChainDesc.BufferCount = 2;
	//モニターにうつしたら、中身を破棄
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//コマンドキュー、ウインドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1 **>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	//RTV用のヒープでディスクリプタの数は２。　RTVはShader内で触る物ではないので、shaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHesp(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	//SRV用のヒープでディスクリプタの数は128。SRVはShaderVisibleはtrue
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHesp(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	//DSV用のヒープでディスクリプタの数は１。DSVはShaderVisidleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHesp(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	//SwapChainからResourceを引っ張ってくる
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	//1つ目
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	//2つ目のディスクリプハンドルを得る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);


	//初期値0でFenceを作る
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを持つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	//dxcCompilerを初期化
	IDxcUtils *dxcUtils = nullptr;
	IDxcCompiler3 *dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler *includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//RootParameter作成。複製設定できるので配列
	D3D12_ROOT_PARAMETER rootParamerers[4] = {};
	rootParamerers[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[0].Descriptor.ShaderRegister = 0;           //レジスタ番号0とバインド
	/*rootParamerers[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[1].Descriptor.ShaderRegister = 0;*/

	rootParamerers[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParamerers[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);


	rootParamerers[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParamerers[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParamerers[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[3].Descriptor.ShaderRegister = 1;
	descriptionRootSignature.pParameters = rootParamerers;//ルートパラメータ配列のポインタ
	descriptionRootSignature.NumParameters = _countof(rootParamerers);//配列の長さ


	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

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
	//SoundPlayerWave(xAudio2.Get(), soundData1);

	Window w;
	w.hInstance = GetModuleHandle(nullptr);

	//DirectInputの初期化
	IDirectInput8 *directInput = nullptr;
	result = DirectInput8Create(
		w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void **)&directInput, nullptr);
	assert(SUCCEEDED(result));

	//キーボードデバイスの生成
	IDirectInputDevice8 *keyboard = nullptr;
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	//入力データの形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);//標準形式
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));


	//シリアライズしてバイナルにする
	ID3DBlob *signatureBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	//InputLayoutの作成
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;


	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	IDxcBlob *vertexShaderBlob = CompileShader(L"Particle.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob *pixelShaderBlob = CompileShader(L"Particle.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(pixelShaderBlob != nullptr);




	//PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	//InputLayout
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	//VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	//PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	//BlendState
	graphicsPipelineStateDesc.BlendState = blendDesc;
	//RasterizerState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;


	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジのタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;

	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	//比較関数はLessEqual
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// 自作した数学関数の使用
	Math math;

	//Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};

	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点６つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;

	//1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	//Index用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;


	//インデックスリソースにデータを書き込む
	uint32_t *indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

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
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

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
	Microsoft::WRL::ComPtr<ID3D12Resource> matetialResource = CreateBufferResource(device, sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialData = nullptr;
	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = math.MakeIdentity4x4();


	//Sprite用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialDataSprite = nullptr;

	//書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&materialDataSprite));
	//白で設定
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = math.MakeIdentity4x4();

	//平行光源用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	//マテリアルにデータを書き込む
	DirectionalLight *directionLightData = nullptr;
	//書き込むためのアドレスを取得
	directionLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionLightData));

	directionLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionLightData->direction = { 1.0f,0.0f,0.0f };
	directionLightData->intensity = 1.0f;

	directionLightData->direction = math.Normalize(directionLightData->direction);



	//頂点リソースを作成
	Microsoft::WRL::ComPtr < ID3D12Resource> vertexResourceSphere = CreateBufferResource(device, sizeof(VertexData) * vertexCount);

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
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSphere = CreateBufferResource(device, sizeof(uint32_t) * indexCount);


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
	Microsoft::WRL::ComPtr <ID3D12Resource> matetialResourceSphere = CreateBufferResource(device, sizeof(Material));
	//マテリアルにデータを書き込む
	Material *materialDataSphere = nullptr;
	//書き込むためのアドレスを取得
	matetialResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&materialDataSphere));
	//白で設定
	materialDataSphere->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSphere->enableLighting = true;


	VertexData *VertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&VertexDataSprite));

	//1枚目の三角形
	VertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };
	VertexDataSprite[0].texcoord = { 0.0f,1.0f };
	VertexDataSprite[0].normal = { 0.0f,0.0f,1.0f };
	VertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };
	VertexDataSprite[1].texcoord = { 0.0f,0.0f };
	VertexDataSprite[1].normal = { 0.0f,0.0f,1.0f };
	VertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };
	VertexDataSprite[2].texcoord = { 1.0f,1.0f };
	VertexDataSprite[2].normal = { 0.0f,0.0f,1.0f };

	//2枚目の三角形
	VertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };
	VertexDataSprite[3].texcoord = { 1.0f,0.0f };
	VertexDataSprite[3].normal = { 0.0f,0.0f,1.0f };

	//Sprite用のTransformationMatrix用のリソースをス来る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix *transformetionMatrixDataSprite = nullptr;
	//書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&transformetionMatrixDataSprite));
	//単位行列を書き込んでおく
	*transformetionMatrixDataSprite = TransformationMatrix{ math.MakeIdentity4x4() };

	//Instancing用
	const uint32_t kNumMaxInstance = 100;
	//instancing用のtransformatioMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = CreateBufferResource(device, sizeof(ParticleForGPU) * kNumMaxInstance);
	//書き込む為のアドレスを取得
	ParticleForGPU *instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&instancingData));
	//単位行列を書き込んでいく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		instancingData[index].WVP = math.MakeIdentity4x4();
		instancingData[index].World = math.MakeIdentity4x4();
		instancingData[index].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}




	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	//データを書(き込む
	TransformationMatrix *wvpData = nullptr;
	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->WVP = math.MakeIdentity4x4();


	//Plane用のWVPリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourcePlane = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix *wvpDataPlane = nullptr;
	wvpResourcePlane->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataPlane));
	wvpDataPlane->WVP = math.MakeIdentity4x4();


	// スフィア用のWVPリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSphere = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix *wvpDataSphere = nullptr;
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataSphere));
	wvpDataSphere->WVP = math.MakeIdentity4x4();


	//ビューポート
	D3D12_VIEWPORT viewport{};

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClinentHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//シザー矩形
	D3D12_RECT scissorRecct{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRecct.left = 0;
	scissorRecct.right = kClientWidth;
	scissorRecct.top = 0;
	scissorRecct.bottom = kClinentHeight;




	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata &metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2, mipImages2, device, commandList);

	//2枚目のmetDateを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);

	//instanc用
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 3);
	device->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	//SRVの生成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);



	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/circle.png");
	const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);


	//metDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	////SRVを作成するDescriptorHeapの場所を決める
	//D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	////先頭はImGuiが使っているのでその次を使う
	//textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);

	//SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);


	//DepthStencilTextureをウィンドウのサイズで作成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClinentHeight);


	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	Transform transforms[kNumMaxInstance];
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		transforms[index].scale = { 1.0f, 1.0f, 1.0f };
		transforms[index].rotate = { 0.0f, 0.0f, 0.0f };
		transforms[index].translate = { index * 0.1f, index * 0.1f, index * 0.1f };
	}

	Emitter emitter{};
	emitter.count = 3;
	emitter.transform.translate = { 0.0f,0.0f,0.0f };
	emitter.transform.rotate = { 0.0f,0.0f,0.0f };
	emitter.transform.scale = { 1.0f, 1.0f, 1.0f };
	std::list<Particle> particles;
	std::list<Particle> centerParticles;//中心から出る

	/*for (std::list<Particle>::iterator particleIterator = particles.begin(); particleIterator != particles.end();++particleIterator) {
		particles.transform.scale = { 1.0f, 1.0f, 1.0f };
		particles[index].transform.rotate = { 0.0f, 0.0f, 0.0f };
		particles[index].transform.translate = { index * 0.1f, index * 0.1f, index * 0.1f };
	}*/



	//ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	bool useMonsterBall = true;
	bool isParticleUpdate = true;
	bool useBillboard = true;
	bool useCenterParticle = false;
	bool startParticle = false;
	bool stopParticle = false;

	//07_01 トリガー処理
	bool GetKey(uint8_t key);
	bool LetKey(uint8_t key);
	bool GetKeyMoment(uint8_t key);
	bool LetKeyMoment(uint8_t key);

	const float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
	const float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;

	Vector3 cameraRotateDegree = {
		cameraTransform.rotate.x * kRadianToDegree,
		cameraTransform.rotate.y * kRadianToDegree,
		cameraTransform.rotate.z * kRadianToDegree
	};

	/*if (useBillboard) {
		cameraTransform = { { 1.0f,1.0f,1.0f }, { std::numbers::pi_v<float> / 3.0f,std::numbers::pi_v<float>,0.0f }, { 0.0f,23.0f,10.0f } };
	} else {
		cameraTransform = { { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f  }, { 0.0f,0.0f,-10.0f } };
	}*/
	//メインループ

	MSG msg{};
	while (msg.message != WM_QUIT) {


		//WIndowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			//Update

			//キーボード情報の取得開始
			keyboard->Acquire();
			keyboard->GetDeviceState(sizeof(keys), keys);


			int instanceCount = 10;

			if (GetKey(DIK_0)) {
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
			Matrix4x4 projectionMatrix = math.MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClinentHeight), 0.1f, 100.0f);


			Matrix4x4 backToFrontMatrix = math.MakeRotateYMatrix(std::numbers::pi_v<float>);
			Matrix4x4 billboardMatrix = math.Multiply(backToFrontMatrix, cameraMatrix);
			billboardMatrix.m[3][0] = 0.0f;
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;


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

			//Sprite用のworldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = math.MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = math.MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = math.MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClinentHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = math.Multiply(worldMatrixSprite, math.Multiply(viewMatrixSprite, projectionMatrixSprite));
			transformetionMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;

			//uvTranslate用の行列
			Matrix4x4 uvTransformMatrix = math.MkeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			//instancing用
			for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
				Matrix4x4 worldMatrixInstanc = math.MakeAffineMatrix(transforms[index].scale, transforms[index].rotate, transforms[index].translate);
				Matrix4x4 worldViewProjectionMatrixInstanc = math.Multiply(worldMatrixInstanc, worldViewProjectionMatrix);
				instancingData[index].WVP = worldViewProjectionMatrixInstanc;
				instancingData[index].World = worldMatrixInstanc;
			}



			//particle用
			const float kDeltaTime = 1.0f / 60.0f;
			uint32_t numInstance = 0;
			if (isParticleUpdate && startParticle && !stopParticle) {
				if (!useCenterParticle) {
					for (std::list<Particle>::iterator particleIterator = particles.begin(); particleIterator != particles.end(); ) {
						if ((*particleIterator).lifeTime <= (*particleIterator).currentTime) {
							particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
							particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
							particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
							// 古いパーティクルを erase して次へ進む
							particleIterator = particles.erase(particleIterator);
							continue;
						}



						if (numInstance < kNumMaxInstance) {
							particleIterator->transform.translate.x += particleIterator->velocity.x * kDeltaTime;
							particleIterator->transform.translate.y += particleIterator->velocity.y * kDeltaTime;
							particleIterator->transform.translate.z += particleIterator->velocity.z * kDeltaTime;
							particleIterator->currentTime += kDeltaTime;
							// ワールド行列
							Matrix4x4 worldMatrix =
								math.MakeAffineMatrix(
									particleIterator->transform.scale,
									particleIterator->transform.rotate,
									particleIterator->transform.translate);

							// WVP行列（ビルボード）
							Matrix4x4 WVP = math.Multiply(worldMatrix, billboardMatrix);

							// アルファ値計算（フェードアウト）
							float alpha = 1.0f - (particleIterator->currentTime / particleIterator->lifeTime);

							// インスタンシングデータに書き込み
							instancingData[numInstance].WVP = WVP;
							instancingData[numInstance].World = worldMatrix;
							instancingData[numInstance].color = particleIterator->color;
							instancingData[numInstance].color.w = alpha;

							++numInstance;
						}


						++particleIterator;
					}

					//エミッター
					emitter.frequency = 0.5f;
					emitter.frequencyTime += kDeltaTime;//時刻を決める
					if (emitter.frequency <= emitter.frequencyTime) {
						particles.splice(particles.end(), Emit(emitter, randomEngine));
						emitter.frequencyTime -= emitter.frequency;
					}
				}


				if (useCenterParticle) {
					std::list<Particle> &targetList = useCenterParticle ? centerParticles : particles;

					for (std::list<Particle>::iterator particleIterator = targetList.begin(); particleIterator != targetList.end(); ) {

						if (particleIterator->lifeTime <= particleIterator->currentTime) {

							// 消えたら単純にerase
							particleIterator = targetList.erase(particleIterator);
							continue;
						}

						if (numInstance < kNumMaxInstance) {

							// 位置更新
							particleIterator->transform.translate += particleIterator->velocity * kDeltaTime;
							particleIterator->currentTime += kDeltaTime;

							Matrix4x4 worldMatrix =
								math.MakeAffineMatrix(
									particleIterator->transform.scale,
									particleIterator->transform.rotate,
									particleIterator->transform.translate);

							Matrix4x4 WVP = math.Multiply(worldMatrix, billboardMatrix);

							float alpha = 1.0f - (particleIterator->currentTime / particleIterator->lifeTime);

							instancingData[numInstance].WVP = WVP;
							instancingData[numInstance].World = worldMatrix;
							instancingData[numInstance].color = particleIterator->color;
							instancingData[numInstance].color.w = alpha;

							++numInstance;
						}

						++particleIterator;
					}

					//エミッター
					emitter.frequency = 0.5f;

					emitter.frequencyTime += kDeltaTime;//時刻を決める
					if (emitter.frequency <= emitter.frequencyTime) {
						centerParticles.splice(centerParticles.end(), CenterEmit(emitter, randomEngine));
						emitter.frequencyTime -= emitter.frequency;
					}
				}
			}



			//開発用UIの処理
			ImGui::ShowDemoWindow();

			////カラー
			//ImGui::Begin("MaterialColor");
			//ImGui::ColorEdit4("color", &materialData->color.x);
			//ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
			//ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
			//ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);

			//if (ImGui::CollapsingHeader("Sphere")) {
			//	ImGui::ColorEdit4("color", &materialDataSphere->color.x);
			//	ImGui::DragFloat3("SphereTranslate", &transformSphere.translate.x);
			//	ImGui::DragFloat3("SphereScale", &transformSphere.scale.x);
			//	ImGui::DragFloat3("SphereRotate", &transformSphere.rotate.x);
			//	
			//}

			/*if (ImGui::CollapsingHeader("plane")) {


				ImGui::SliderAngle("planeRotateX", &transformPlane.rotate.x);
				ImGui::SliderAngle("planeRotateY", &transformPlane.rotate.y);
				ImGui::SliderAngle("planeRotateZ", &transformPlane.rotate.z);

			}*/

			if (ImGui::Begin("Camera Control (ImGui)")) {

				// 1. 平行移動 (Translate / Position)
				ImGui::Text("Translate (Position)");
				// &cameraTransform.translate.x のように、Vector3の最初の要素のアドレスを渡します
				ImGui::DragFloat3("##Position", &cameraTransform.translate.x, 0.1f, -100.0f, 100.0f);

				ImGui::Separator();

				// 2. 回転 (Rotate) - 度数法で編集
				ImGui::Text("Rotate (Degree)");
				// 変更があった場合のみ、ラジアンに再変換して cameraTransform に適用
				if (ImGui::DragFloat3("##Rotation", &cameraRotateDegree.x, 1.0f, -360.0f, 360.0f)) {
					cameraTransform.rotate.x = cameraRotateDegree.x * kDegreeToRadian;
					cameraTransform.rotate.y = cameraRotateDegree.y * kDegreeToRadian;
					cameraTransform.rotate.z = cameraRotateDegree.z * kDegreeToRadian;
				}

				ImGui::Separator();

				// 3. スケール (Scale)
				ImGui::Text("Scale");
				ImGui::DragFloat3("##Scale", &cameraTransform.scale.x, 0.01f, 0.01f, 10.0f);

				ImGui::End();
			}
			if (ImGui::Begin("Particle")) {
				// ---- パーティクル開始／停止 ----
				if (ImGui::Button("Start Particle")) {
					startParticle = true;
					stopParticle = false;
				}

				ImGui::SameLine();
				if (ImGui::Button("Stop Particle")) {
					stopParticle = true;
					startParticle = false;
				}

				if (!useCenterParticle) {
					if (ImGui::Button("Add Particle")) {
						startParticle = true;
						particles.splice(particles.end(), Emit(emitter, randomEngine));
					}
				}

				if (useCenterParticle) {
					if (ImGui::Button("Emit Center Particle")) {
						startParticle = true;
						centerParticles.splice(centerParticles.end(), Emit(emitter, randomEngine));
					}
				}
				ImGui::Checkbox("Center Particle", &useCenterParticle);
				ImGui::DragFloat3("EmitterTranslate", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);				ImGui::Checkbox("Particle Update", &isParticleUpdate);
				//ImGui::Checkbox("Use Billboard", &useBillboard);
			};
			//ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			/*ImGui::DragFloat4("Lightcolor", &directionLightData->color.x);
			ImGui::SliderFloat3("LightDirection", &directionLightData->direction.x, -1.0f, 1.0f);
			ImGui::DragFloat("LightIntensity", &directionLightData->intensity);
			if (ImGui::CollapsingHeader("Sprite")) {
				ImGui::DragFloat3("SpriteTranslate", &transformSprite.translate.x);
				ImGui::DragFloat3("SpriteScale", &transformSprite.scale.x);
				ImGui::DragFloat3("SpriteRotate", &transformSprite.rotate.x);
			}
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);*/
			ImGui::End();

			//ゲーム処理

			//Draw

			//ImGuiの内部コマンドを生成
			ImGui::Render();

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			//TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			//今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//バリアを張る対象のリソース。現在のバックバッファに対して使う
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			//前のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			//後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);



			//描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			//指定した色での画面全体をクリアする
			float clearColor[] = { 0.1f,0.25,0.5,1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			//指定した深度で画面全体をクリアする
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());


			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRecct);

			//RootSignatureを設定。PSOに設定しているけど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(graphicsPipelineState.Get());
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->IASetIndexBuffer(nullptr);


			//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えて置けばよい
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

			//TransformationMatrixCbufferの場所を設定
			//commandList->SetGraphicsRootConstantBufferView(1, wvpResourcePlane->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);

			commandList->SetGraphicsRootConstantBufferView(3, directionLightResource->GetGPUVirtualAddress());

			//SRVのDescriptorの先頭を設定
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

			//描画
			//commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

			//マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());


			////Supriteの描画。
			//commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			//commandList->IASetIndexBuffer(&indexBufferViewSprite);

			////TransformationMatrixCbufferの場所を設定
			//commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

			////描画
			////commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

			////Shereの描画
			//commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
			//commandList->IASetIndexBuffer(&indexBufferViewShere);

			////マテリアルCBufferの場所を設定
			//commandList->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress());

			//// 描画
			////commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);

			//instancing用
			commandList->DrawInstanced(UINT(modelData.vertices.size()), numInstance, 0, 0);


			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

			//今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			//コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			//GPUにコマンドリストの実行を行わせる
			Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
			//GPUとのosに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			//Fenceの値を更新
			fenceValue++;
			//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);

			//Fenceの値が指定したSignal値にたどり着いているか確認する
			//GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue) {
				//指定したSignalにたどり着いていないのでたどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				//イベント待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			//次のフレーム用のコマンドリスト準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));

		}


	}

	//出力ウィンドへの文字 
	Log(logStream, "Hello,DirectX!\n");
	Log(logStream, ConvertString(std::format(L"windth:{}\n", kClientWidth)));



	//文字列を格納する
	std::string str0{ "STRING" };

	//整数を文字列にする
	std::string str1{ std::to_string(10) };

	CloseWindow(hwnd);

	// 解放処理
	CloseHandle(fenceEvent);


	//xAudio2解放
	xAudio2.Reset();

	//音声データ解放
	SoundUnload(&soundData1);

	signatureBlob->Release();
	if (errorBlob) {
		errorBlob->Release();
	}
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();


	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//COMの終了処理
	CoUninitialize();



	return 0;
}


