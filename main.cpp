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

// Вспомогательные функции

std::string EscapeForCSV(const std::string& data) {
    bool needQuotes = false;
    if (data.find(',') != std::string::npos || data.find('"') != std::string::npos || data.find('\n') != std::string::npos) {
        needQuotes = true;
    }

    std::string escaped = data;
    if (needQuotes) {
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\"\""); // Экранирование кавычек
            pos += 2;
        }
        return "\"" + escaped + "\"";
    }
    return data;
}

std::wstring ConvertFileTimeToLocalTime(const LARGE_INTEGER& ft) {
    FILETIME fileTime, localFileTime;
    fileTime.dwLowDateTime = ft.LowPart;
    fileTime.dwHighDateTime = ft.HighPart;
    if (!FileTimeToLocalFileTime(&fileTime, &localFileTime)) {
        return L"";
    }
    SYSTEMTIME sysTime;
    if (!FileTimeToSystemTime(&localFileTime, &sysTime)) {
        return L"";
    }
    wchar_t buffer[256];
    swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
    return std::wstring(buffer);
}

std::string WideToUTF8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::vector<std::wstring> FindEtlFiles(const std::wstring& directory) {
    std::vector<std::wstring> files;
    std::wstring searchPath = directory + L"\\*.etl";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return files;
    }
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(directory + L"\\" + findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
    return files;
}

std::string ProcessSystemDirectory(const std::wstring& directory, const std::string& format) {
    ETLParser parser;
    std::vector<std::wstring> etlFiles = FindEtlFiles(directory);
    for (const auto& file : etlFiles) {
        if (!parser.Parse(file)) {
            std::wcerr << L"Failed to parse: " << file << std::endl;
        }
    }
    if (format == "csv") {
        return parser.ToCSV();
    } else if (format == "json") {
        return parser.ToJSON();
    }
    return "";
}

std::wstring ConvertUtf8ToWide(const std::string& utf8) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], size_needed);
    return wstr;
}

// Класс для парсинга ETL-файлов
class ETLParser {
public:
    ETLParser() {}

    bool Parse(const std::wstring& filename) {
        filename_ = filename;
        EVENT_TRACE_LOGFILE logFile = { 0 };
        logFile.LogFileName = const_cast<LPWSTR>(filename_.c_str());
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        logFile.EventRecordCallback = EventCallback;
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

    std::string ToCSV() {
        std::stringstream ss;
        // Заголовки колонок
        ss << "EventID,ProviderID,ProcessID,ThreadID,Timestamp,KernelTime,UserTime,EventVersion,EventOpcode,EventLevel,EventKeyword,TimeLocal\n";

        for (const auto& event : events_) {
            wchar_t guidStr[40];
            StringFromGUID2(event.ProviderID, guidStr, 40);
            std::string providerUTF8 = WideToUTF8(guidStr);
            std::string timeLocalUTF8 = WideToUTF8(event.TimeLocal);

            // Экранирование строковых значений
            std::string providerEscaped = EscapeForCSV(providerUTF8);
            std::string timeLocalEscaped = EscapeForCSV(timeLocalUTF8);

            // Формируем строку с данными
            ss << event.EventID << ","                       // EventID
            << providerEscaped << ","                     // ProviderID
            << event.ProcessID << ","                      // ProcessID
            << event.ThreadID << ","                       // ThreadID
            << event.Timestamp << ","                      // Timestamp
            << event.KernelTime << ","                     // KernelTime
            << event.UserTime << ","                       // UserTime
            << static_cast<int>(event.EventVersion) << "," // EventVersion
            << static_cast<int>(event.EventOpcode) << ","  // EventOpcode
            << static_cast<int>(event.EventLevel) << ","   // EventLevel
            << event.EventKeyword << ","                     // EventKeyword
            << "\"" << timeLocalEscaped << "\"\n";          // TimeLocal

        }
        return ss.str();
    }

    std::string ToJSON() {
        std::stringstream ss;
        ss << "[\n";
        bool first = true;
        for (const auto& event : events_) {
            if (!first) {
                ss << ",\n";
            }
            first = false;
            wchar_t guidStr[40];
            StringFromGUID2(event.ProviderID, guidStr, 40);
            std::string providerUTF8 = WideToUTF8(guidStr);
            std::string timeLocalUTF8 = WideToUTF8(event.TimeLocal);
            ss << "  {\n"
                << "    \"EventID\": " << event.EventID << ",\n"
                << "    \"ProviderID\": \"" << providerUTF8 << "\",\n"
                << "    \"ProcessID\": " << event.ProcessID << ",\n"
                << "    \"ThreadID\": " << event.ThreadID << ",\n"
                << "    \"Timestamp\": " << event.Timestamp << ",\n"
                << "    \"KernelTime\": " << event.KernelTime << ",\n"
                << "    \"UserTime\": " << event.UserTime << ",\n"
                << "    \"EventVersion\": " << static_cast<int>(event.EventVersion) << ",\n"
                << "    \"EventOpcode\": " << static_cast<int>(event.EventOpcode) << ",\n"
                << "    \"EventLevel\": " << static_cast<int>(event.EventLevel) << ",\n"
                << "    \"EventKeyword\": " << event.EventKeyword << ",\n"
                << "    \"TimeLocal\": \"" << timeLocalUTF8 << "\"\n"
                << "  }";
        }
        ss << "\n]";
        return ss.str();
    }

private:
    static void WINAPI EventCallback(EVENT_RECORD* pEvent) {
        ETLParser* parser = static_cast<ETLParser*>(pEvent->UserContext);
        if (parser) {
            parser->HandleEvent(pEvent);
        }
    }

    void HandleEvent(PEVENT_RECORD pEvent) {
        EventData data;
        data.EventID = pEvent->EventHeader.EventDescriptor.Id;
        data.ProviderID = pEvent->EventHeader.ProviderId;
        data.ProcessID = pEvent->EventHeader.ProcessId;
        data.ThreadID = pEvent->EventHeader.ThreadId;
        data.Timestamp = pEvent->EventHeader.TimeStamp.QuadPart;
        data.KernelTime = pEvent->EventHeader.KernelTime;
        data.UserTime = pEvent->EventHeader.UserTime;
        data.EventVersion = pEvent->EventHeader.EventDescriptor.Version;
        data.EventOpcode = pEvent->EventHeader.EventDescriptor.Opcode;
        data.EventLevel = pEvent->EventHeader.EventDescriptor.Level;
        data.EventKeyword = pEvent->EventHeader.EventDescriptor.Keyword;
        data.TimeLocal = ConvertFileTimeToLocalTime(pEvent->EventHeader.TimeStamp);
        events_.push_back(data);
    }

    std::wstring filename_;
    std::vector<EventData> events_;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <directory> <format(csv/json)>" << std::endl;
        return 1;
    }
    std::wstring directory = ConvertUtf8ToWide(argv[1]);
    std::string format = argv[2];
    if (format != "csv" && format != "json") {
        std::cerr << "Invalid format. Use 'csv' or 'json'." << std::endl;
        return 1;
    }
    std::string output = ProcessSystemDirectory(directory, format);
    std::cout << output << std::endl;
    return 0;
}
