#pragma once
#include <memory>
#include "Framework.h"
#include "engine/Scene/SceneFactory.h"
#include "engine/Graphics/RenderTexture.h"
#include "engine/Graphics/PostEffect.h"

class Game : public Framework {
public:

	//初期化
	void Initialize() override;

	//終了
	void Finalize() override;

	//更新
	void Update() override;

	//描画
	void Draw() override;

private:

	//シーンファクトリー
	void HandleResize();

	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	// オフスクリーンレンダリング用のテクスチャ
	std::unique_ptr<RenderTexture> renderTexture_;

	// ポストエフェクト描画用のテクスチャ (ImGuiのGame Viewに貼る用)
	std::unique_ptr<RenderTexture> postEffectTexture_;

	// ポストエフェクト
	std::unique_ptr<PostEffect> postEffect_;

	// 深度バッファのSRVの番号を覚えておく変数
	uint32_t depthSrvIndex_ = 0;

	// ImGuiを表示するかどうか
	bool showImGui_ = true;

	uint32_t renderWidth_ = 0;
	uint32_t renderHeight_ = 0;
};

