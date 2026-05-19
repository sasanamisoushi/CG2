#include "JsonHelper.h"
#include "externals/json.hpp" // ここでのみインクルード！
#include <fstream>
#include <cassert>
#include <Windows.h> // OutputDebugStringA を使うため

// cppファイルの中だけで json の実体を持つ構造体を定義する
struct JsonData {
	nlohmann::json root;
};

bool JsonHelper::LoadFile(const std::string &filePath) {
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		OutputDebugStringA((" [JsonHelper] File not found: " + filePath + "\n").c_str());
		return false;
	}

	data_ = std::make_unique<JsonData>();

	try {
		ifs >> data_->root;
	} catch (const nlohmann::json::parse_error &e) {
		OutputDebugStringA((" [JsonHelper] Parse error: " + std::string(e.what()) + "\n").c_str());
		assert(false && "JSON Parse Error!");
		return false;
	}
	return true;
}

// 追加：キーの存在チェック（先ほど抜けていた関数）
bool JsonHelper::HasKey(const std::string &key) const {
	if (!data_) return false;
	return data_->root.contains(key);
}

float JsonHelper::GetFloat(const std::string &key, float defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	return data_->root[key].get<float>();
}

int JsonHelper::GetInt(const std::string &key, int defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	return data_->root[key].get<int>();
}

bool JsonHelper::GetBool(const std::string &key, bool defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	return data_->root[key].get<bool>();
}

std::string JsonHelper::GetString(const std::string &key, const std::string &defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	return data_->root[key].get<std::string>();
}

Vector3 JsonHelper::GetVector3(const std::string &key, const Vector3 &defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	auto &node = data_->root[key];

	// Blenderの配列形式 [x, y, z] またはオブジェクト形式 {"x":0, "y":0, "z":0} の両方に対応
	if (node.is_array() && node.size() >= 3) {
		return Vector3{ node[0].get<float>(), node[1].get<float>(), node[2].get<float>() };
	} else if (node.is_object()) {
		return Vector3{
			node.value("x", defaultValue.x),
			node.value("y", defaultValue.y),
			node.value("z", defaultValue.z)
		};
	}
	return defaultValue;
}

Vector4 JsonHelper::GetVector4(const std::string &key, const Vector4 &defaultValue) const {
	if (!HasKey(key)) return defaultValue;
	auto &node = data_->root[key];

	if (node.is_array() && node.size() >= 4) {
		return Vector4{ node[0].get<float>(), node[1].get<float>(), node[2].get<float>(), node[3].get<float>() };
	}
	return defaultValue;
}
