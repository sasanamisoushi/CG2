#include "StringUtility.h"
#include <stringapiset.h>

std::wstring StringUtility::ConvertString(const std::string &str) {

	if (str.empty()) {
		return std::wstring();
	}

	int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
	if (sizeNeeded <= 0) {
		return std::wstring();
	}

	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string StringUtility::ConvertString(const std::wstring &str) {
    if (str.empty()) {
        return std::string();
    }

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) {
        return std::string();
    }

    std::string result(sizeNeeded - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}
