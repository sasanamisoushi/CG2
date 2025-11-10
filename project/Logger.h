#pragma once
#include <string>


class Logger {
	//ログ出力
	void Log(const std::string &message);

	//ワイド文字
	void LogW(const std::wstring &message);
};

