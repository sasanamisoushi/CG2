#pragma once
#include <string>
class StringUtility {
public:
	//strindをwstingに変換
	std::wstring ConvertString(const std::string &str);

	//wstringをstringに変換
	std::string ConvertString(const std::wstring &str);
};
