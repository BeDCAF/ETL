#include <windows.h> // Работа с Windows API 
#include <evntrace.h> // Функции для работы с Event Tracing for Windows
#include <fstream>// Работа с файлами
#include <vector>
#include <iostream>
#include <sstream> // Работа со строковыми потоками
#include <iomanip> // Форматирование вывода
#include <tdh.h> // Функции для работы с данными событий (Trace Data Helper)


#pragma comment(lib, "advapi32.lib") //Для работы с ETW
#pragma comment(lib, "tdh.lib") // Trace Data Helper
#pragma comment(lib, "ole32.lib") // Для работы с GUID

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

// Класс для парсинга ETL-файлов
class ETLParser {
public:
    ETLParser() {}

    bool Parse(const std::wstring& filename) {
        filename_ = filename;
        EVENT_TRACE_LOGFILE logFile = { 0 };
        logFile.LogFileName = const_cast<LPWSTR>(filename_.c_str());
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        logFile.Context = this;

        TRACEHANDLE traceHandle = OpenTrace(&logFile);
        if (traceHandle == INVALID_PROCESSTRACE_HANDLE) {
            std::cerr << "OpenTrace failed: " << GetLastError() << std::endl;
            return false;
        }

        ULONG status = ProcessTrace(&traceHandle, 1, NULL, NULL);
        CloseTrace(traceHandle);
        return status == ERROR_SUCCESS;
    }

private:
    std::wstring filename_;
    std::vector<EventData> events_;
};

int main() {
    
    // UTF-8 в консоли
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    return 0;
}

