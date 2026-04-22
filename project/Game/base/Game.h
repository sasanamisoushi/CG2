#pragma once
#include <memory>
#include "Framework.h"
#include "SceneFactory.h"
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
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	// オフスクリーンレンダリング用のテクスチャ
	std::unique_ptr<RenderTexture> renderTexture_;

	// ポストエフェクト
	std::unique_ptr<PostEffect> postEffect_;
};

