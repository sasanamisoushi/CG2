#include "Logger.h"
#include <iostream>
#include <fstream>


void Logger::Log(const std::string &message) {

	//コンソールに出力
	std::cout << message;

	//ファイルに出力
	static std::ofstream logFile("log.txt", std::ios::app);

	logFile << message;
	logFile.flush();
}
void LogW(const std::wstring &message) {

	//コンソールに出力
	std::wcout << message;
	//ファイルに出力
	static std::wofstream logFile("log_w.txt", std::ios::app);
	logFile << message;
	logFile.flush();
}

