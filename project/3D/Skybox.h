#pragma once
#include "MyMath.h"
#include "SkyboxCommon.h"
#include "engine/Camera/Camera.h"
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <memory>

class Skybox {
public:
    // スカイボックス用の頂点構造体（座標のみ）
    struct VertexData {
        Vector4 position;
    };

    // シェーダーに送る定数バッファ用構造体
    struct ConstBufferDataViewProjection {
        Matrix4x4 view;
        Matrix4x4 projection;
    };

    void Initialize(const std::string &textureFilePath);
    void Update(Camera *camera);
    void Draw();

private:
    void CreateVertexData();
    void CreateConstBuffer();

    SkyboxCommon *skyboxCommon_ = nullptr;

    // 頂点リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // 定数バッファ（行列用）
    Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
    ConstBufferDataViewProjection *constMap_ = nullptr;

    // テクスチャパス
    std::string textureFilePath_;

    // 行列計算用
    std::unique_ptr<MyMath> math_;
};
