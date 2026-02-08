#include "ParticleManager.h"
#include <cassert>
#include "Logger.h"
#include "TextureManager.h"
#include "SrvManager.h"
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

// 文字列変換用ヘルパー（もしStringUtilityなどがなければこれを使ってください）
std::string ConvertString(const std::wstring &str);

IDxcBlob *CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	// Compilerに使用するProfile
	const wchar_t *profile,
	// 初期化で生成したものを3つ
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler, // 変数名をわかりやすく変更
	IDxcIncludeHandler *includeHandler) {
	// 1. これからシェーダーをコンパイルする旨をログに出す
	// Loggerクラスのインスタンス(logger)を使用
	std::string message = ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile));
	logger.Log(message);

	// 2. hlslファイルを読む
	ComPtr<IDxcBlobEncoding> shaderSource = nullptr; // ComPtrに変更
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 3. 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		filePath.c_str(),          // コンパイル対象のhlslファイル名
		L"-E", L"main",            // エントリーポイント
		L"-T", profile,            // ShaderProfile
		L"-Zi", L"-Qembed_debug",  // デバッグ情報
		L"-Od",                    // 最適化なし
		L"-Zpr",                   // 行優先
	};

	// 4. 実際にShaderをコンパイルする
	ComPtr<IDxcResult> shaderResult = nullptr; // ComPtrに変更
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	// 致命的なエラーチェック
	assert(SUCCEEDED(hr));

	// 5. 警告・エラーが出てたらログに出して止める
	ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		// エラー内容をログに出す
		std::string errorMsg = shaderError->GetStringPointer();
		logger.Log(errorMsg);
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	// 6. コンパイル結果から実行用のバイナリ部分を取得
	ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	// 7. 成功したらログを出す
	message = ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile));
	logger.Log(message);

	// ComPtrを使っているので手動Releaseは不要です

	// 実行用のバイナリを返却 (Detachして所有権を呼び出し元に移す)
	return shaderBlob.Detach();
}


void ParticleManager::Initialize(DirectXCommon *dxCommon) {
   // -------------------------------------------------------------
   // 引数でDirectXCommonとSrvManagerのポインタを受け取って記録する
   // -------------------------------------------------------------

	dxCommon_ = dxCommon;

	//デバイスを取得しておく
	ID3D12Device *device = dxCommon_->GetDevice();

	// -------------------------------------------------------------
	// ランダムエンジンの初期化
	// -------------------------------------------------------------

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// -------------------------------------------------------------
	// パイプライン生成 (RootSignatureとPSO)
	// -------------------------------------------------------------

	CreateRootSignature();
	CreatePipelineState();

	// -------------------------------------------------------------
	// 頂点データの初期化
	// -------------------------------------------------------------

	//パーティクルを板ポリゴンとして描画するための４頂点
	VertexData vertices[4];
	//左下
	vertices[0].position = { -1.0f,-1.0f,0.0f,1.0f };
	vertices[0].texcoord = { 0.0f,1.0f };
	vertices[0].normal = { 0.0f,0.0f,-1.0f };
	//左上
	vertices[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };
	vertices[1].texcoord = { 0.0f,0.0f };
	vertices[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertices[2].position = { 1.0f,-1.0f,0.0f,1.0f };
	vertices[2].texcoord = { 1.0f,1.0f };
	vertices[2].normal = { 0.0f,0.0f,-1.0f };
	//右上
	vertices[3].position = { 1.0f,1.0f,0.0f,1.0f };
	vertices[3].texcoord = { 1.0f,0.0f };
	vertices[3].normal = { 0.0f,0.0f,-1.0f };

	// -------------------------------------------------------------
	// 頂点リソース生成
	// -------------------------------------------------------------

	// 頂点バッファのサイズ = 頂点1つのサイズ * 頂点数
	UINT sizeInBytes = sizeof(VertexData) * 4;

	// ヒーププロパティ設定(Upload)
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソース設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource_)
	);
	assert(SUCCEEDED(hr));

	// -------------------------------------------------------------
	// 頂点バッファビュー (VBV) 作成
	// -------------------------------------------------------------
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeInBytes;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// -------------------------------------------------------------
	// 頂点リソースに頂点データを書き込む
	// -------------------------------------------------------------
	VertexData *vertexMap = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexMap));

	// 全頂点データをメモリコピー
	std::memcpy(vertexMap, vertices, sizeof(vertices));

	vertexResource_->Unmap(0, nullptr);
}

void ParticleManager::Update(Camera* camera) {
	assert(camera); // カメラがないと描画計算ができません

	// 1. カメラ情報の取得
	Matrix4x4 matView = camera->GetViewMatrix();
	Matrix4x4 matProjection = camera->GetProjectionMatrix();

	// 2. ビルボード行列の計算
	// カメラのワールド行列の回転成分を取り出すことで、常にカメラの方を向く行列を作れます
	Matrix4x4 matBillboard = camera->GetWorldMatrix();
	// 平行移動成分は除去する（回転のみ利用するため）
	matBillboard.m[3][0] = 0.0f;
	matBillboard.m[3][1] = 0.0f;
	matBillboard.m[3][2] = 0.0f;
	matBillboard.m[3][3] = 1.0f;


	// 3. 全てのパーティクルグループについて処理する (スライド)
	// particleGroups は std::unordered_map<std::string, ParticleGroup> と想定
	for (auto &[name, group] : particleGroups_) {

		uint32_t numInstance = 0; // 今回描画するインスタンス数

		MyMath myMath; // 行列計算用インスタンス

		// 4. グループ内の全てのパーティクルについて処理する (スライド)
		// std::list なので、途中で削除(erase)しても良いようにイテレータを使う
		for (auto it = group.particle.begin(); it != group.particle.end(); ) {

			// ■ 寿命に達していたらグループから外す (スライド)
			if (it->currentTime >= it->lifeTime) {
				it = group.particle.erase(it); // 削除して次のイテレータを受け取る
				continue;                       // 次のループへ
			}

			// ■ 場の影響を計算（加速） (スライド)
			// 例: 重力を加える場合など (今回は省略可、必要なら以下のように記述)
			// it->velocity.y -= 0.01f; 

			// ■ 移動処理（速度を座標に加算） (スライド)
			it->transform.translate.x += it->velocity.x;
			it->transform.translate.y += it->velocity.y;
			it->transform.translate.z += it->velocity.z;

			// ■ 経過時間を加算 (スライド)
			const float kDeltaTime = 1.0f / 60.0f; // 固定60fps想定
			it->currentTime += kDeltaTime;


			// ■ ワールド行列を計算 (スライド)
			// ビルボード行列を使うため、回転計算は matBillboard を使用
			// 手順: スケール -> 回転(ビルボード) -> 平行移動
			Matrix4x4 scaleMat = myMath.MkeScaleMatrix(it->transform.scale);
			Matrix4x4 translateMat = myMath.MakeTranslateMatrix(it->transform.translate);

			// 行列の合成 (Scale * Billboard * Translate)
			Matrix4x4 worldMatrix = myMath.Multiply(scaleMat, matBillboard);
			worldMatrix = myMath.Multiply(worldMatrix, translateMat);


			// ■ ワールドビュープロジェクション行列を合成 (スライド)
			Matrix4x4 worldViewProjection = myMath.Multiply(worldMatrix, myMath.Multiply(matView, matProjection));


			// ■ インスタンシング用データ1個分の書き込み (スライド)
			// インスタンス数が最大数を超えない範囲で書き込む
			if (numInstance < group.kNumMaxInstance) {

				group.instancingDataPtr[numInstance].WVP = worldViewProjection;
				group.instancingDataPtr[numInstance].World = worldMatrix;
				group.instancingDataPtr[numInstance].color = it->color;

				// (応用) フェードアウト処理などを入れたい場合はここで color.w を操作
				// float alpha = 1.0f - (it->currentTime / it->lifeTime);
				// group.instancingDataPtr[numInstance].color.w = alpha;

				numInstance++; // 書き込んだ件数をカウント
			}

			++it; // 次のパーティクルへ
		}

		// ※描画時に「何個描画するか」が必要になるので、ここで記録しておくと便利です
		// group.numActiveInstances = numInstance; 
	}
}


void ParticleManager::Draw() {
	// 1. コマンドリストの取得
	ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

	// 2. パイプラインステート等の共通設定
	// これから使うルートシグネチャとPSOをセット
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// プリミティブ形状の設定（三角形リスト）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 3. グループごとの描画ループ
	for (auto &[name, group] : particleGroups_) {

		// 描画するインスタンス数（現在のパーティクル数）
		UINT instanceCount = static_cast<UINT>(group.particle.size());

		// 上限を超えていたら切る（安全策）
		if (instanceCount > group.kNumMaxInstance) {
			instanceCount = group.kNumMaxInstance;
		}

		// パーティクルが0個なら描画しない
		if (instanceCount == 0) {
			continue;
		}

		// 4. リソースのバインド (SrvManagerを利用)
		// ルートパラメータの番号は CreateRootSignature の設定順序に依存します。
		// ParticleManager::CreateRootSignature では次の順序です:
		// [0]=CBV (pixel), [1]=DescriptorTable (vertex) = instancing, [2]=DescriptorTable (pixel) = texture, [3]=CBV (pixel)

		// (1) テクスチャのSRVをセット -> RootParameter index 2
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, group.textureSrvIndex);

		// (2) インスタンシングデータのSRVをセット -> RootParameter index 1
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);

		// 5. 描画コマンド発行 (DrawInstanced)
		// 頂点数6 (四角形=三角形2枚), インスタンス数, ...
		commandList->DrawInstanced(6, instanceCount, 0, 0);
	}
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath) {
	//登録済みの名前なら何もしない
	if (particleGroups_.contains(name)) {
		return;
	}

	//新しいグループを作成
	ParticleGroup group;
	group.kNumMaxInstance = 1000;// このグループの最大パーティクル数

	// テクスチャの読み込み & SRVインデックス取得
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group.textureFilePath = textureFilePath;
	// テクスチャのSRVインデックスを取得 (TextureManagerの実装に合わせて書き換えてください)
	group.textureSrvIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);


	// 2. インスタンシング用リソース (StructuredBuffer) の生成
	// GPUに送るデータ(ParticleForGPU)を kNumMaxInstance 個分確保する
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleForGPU) * group.kNumMaxInstance);

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&group.instancingResource)
	);
	assert(SUCCEEDED(hr));


	// 3. データを書き込むためのメモリをマッピング
	hr = group.instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&group.instancingDataPtr));
	assert(SUCCEEDED(hr));
	// 初期化（念のためゴミが入らないようにゼロ埋めしても良い）
	std::memset(group.instancingDataPtr, 0, sizeof(ParticleForGPU) * group.kNumMaxInstance);


	// 4. StructuredBuffer用のSRVを作成
	// SrvManagerを使って空いているSRV番号を確保する
	group.instancingSrvIndex = SrvManager::GetInstance()->Allocate();

	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		group.instancingSrvIndex,
		group.instancingResource.Get(),
		group.kNumMaxInstance,
		sizeof(ParticleForGPU)
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = group.kNumMaxInstance;
	srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;


	// 確保した場所にSRVを作る
	dxCommon_->GetDevice()->CreateShaderResourceView(
		group.instancingResource.Get(),
		&srvDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(group.instancingSrvIndex)
	);


	// 5. マップに登録
	particleGroups_[name] = group;
}

void ParticleManager::Emit(const std::string name, const Vector3 &position, uint32_t count) {
	// 1. 登録済みのパーティクルグループを検索
	if (particleGroups_.find(name) == particleGroups_.end()) {
		assert(false); // 指定された名前のグループが見つからない
		return;
	}

	// グループの参照を取得
	ParticleGroup &group = particleGroups_[name];

	// 2. ランダム生成器の準備
	// staticをつけることで、関数呼び出しのたびに初期化されるのを防ぐ（高速化）
	static std::random_device seed_gen;
	static std::mt19937_64 engine(seed_gen());

	// ランダムの分布設定 (必要に応じて数値を調整してください)
	std::uniform_real_distribution<float> distDir(-1.0f, 1.0f);   // 方向用 (-1.0 ~ 1.0)
	std::uniform_real_distribution<float> distPos(-0.1f, 0.1f);   // 発生位置のズレ
	std::uniform_real_distribution<float> distLife(1.0f, 3.0f);   // 寿命 (1秒 ~ 3秒)

	// 3. 指定された数だけパーティクルを生成
	for (uint32_t i = 0; i < count; ++i) {
		Particle newParticle{};

		// ■ Transform (位置・スケール・回転)
		newParticle.transform.scale = { 1.0f, 1.0f, 1.0f };
		newParticle.transform.rotate = { 0.0f, 0.0f, 0.0f };

		// 指定された座標に、少しランダムなズレを加える（一点から重なって出ないように）
		newParticle.transform.translate = {
			position.x + distPos(engine),
			position.y + distPos(engine),
			position.z + distPos(engine)
		};

		// ■ Velocity (速度)
		// ランダムなベクトルを作成して正規化し、スピードを掛ける例
		// ここでは簡易的にxyzそれぞれランダムに設定
		const float kSpeed = 0.1f; // 移動スピード
		newParticle.velocity = {
			distDir(engine) * kSpeed,
			distDir(engine) * kSpeed,
			distDir(engine) * kSpeed
		};

		// ■ Color (色)
		newParticle.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白

		// ■ Lifetime (寿命)
		newParticle.lifeTime = distLife(engine);
		newParticle.currentTime = 0.0f; // 経過時間は0からスタート

		// リストに追加
		group.particle.push_back(newParticle);
	}
}

void ParticleManager::CreateRootSignature() {
	HRESULT hr = S_FALSE;

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

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//RootParameter作成。複製設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;           //レジスタ番号0とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列のポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

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

	//シリアライズしてバイナルにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		std::string errorMsg = reinterpret_cast<char *>(errorBlob->GetBufferPointer());
		logger.Log(errorMsg);
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)
	);
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePipelineState() {
	HRESULT hr = S_FALSE;

	//dxcCompilerを初期化
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	

	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	vertexShaderBlob = CompileShader(L"Particle.VS.hlsl",
		L"vs_6_0", dxcUtils.Get(),dxcCompiler.Get(),includeHandler.Get());
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	pixelShaderBlob = CompileShader(L"Particle.PS.hlsl",
		L"ps_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(pixelShaderBlob != nullptr);


	//----------------頂点レイアウト----------------
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

	//----------------パイプラインステートの設定----------------
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;

	//PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	//InputLayout
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	//VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	//PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	
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

	//----------------生成----------------
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		logger.Log("Failed to create GraphicsPipelineState for Particle.\n");
		assert(false);
	}
}
