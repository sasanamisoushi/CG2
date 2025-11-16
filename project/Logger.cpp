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