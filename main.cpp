#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <windows.h>
#include <cstdlib>
#include "json.hpp" // Для будующей конвертации в json

using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;

// Получение пути до ETL файла
bool getETLFilePath(string& etlPath) {
    cout << "Введите путь до ETL файла: ";
    getline(cin, etlPath);
    return fs::exists(etlPath) && !fs::is_directory(etlPath);
}

int main() {
    
    // UTF-8 в консоли
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    string etlPath;
    if (!getETLFilePath(etlPath)) {
        cerr << "Ошибка: ETL-файл не найден или это директория." << endl;
        return 1;
    }

    return 0;
}

