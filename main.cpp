#include <windows.h> // Работа с Windows API 
#include <evntrace.h> // Функции для работы с Event Tracing for Windows
#include <fstream>// Работа с файлами
#include <vector>
#include <iostream>
#include <sstream> // Работа со строковыми потоками
#include <iomanip> // Форматирование вывода
#include <tdh.h> // Функции для работы с данными событий (Trace Data Helper)

using namespace std;

// Получение пути до ETL файла
bool getETLFilePath(string& etlPath) {
    cout << "Введите путь до ETL файла: ";
    getline(cin, etlPath);
    return fs::exists(etlPath) && !fs::is_directory(etlPath);
}

// Структура для хранения данных о событии
struct EventData {
    ULONG EventID; // Идентификатор события
    GUID ProviderID; // GUID провайдера события
    ULONG ProcessID; // Идентификатор процесса
    ULONG ThreadID; // Идентификатор потока
    ULONGLONG Timestamp; // Временная метка
    ULONG KernelTime; // Время выполнения в ядре
    ULONG UserTime; // Время выполнения в пользовательском режиме
    UCHAR EventVersion; // Версия события
    UCHAR EventOpcode; // Код операции события
    UCHAR EventLevel;  // Уровень важности события
    ULONGLONG EventKeyword; // Ключевые слова события
    std::wstring TimeLocal; // Локализованное время в строковом формате
};

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

