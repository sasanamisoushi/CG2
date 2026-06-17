#pragma once
#include "engine/math/MyMath.h"
#include <string>
#include <vector>
#include <memory>

// jsonの実体を完全に隠蔽するための専用構造体を宣言
struct JsonData;

class JsonHelper {
public:
	// JSONファイルを開いて内部にロードする
	bool LoadFile(const std::string &filePath);

	// 基本型の安全な取得(キーが存在しない場合はデフォルト値を返す)
	float GetFloat(const std::string &key, float defaultValue = 0.0f) const;
	int GetInt(const std::string &key, int defaultValue = 0) const;
	bool GetBool(const std::string &key, bool defaultValue = false) const;
	std::string GetString(const std::string &key, const std::string &defaultValue = "") const;

	// ゲーム数学型の取得ヘルパー
	Vector3 GetVector3(const std::string &key, const Vector3 &defaultValue = { 0.0f,0.0f,0.0f }) const;
	Vector4 GetVector4(const std::string &key, const Vector4 &defaultValue = { 1.0f,1.0f,1.0f,1.0f }) const;

	// 配列やオブジェクトなどの構造化データの存在チェック
	bool HasKey(const std::string &key) const;

private:
	// 内部構造の隠蔽 (Pimplパターン)
	std::unique_ptr<JsonData> data_;
};
