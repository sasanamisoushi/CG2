#include "Skybox.h"
#include "engine/Resource/TextureManager.h"
#include <cassert>

void Skybox::Initialize(const std::string &textureFilePath) {
    skyboxCommon_ = SkyboxCommon::GetInstance();
    textureFilePath_ = textureFilePath;

    // 行列計算オブジェクトを初期化
    math_ = std::make_unique<MyMath>();

    // テクスチャのロード（前回のTextureManagerの拡張が前提）
    TextureManager::GetInstance()->LoadTexture(textureFilePath_);

    CreateVertexData();
    CreateConstBuffer();
}

void Skybox::Update(Camera *camera) {
    // カメラの行列を定数バッファに書き込む
    // ※VS側で平行移動を消す処理を入れているため、ここではそのまま渡してOK
    constMap_->view = camera->GetViewMatrix();
    constMap_->projection = camera->GetProjectionMatrix();
}

void Skybox::Draw() {
    // 1. スカイボックス共通の描画設定（PSO, RootSignatureなど）
    skyboxCommon_->SetCommonDrawSettings();

    auto commandList = skyboxCommon_->GetDxCommon()->GetCommandList();

    // 2. VBセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 3. 定数バッファセット (b0: ViewProjection)
    commandList->SetGraphicsRootConstantBufferView(0, constBuffer_->GetGPUVirtualAddress());

    // 4. テクスチャセット (t0: TextureCube)
    commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

    // 5. 描画（36頂点）
    commandList->DrawInstanced(36, 1, 0, 0);
}

void Skybox::CreateVertexData() {
    // 立方体の8頂点を定義（中心が原点）
    // 頂点インデックスを使わず、三角形リストとして展開（36頂点）
    std::vector<VertexData> vertices = {
        // 前面
        {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f, -1.0f, 1.0f}}, {{ -1.0f, -1.0f, -1.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f, -1.0f, 1.0f}}, {{  1.0f, -1.0f, -1.0f, 1.0f}},
        // 背面
        {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{  1.0f, -1.0f,  1.0f, 1.0f}},
        {{ 1.0f, -1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{ -1.0f, -1.0f,  1.0f, 1.0f}},
        // 左面
        {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ -1.0f, -1.0f,  1.0f, 1.0f}},
        {{-1.0f, -1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ -1.0f, -1.0f, -1.0f, 1.0f}},
        // 右面
        {{ 1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{  1.0f, -1.0f, -1.0f, 1.0f}},
        {{ 1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{  1.0f, -1.0f,  1.0f, 1.0f}},
        // 上面
        {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{ -1.0f,  1.0f, -1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{  1.0f,  1.0f, -1.0f, 1.0f}},
        // 下面
        {{-1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f, -1.0f, -1.0f, 1.0f}}, {{ -1.0f, -1.0f,  1.0f, 1.0f}},
        {{-1.0f, -1.0f,  1.0f, 1.0f}}, {{ 1.0f, -1.0f, -1.0f, 1.0f}}, {{  1.0f, -1.0f,  1.0f, 1.0f}},
    };

    UINT size = sizeof(VertexData) * (UINT)vertices.size();
    vertexResource_ = skyboxCommon_->GetDxCommon()->CreateBufferResource(size);

    VertexData *vData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vData));
    std::memcpy(vData, vertices.data(), size);
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = size;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Skybox::CreateConstBuffer() {
    constBuffer_ = skyboxCommon_->GetDxCommon()->CreateBufferResource(sizeof(ConstBufferDataViewProjection));
    constBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&constMap_));
}