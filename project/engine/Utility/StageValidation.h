#pragma once

#include "externals/json.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef CG2_ENABLE_STAGE_VALIDATION
#include <Windows.h>
#endif

namespace StageValidation {

struct CheckItemResult {
	std::string label;
	std::string level = "OK";
	std::string detail;
	std::vector<std::string> messages;
};

struct ValidationMarker {
	std::string level;
	std::string message;
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Report {
	std::string source;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
	std::vector<CheckItemResult> checkItems;
	std::vector<ValidationMarker> markers;
	uint64_t revision = 0;

	bool HasErrors() const { return !errors.empty(); }
	bool HasWarnings() const { return !warnings.empty(); }
	bool HasMessages() const { return HasErrors() || HasWarnings(); }
	bool HasCheckItems() const { return !checkItems.empty(); }
	bool HasMarkers() const { return !markers.empty(); }
};

inline Report lastReport;

#ifdef CG2_ENABLE_STAGE_VALIDATION

inline constexpr double kEpsilon = 0.0001;

inline std::string ToString(size_t value) {
	return std::to_string(static_cast<unsigned long long>(value));
}

inline bool IsNumberArray(const nlohmann::json &value, size_t expectedSize) {
	if (!value.is_array() || value.size() != expectedSize) {
		return false;
	}

	for (const auto &entry : value) {
		if (!entry.is_number()) {
			return false;
		}
	}

	return true;
}

inline bool HasNearZeroAxis(const nlohmann::json &value) {
	if (!IsNumberArray(value, 3)) {
		return false;
	}

	for (const auto &entry : value) {
		if (std::abs(entry.get<double>()) <= kEpsilon) {
			return true;
		}
	}

	return false;
}

inline std::string GetObjectName(const nlohmann::json &object, size_t index) {
	if (object.contains("name") && object["name"].is_string()) {
		return object["name"].get<std::string>();
	}

	return "object[" + ToString(index) + "]";
}

inline std::string GetBaseName(const std::string &name) {
	const size_t dotPos = name.find('.');
	if (dotPos == std::string::npos) {
		return name;
	}
	return name.substr(0, dotPos);
}

inline bool StartsWith(const std::string &text, const std::string &prefix) {
	return text.rfind(prefix, 0) == 0;
}

inline bool ContainsString(const std::vector<std::string> &values, const std::string &target) {
	for (const std::string &value : values) {
		if (value == target) {
			return true;
		}
	}
	return false;
}

inline bool IsUnsetString(const std::string &value) {
	return value.empty() || value == "None";
}

inline bool IsValidColliderType(const std::string &type) {
	return type == "Box" || type == "BOX";
}

inline bool ContainsText(const std::string &text, const std::string &target) {
	return text.find(target) != std::string::npos;
}

inline int SeverityRank(const std::string &level) {
	if (level == "ERROR") {
		return 2;
	}
	if (level == "WARNING") {
		return 1;
	}
	return 0;
}

inline const std::vector<std::pair<std::string, std::string>> &CheckDefinitions() {
	static const std::vector<std::pair<std::string, std::string>> definitions = {
		{ "export_targets", "出力対象の有無" },
		{ "player_spawn", "PLAYER配置" },
		{ "enemy_respawn_presence", "敵リスポーン地点配置" },
		{ "stage_bounds_presence", "StageBounds配置" },
		{ "spawn_inside_stage_bounds", "スポーンがStageBounds内にある" },
		{ "spawn_buried", "リスポーン地点の埋まり" },
		{ "respawn_duplicates", "リスポーン地点の重複" },
		{ "base_name_duplicates", "名前ベース重複" },
		{ "parenting", "親子関係" },
		{ "path_id_duplicates", "飛行パスID重複" },
		{ "path_points", "飛行パス制御点数" },
		{ "path_speed", "飛行パス速度" },
		{ "path_distance", "飛行パス距離" },
		{ "path_far_coordinates", "飛行パスの遠すぎる座標" },
		{ "unused_paths", "未使用の飛行パス" },
		{ "path_reference", "敵から参照する飛行パス" },
		{ "collider_type", "コライダー種別" },
		{ "collider_size", "Boxコライダーサイズ" },
		{ "aabb_rotation", "AABB想定Boxコライダー回転" },
		{ "negative_scale", "マイナススケール" },
		{ "far_coordinates", "異常に遠い座標" },
		{ "empty_mesh", "空っぽのメッシュ" },
		{ "filename", "ファイル名禁則文字" },
		{ "unapplied_transform", "トランスフォーム未適用" },
		{ "uv_map", "UVマップ" },
		{ "ngon", "Nゴン" },
		{ "material", "マテリアル割り当て" },
		{ "category", "ゲーム用種類設定" },
		{ "obstacle_type", "障害物メッシュ設定" },
		{ "obstacle_collider", "障害物コライダー設定" },
		{ "enemy_type", "敵タイプ設定" },
		{ "scale_zero", "0に近いスケール" },
		{ "stage_bounds_category", "StageBounds種類設定" },
		{ "other", "その他" },
	};
	return definitions;
}

inline std::string ClassifyValidationMessage(const std::string &message) {
	if (ContainsText(message, "出力対象")) return "export_targets";
	if (ContainsText(message, "StageBoundsの外")) return "spawn_inside_stage_bounds";
	if (ContainsText(message, "障害物に埋まっています")) return "spawn_buried";
	if (ContainsText(message, "敵リスポーン地点が重複")) return "respawn_duplicates";
	if (StartsWith(message, "PLAYER ")) return "player_spawn";
	if (ContainsText(message, "敵リスポーン地点がありません")) return "enemy_respawn_presence";
	if (StartsWith(message, "StageBounds が")) return "stage_bounds_presence";
	if (ContainsText(message, "名前のベースが重複")) return "base_name_duplicates";
	if (ContainsText(message, "親子関係")) return "parenting";
	if (ContainsText(message, "飛行パスIDが重複")) return "path_id_duplicates";
	if (ContainsText(message, "飛行パスには") || ContainsText(message, "points 配列")) return "path_points";
	if (ContainsText(message, "飛行パスの速度") || ContainsText(message, "speed は")) return "path_speed";
	if (ContainsText(message, "飛行パスの距離が0") || ContainsText(message, "距離0の制御点区間")) return "path_distance";
	if (ContainsText(message, "飛行パスに絶対値")) return "path_far_coordinates";
	if (ContainsText(message, "使われていない飛行パス")) return "unused_paths";
	if (ContainsText(message, "飛行パスID") && ContainsText(message, "見つかりません")) return "path_reference";
	if (ContainsText(message, "collider の値") || ContainsText(message, "collider.type")) return "collider_type";
	if (ContainsText(message, "Box コライダーのサイズ") || ContainsText(message, "Box collider の size")) return "collider_size";
	if (ContainsText(message, "AABB想定")) return "aabb_rotation";
	if (ContainsText(message, "マイナススケール")) return "negative_scale";
	if (ContainsText(message, "絶対値")) return "far_coordinates";
	if (ContainsText(message, "空っぽのメッシュ")) return "empty_mesh";
	if (ContainsText(message, "ファイル名")) return "filename";
	if (ContainsText(message, "トランスフォームが未適用")) return "unapplied_transform";
	if (ContainsText(message, "UVマップ")) return "uv_map";
	if (ContainsText(message, "Nゴン")) return "ngon";
	if (ContainsText(message, "マテリアル")) return "material";
	if (ContainsText(message, "種類が未設定") || ContainsText(message, "category")) return "category";
	if (ContainsText(message, "OBSTACLE は MESH") || ContainsText(message, "障害物は MESH")) return "obstacle_type";
	if (ContainsText(message, "OBSTACLE にコライダー")) return "obstacle_collider";
	if (ContainsText(message, "enemy_type")) return "enemy_type";
	if (ContainsText(message, "スケールに 0 に近い軸") || ContainsText(message, "StageBounds のサイズ")) return "scale_zero";
	if (ContainsText(message, "StageBounds は")) return "stage_bounds_category";
	return "other";
}

inline size_t Utf8SafePrefixLength(const std::string &text, size_t maxBytes) {
	if (text.size() <= maxBytes) {
		return text.size();
	}

	size_t prefixLength = maxBytes;
	while (prefixLength > 0 && prefixLength < text.size()) {
		const unsigned char byte = static_cast<unsigned char>(text[prefixLength]);
		if ((byte & 0xC0) != 0x80) {
			break;
		}
		--prefixLength;
	}
	return prefixLength;
}

inline std::string ShortenText(const std::string &text, size_t maxLength = 140) {
	if (text.size() <= maxLength) {
		return text;
	}
	if (maxLength <= 3) {
		return text.substr(0, maxLength);
	}

	const size_t prefixLength = Utf8SafePrefixLength(text, maxLength - 3);
	return text.substr(0, prefixLength) + "...";
}

inline void AddUniqueString(std::vector<std::string> &values, const std::string &message) {
	if (!ContainsString(values, message)) {
		values.push_back(message);
	}
}

inline void DeduplicateStrings(std::vector<std::string> &values) {
	std::vector<std::string> uniqueValues;
	for (const std::string &value : values) {
		AddUniqueString(uniqueValues, value);
	}
	values = std::move(uniqueValues);
}

inline void ApplyCheckMessage(std::vector<CheckItemResult> &items, const std::string &level, const std::string &message) {
	const std::string checkId = ClassifyValidationMessage(message);
	CheckItemResult *target = nullptr;

	for (size_t index = 0; index < items.size(); ++index) {
		if (CheckDefinitions()[index].first == checkId) {
			target = &items[index];
			break;
		}
	}

	if (!target && !items.empty()) {
		target = &items.back();
	}
	if (!target) {
		return;
	}

	if (SeverityRank(level) > SeverityRank(target->level)) {
		target->level = level;
	}
	AddUniqueString(target->messages, message);
}

inline std::vector<CheckItemResult> BuildCheckItems(const std::vector<std::string> &errors, const std::vector<std::string> &warnings, const std::vector<CheckItemResult> &exportedItems = {}) {
	std::vector<CheckItemResult> items;
	for (const auto &definition : CheckDefinitions()) {
		CheckItemResult item;
		item.label = definition.second;
		items.push_back(item);
	}

	for (const CheckItemResult &exportedItem : exportedItems) {
		for (CheckItemResult &item : items) {
			if (item.label != exportedItem.label) {
				continue;
			}

			if (SeverityRank(exportedItem.level) > SeverityRank(item.level)) {
				item.level = exportedItem.level;
			}
			item.detail = exportedItem.detail;
			for (const std::string &message : exportedItem.messages) {
				AddUniqueString(item.messages, message);
			}
			break;
		}
	}

	for (const std::string &message : errors) {
		ApplyCheckMessage(items, "ERROR", message);
	}
	for (const std::string &message : warnings) {
		ApplyCheckMessage(items, "WARNING", message);
	}

	for (CheckItemResult &item : items) {
		if (item.detail.empty() && !item.messages.empty()) {
			item.detail = item.messages.front();
			if (item.messages.size() > 1) {
				item.detail += " ほか" + ToString(item.messages.size() - 1) + "件";
			}
		}
		item.detail = ShortenText(item.detail);
	}

	return items;
}

inline std::string StripCarriageReturn(std::string text) {
	if (!text.empty() && text.back() == '\r') {
		text.pop_back();
	}
	return text;
}

inline void AddStringList(std::vector<std::string> &target, const nlohmann::json &values) {
	if (!values.is_array()) {
		return;
	}

	for (const auto &value : values) {
		if (value.is_string()) {
			AddUniqueString(target, value.get<std::string>());
		}
	}
}

inline std::vector<CheckItemResult> ParseExportedCheckItems(const nlohmann::json &validationData) {
	std::vector<CheckItemResult> items;
	if (!validationData.contains("checks") || !validationData["checks"].is_string()) {
		return items;
	}

	std::istringstream stream(validationData["checks"].get<std::string>());
	std::string line;
	while (std::getline(stream, line)) {
		line = StripCarriageReturn(line);
		if (line.empty()) {
			continue;
		}

		const size_t firstTab = line.find('\t');
		if (firstTab == std::string::npos) {
			continue;
		}
		const size_t secondTab = line.find('\t', firstTab + 1);
		if (secondTab == std::string::npos) {
			continue;
		}

		CheckItemResult item;
		item.level = line.substr(0, firstTab);
		if (item.level != "OK" && item.level != "WARNING" && item.level != "ERROR") {
			item.level = "OK";
		}
		item.label = line.substr(firstTab + 1, secondTab - firstTab - 1);
		item.detail = line.substr(secondTab + 1);
		items.push_back(item);
	}

	return items;
}

inline std::vector<CheckItemResult> ReadExportedValidation(Report &report, const nlohmann::json &root) {
	if (!root.contains("validation") || !root["validation"].is_object()) {
		return {};
	}

	const auto &validationData = root["validation"];
	if (validationData.contains("errors")) {
		AddStringList(report.errors, validationData["errors"]);
	}
	if (validationData.contains("warnings")) {
		AddStringList(report.warnings, validationData["warnings"]);
	}

	return ParseExportedCheckItems(validationData);
}

inline void AddNameByBase(std::vector<std::pair<std::string, std::vector<std::string>>> &namesByBase, const std::string &baseName, const std::string &name) {
	for (auto &entry : namesByBase) {
		if (entry.first == baseName) {
			entry.second.push_back(name);
			return;
		}
	}

	namesByBase.push_back({ baseName, { name } });
}

struct Vec3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct StageBoundsInfo {
	std::string name;
	Vec3 center;
	Vec3 halfExtents;
	Vec3 rotationDegrees;
};

struct SpawnPointInfo {
	std::string name;
	std::string label;
	Vec3 position;
};

struct LabelPositionInfo {
	std::string name;
	Vec3 position;
};

inline Vec3 ReadVec3(const nlohmann::json &value) {
	return {
		value[0].get<double>(),
		value[1].get<double>(),
		value[2].get<double>(),
	};
}

inline Vec3 ToGamePosition(Vec3 blenderPosition) {
	return {
		blenderPosition.x,
		blenderPosition.z,
		blenderPosition.y,
	};
}

inline ValidationMarker MakeMarker(const std::string &level, const std::string &message, Vec3 position) {
	ValidationMarker marker;
	marker.level = level;
	marker.message = ShortenText(message, 96);
	marker.x = position.x;
	marker.y = position.y;
	marker.z = position.z;
	return marker;
}

inline bool MessageReferencesName(const std::string &message, const std::string &name) {
	return StartsWith(message, name + ":") || ContainsText(message, name);
}

inline void AddMarkersForMessages(
	std::vector<ValidationMarker> &markers,
	const std::vector<std::string> &messages,
	const std::string &level,
	const std::vector<LabelPositionInfo> &labelPositions) {

	for (const std::string &message : messages) {
		for (const LabelPositionInfo &labelPosition : labelPositions) {
			if (!MessageReferencesName(message, labelPosition.name)) {
				continue;
			}

			markers.push_back(MakeMarker(level, message, labelPosition.position));
			break;
		}
	}
}

inline std::vector<ValidationMarker> BuildMarkers(
	const std::vector<std::string> &errors,
	const std::vector<std::string> &warnings,
	const std::vector<LabelPositionInfo> &labelPositions) {

	std::vector<ValidationMarker> markers;
	AddMarkersForMessages(markers, errors, "ERROR", labelPositions);
	AddMarkersForMessages(markers, warnings, "WARNING", labelPositions);
	return markers;
}

inline Vec3 AbsVec3(Vec3 value) {
	return {
		std::abs(value.x),
		std::abs(value.y),
		std::abs(value.z),
	};
}

inline double DegreesToRadians(double degrees) {
	return degrees * 3.14159265358979323846 / 180.0;
}

inline Vec3 RotateX(Vec3 value, double radians) {
	const double s = std::sin(radians);
	const double c = std::cos(radians);
	return { value.x, value.y * c - value.z * s, value.y * s + value.z * c };
}

inline Vec3 RotateY(Vec3 value, double radians) {
	const double s = std::sin(radians);
	const double c = std::cos(radians);
	return { value.x * c + value.z * s, value.y, -value.x * s + value.z * c };
}

inline Vec3 RotateZ(Vec3 value, double radians) {
	const double s = std::sin(radians);
	const double c = std::cos(radians);
	return { value.x * c - value.y * s, value.x * s + value.y * c, value.z };
}

inline Vec3 InverseRotateEulerDegrees(Vec3 value, Vec3 rotationDegrees) {
	value = RotateZ(value, DegreesToRadians(-rotationDegrees.z));
	value = RotateY(value, DegreesToRadians(-rotationDegrees.y));
	value = RotateX(value, DegreesToRadians(-rotationDegrees.x));
	return value;
}

inline bool IsInsideStageBounds(const Vec3 &point, const StageBoundsInfo &bounds) {
	Vec3 local = {
		point.x - bounds.center.x,
		point.y - bounds.center.y,
		point.z - bounds.center.z,
	};
	local = InverseRotateEulerDegrees(local, bounds.rotationDegrees);

	return
		std::abs(local.x) <= bounds.halfExtents.x + kEpsilon &&
		std::abs(local.y) <= bounds.halfExtents.y + kEpsilon &&
		std::abs(local.z) <= bounds.halfExtents.z + kEpsilon;
}

inline bool IsInsideAnyStageBounds(const Vec3 &point, const std::vector<StageBoundsInfo> &boundsList) {
	for (const StageBoundsInfo &bounds : boundsList) {
		if (IsInsideStageBounds(point, bounds)) {
			return true;
		}
	}
	return false;
}

inline void OutputReportToDebug(const Report &report) {
	if (!report.HasMessages()) {
		return;
	}

	std::ostringstream stream;
	stream << "[StageValidation] " << report.source << "\n";

	for (const auto &error : report.errors) {
		stream << "  [ERROR] " << error << "\n";
	}

	for (const auto &warning : report.warnings) {
		stream << "  [WARNING] " << warning << "\n";
	}

	OutputDebugStringA(stream.str().c_str());
}

inline const Report &FinalizeReport(
	Report report,
	const std::vector<CheckItemResult> &exportedItems,
	const std::vector<LabelPositionInfo> &labelPositions) {

	DeduplicateStrings(report.errors);
	DeduplicateStrings(report.warnings);
	report.checkItems = BuildCheckItems(report.errors, report.warnings, exportedItems);
	report.markers = BuildMarkers(report.errors, report.warnings, labelPositions);
	lastReport = std::move(report);
	OutputReportToDebug(lastReport);
	return lastReport;
}

inline const Report &SetErrorReport(const std::string &source, const std::string &message) {
	Report report;
	report.source = source;
	report.revision = lastReport.revision + 1;
	report.errors.push_back(message);
	return FinalizeReport(std::move(report), std::vector<CheckItemResult>{}, std::vector<LabelPositionInfo>{});
}

inline const Report &ValidateSceneJson(const nlohmann::json &root, const std::string &source) {
	Report report;
	report.source = source;
	report.revision = lastReport.revision + 1;
	std::vector<CheckItemResult> exportedItems;
	std::vector<LabelPositionInfo> labelPositions;

	if (!root.is_object()) {
		report.errors.push_back("JSON のルートは object にしてください。");
		return FinalizeReport(std::move(report), exportedItems, labelPositions);
	}

	exportedItems = ReadExportedValidation(report, root);

	if (!root.contains("objects") || !root["objects"].is_array()) {
		report.errors.push_back("objects 配列がありません。");
		return FinalizeReport(std::move(report), exportedItems, labelPositions);
	}

	const auto &objects = root["objects"];
	if (objects.empty()) {
		report.errors.push_back("出力対象のオブジェクトがありません。");
	}

	std::vector<std::string> pathIds;
	if (root.contains("paths")) {
		if (!root["paths"].is_array()) {
			report.errors.push_back("paths は配列にしてください。");
		} else {
			const auto &paths = root["paths"];
			for (size_t index = 0; index < paths.size(); ++index) {
				const auto &pathData = paths[index];
				if (!pathData.is_object()) {
					report.errors.push_back("paths[" + ToString(index) + "] は object にしてください。");
					continue;
				}

				std::string pathId;
				std::string pathName;
				if (pathData.contains("name") && pathData["name"].is_string()) {
					pathName = pathData["name"].get<std::string>();
				}

				if (!pathData.contains("id") || !pathData["id"].is_string()) {
					report.errors.push_back("paths[" + ToString(index) + "]: id がありません。");
				} else {
					pathId = pathData["id"].get<std::string>();
					if (ContainsString(pathIds, pathId)) {
						report.errors.push_back("飛行パスIDが重複しています: " + pathId);
					} else {
						pathIds.push_back(pathId);
					}
				}

				if (!pathData.contains("points") || !pathData["points"].is_array()) {
					report.errors.push_back((pathId.empty() ? "paths[" + ToString(index) + "]" : pathId) + ": points 配列がありません。");
				} else {
					const auto &points = pathData["points"];
					if (points.size() < 2) {
						report.errors.push_back((pathId.empty() ? "paths[" + ToString(index) + "]" : pathId) + ": 飛行パスには点が2つ以上必要です。");
					}

					for (size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
						if (!IsNumberArray(points[pointIndex], 3)) {
							report.errors.push_back((pathId.empty() ? "paths[" + ToString(index) + "]" : pathId) + ": points[" + ToString(pointIndex) + "] は数値3つの配列にしてください。");
						}
					}

					if (!points.empty() && IsNumberArray(points[0], 3)) {
						const Vec3 markerPosition = ToGamePosition(ReadVec3(points[0]));
						if (!pathName.empty()) {
							labelPositions.push_back({ pathName, markerPosition });
						}
						if (!pathId.empty() && pathId != pathName) {
							labelPositions.push_back({ pathId, markerPosition });
						}
					}
				}

				if (pathData.contains("speed") && (!pathData["speed"].is_number() || pathData["speed"].get<double>() <= 0.0)) {
					report.errors.push_back((pathId.empty() ? "paths[" + ToString(index) + "]" : pathId) + ": speed は 0 より大きくしてください。");
				}

				if (pathData.contains("loop") && !pathData["loop"].is_boolean()) {
					report.errors.push_back((pathId.empty() ? "paths[" + ToString(index) + "]" : pathId) + ": loop は boolean にしてください。");
				}
			}
		}
	}

	size_t playerCount = 0;
	size_t enemyCount = 0;
	size_t stageBoundsCount = 0;
	std::vector<std::string> playerNames;
	std::vector<std::string> stageBoundsNames;
	std::vector<StageBoundsInfo> stageBoundsList;
	std::vector<SpawnPointInfo> spawnPoints;
	std::vector<std::pair<std::string, std::vector<std::string>>> namesByBase;

	for (size_t index = 0; index < objects.size(); ++index) {
		const auto &object = objects[index];
		if (!object.is_object()) {
			report.errors.push_back("objects[" + ToString(index) + "] は object にしてください。");
			continue;
		}

		const std::string name = GetObjectName(object, index);
		const std::string baseName = GetBaseName(name);
		const bool isStageBounds = StartsWith(baseName, "StageBounds");
		AddNameByBase(namesByBase, baseName, name);

		if (isStageBounds) {
			++stageBoundsCount;
			stageBoundsNames.push_back(name);
		}

		if (!object.contains("type") || !object["type"].is_string()) {
			report.errors.push_back(name + ": type がありません。");
		}

		if (!object.contains("transform") || !object["transform"].is_object()) {
			report.errors.push_back(name + ": transform がありません。");
			continue;
		}

		const auto &transform = object["transform"];
		const bool hasTranslation = transform.contains("translation") && IsNumberArray(transform["translation"], 3);
		const bool hasRotation = !transform.contains("rotation") || IsNumberArray(transform["rotation"], 3);
		const bool hasScale = transform.contains("scale") && IsNumberArray(transform["scale"], 3);

		if (!hasTranslation) {
			report.errors.push_back(name + ": transform.translation は数値3つの配列にしてください。");
		} else {
			labelPositions.push_back({ name, ToGamePosition(ReadVec3(transform["translation"])) });
		}
		if (!hasRotation) {
			report.errors.push_back(name + ": transform.rotation は数値3つの配列にしてください。");
		}
		if (!hasScale) {
			report.errors.push_back(name + ": transform.scale は数値3つの配列にしてください。");
		} else if (HasNearZeroAxis(transform["scale"])) {
			report.errors.push_back(name + ": scale に 0 に近い軸があります。");
		}

		std::string category;
		if (object.contains("category")) {
			if (!object["category"].is_string()) {
				report.errors.push_back(name + ": category は文字列にしてください。");
			} else {
				category = object["category"].get<std::string>();
			}
		}

		if (category.empty()) {
			if (object.contains("type") && object["type"].is_string() && object["type"].get<std::string>() == "MESH") {
				report.warnings.push_back(name + ": category がありません。ゲーム側では障害物として扱われる可能性があります。");
			}
		} else if (category == "PLAYER") {
			++playerCount;
			playerNames.push_back(name);
		} else if (category == "ENEMY") {
			++enemyCount;
			if (!object.contains("enemy") || !object["enemy"].is_object()) {
				report.errors.push_back(name + ": enemy_type が未設定です。");
			} else if (!object["enemy"].contains("type") || !object["enemy"]["type"].is_string() || IsUnsetString(object["enemy"]["type"].get<std::string>())) {
				report.errors.push_back(name + ": enemy_type が未設定です。");
			}

			if (object.contains("path_id")) {
				if (!object["path_id"].is_string()) {
					report.errors.push_back(name + ": path_id は文字列にしてください。");
				} else {
					std::string pathId = object["path_id"].get<std::string>();
					if (!ContainsString(pathIds, pathId)) {
						report.errors.push_back(name + ": 飛行パスIDが見つかりません: " + pathId);
					}
				}
			}
		} else if (category == "OBSTACLE") {
			if (object.contains("type") && object["type"].is_string() && object["type"].get<std::string>() != "MESH") {
				report.warnings.push_back(name + ": OBSTACLE は MESH で配置してください。");
			}

			if (!isStageBounds && !object.contains("collider")) {
				report.warnings.push_back(name + ": OBSTACLE にコライダーが設定されていません。");
			}
		} else {
			report.errors.push_back(name + ": category が未対応です: " + category);
		}

		if (isStageBounds && category != "OBSTACLE") {
			report.warnings.push_back(name + ": StageBounds は category を OBSTACLE にしてください。");
		}

		if (isStageBounds && hasTranslation && hasScale) {
			StageBoundsInfo bounds;
			bounds.name = name;
			bounds.center = ReadVec3(transform["translation"]);
			bounds.halfExtents = AbsVec3(ReadVec3(transform["scale"]));
			if (transform.contains("rotation") && hasRotation) {
				bounds.rotationDegrees = ReadVec3(transform["rotation"]);
			}
			stageBoundsList.push_back(bounds);
		}

		if ((category == "PLAYER" || category == "ENEMY") && hasTranslation) {
			SpawnPointInfo spawn;
			spawn.name = name;
			spawn.label = (category == "PLAYER") ? "自機スポーン" : "敵リスポーン地点";
			spawn.position = ReadVec3(transform["translation"]);
			spawnPoints.push_back(spawn);
		}

		if (object.contains("collider")) {
			const auto &collider = object["collider"];
			if (!collider.is_object()) {
				report.errors.push_back(name + ": collider は object にしてください。");
			} else {
				bool isBoxCollider = false;
				if (!collider.contains("type") || !collider["type"].is_string()) {
					report.errors.push_back(name + ": collider.type は文字列にしてください。");
				} else {
					const std::string colliderType = collider["type"].get<std::string>();
					isBoxCollider = IsValidColliderType(colliderType);
					if (!isBoxCollider) {
						report.errors.push_back(name + ": collider.type が未対応です: " + colliderType);
					}
				}

				if (isBoxCollider) {
					if (!collider.contains("size") || !IsNumberArray(collider["size"], 3)) {
						report.errors.push_back(name + ": Box collider の size は数値3つの配列にしてください。");
					} else {
						for (const auto &entry : collider["size"]) {
							if (entry.get<double>() <= 0.0) {
								report.errors.push_back(name + ": Box collider の size はすべて 0 より大きくしてください。");
								break;
							}
						}
					}
				}
			}
		}
	}

	if (playerCount == 0) {
		report.errors.push_back("PLAYER がありません。自機スポーンを1つ配置してください。");
	} else if (playerCount > 1) {
		std::ostringstream stream;
		stream << "PLAYER が複数あります: ";
		for (size_t index = 0; index < playerNames.size(); ++index) {
			if (index > 0) {
				stream << ", ";
			}
			stream << playerNames[index];
		}
		report.errors.push_back(stream.str());
	}

	if (enemyCount == 0) {
		report.warnings.push_back("敵リスポーン地点がありません。");
	}

	if (stageBoundsCount == 0) {
		report.warnings.push_back("StageBounds がありません。ステージ外への移動制限が効かない可能性があります。");
	} else if (stageBoundsCount > 1) {
		std::ostringstream stream;
		stream << "StageBounds が複数あります: ";
		for (size_t index = 0; index < stageBoundsNames.size(); ++index) {
			if (index > 0) {
				stream << ", ";
			}
			stream << stageBoundsNames[index];
		}
		report.warnings.push_back(stream.str());
	}

	if (!stageBoundsList.empty()) {
		for (const SpawnPointInfo &spawn : spawnPoints) {
			if (!IsInsideAnyStageBounds(spawn.position, stageBoundsList)) {
				report.errors.push_back(spawn.name + ": " + spawn.label + "がStageBoundsの外にあります。");
			}
		}
	}

	for (const auto &entry : namesByBase) {
		if (entry.second.size() <= 1) {
			continue;
		}

		std::ostringstream stream;
		stream << "名前のベースが重複しています: " << entry.first << " (";
		for (size_t index = 0; index < entry.second.size(); ++index) {
			if (index > 0) {
				stream << ", ";
			}
			stream << entry.second[index];
		}
		stream << ")";
		report.warnings.push_back(stream.str());
	}

	return FinalizeReport(std::move(report), exportedItems, labelPositions);
}

#else

inline const Report &ValidateSceneJson(const nlohmann::json &, const std::string &) {
	return lastReport;
}

inline const Report &SetErrorReport(const std::string &, const std::string &) {
	return lastReport;
}

#endif

inline const Report &GetLastReport() {
	return lastReport;
}

}
