#pragma once
#include <string>
class StringUtility {
public:
	//strindをwstingに変換
	static std::wstring ConvertString(const std::string &str);

	//wstringをstringに変換
	static std::string ConvertString(const std::wstring &str);
};
