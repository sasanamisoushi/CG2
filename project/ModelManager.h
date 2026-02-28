#pragma once
#include <map>
#include <string>
#include <memory>
#include <Model.h>
class ModelManager {
public:
	//シングルトンインスタンスの取得
	static ModelManager *GetInstance();
	//終了
	void Finalize();

	//初期化
	void Initialize(DirectXCommon *dxCommon);

	//モデルファイルの読み込み
	void LoadModel(const std::string &filePath);

	//モデルの検索
	Model *FindModel(const std::string &filePath);


private:
	static ModelManager *instance;

	//コンストラクタ
	ModelManager() = default;

	//デストラクタ
	~ModelManager() = default;

	//コピーコンストラクタ
	ModelManager(const ModelManager &) = delete;

	//コピー代入演算子
	ModelManager &operator=(const ModelManager &) = delete;

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>>models;

	std::unique_ptr<ModelCommon> modelCommon;



};

