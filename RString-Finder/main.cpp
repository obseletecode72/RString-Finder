#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <stack>
#include <thread>
#include <mutex>

bool containsStringIgnoreCase(const std::string& data, const std::string& target) {
    auto it = std::search(
        data.begin(), data.end(),
        target.begin(), target.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != data.end();
}

void searchRegistryIterative(HKEY hKeyRoot, const std::string& subKey, const std::string& target, std::mutex& outputMutex) {
    std::stack<std::pair<HKEY, std::string>> keysToExplore;
    keysToExplore.push({ hKeyRoot, subKey });

    while (!keysToExplore.empty()) {
        auto [parentKey, currentSubKey] = keysToExplore.top();
        keysToExplore.pop();

        HKEY hKey;
        if (RegOpenKeyExA(parentKey, currentSubKey.c_str(), 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS)
            continue;

        auto valueName = std::make_unique<char[]>(16383); // gosto de deixar o projeto bem cleanzin, entao aqui vai dinamico pro compilador nao gritar xddd
        DWORD valueNameSize;
        DWORD type;
        auto data = std::make_unique<BYTE[]>(16383);
        DWORD dataSize;

        for (DWORD i = 0;; ++i) {
            valueNameSize = 16383;
            dataSize = 16383;
            LONG result = RegEnumValueA(hKey, i, valueName.get(), &valueNameSize, nullptr, &type, data.get(), &dataSize);

            if (result == ERROR_NO_MORE_ITEMS)
                break;

            if (result == ERROR_SUCCESS) {
                bool found = false;
                if (containsStringIgnoreCase(valueName.get(), target))
                    found = true;

                if (type == REG_BINARY) {
                    std::string binaryData(reinterpret_cast<char*>(data.get()), dataSize);
                    if (containsStringIgnoreCase(binaryData, target))
                        found = true;
                }

                if (type == REG_SZ) {
                    std::string stringData(reinterpret_cast<char*>(data.get()), dataSize / sizeof(char));
                    if (containsStringIgnoreCase(stringData, target))
                        found = true;
                }

                if (found) {
                    std::lock_guard<std::mutex> guard(outputMutex);
                    std::cout << "String encontrada: " << currentSubKey << "\\" << valueName.get() << std::endl;
                    RegDeleteValueA(hKey, valueName.get());
                    std::cout << "Valor deletado: " << currentSubKey << "\\" << valueName.get() << std::endl;
                }
            }
        }

        auto subKeyName = std::make_unique<char[]>(16383);
        DWORD subKeyNameSize;
        for (DWORD i = 0;; ++i) {
            subKeyNameSize = 16383;
            LONG result = RegEnumKeyExA(hKey, i, subKeyName.get(), &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

            if (result == ERROR_NO_MORE_ITEMS)
                break;

            if (result == ERROR_SUCCESS)
                keysToExplore.push({ hKeyRoot, currentSubKey.empty() ? subKeyName.get() : currentSubKey + "\\" + subKeyName.get() });
        }

        RegCloseKey(hKey);
    }
}

void searchRegistryMultithreaded(HKEY hKeyRoot, const std::string& subKey, const std::string& target) {
    std::mutex outputMutex;
    std::vector<std::thread> threads;

    std::stack<std::pair<HKEY, std::string>> keysToExplore;
    keysToExplore.push({ hKeyRoot, subKey });

    while (!keysToExplore.empty()) {
        auto [parentKey, currentSubKey] = keysToExplore.top();
        keysToExplore.pop();

        threads.emplace_back([parentKey, currentSubKey, target, &outputMutex]() {
            searchRegistryIterative(parentKey, currentSubKey, target, outputMutex);
            });

        if (threads.size() >= std::thread::hardware_concurrency()) {
            for (auto& t : threads)
                t.join();

            threads.clear();
        }
    }

    for (auto& t : threads)
        t.join();
}

int main() {
    std::string target = "stringprocococcoco";
    std::mutex outputMutex;

    std::vector<std::thread> mainThreads;

    mainThreads.emplace_back([&]() {
        searchRegistryMultithreaded(HKEY_CLASSES_ROOT, "", target);
        });

    mainThreads.emplace_back([&]() {
        searchRegistryMultithreaded(HKEY_CURRENT_USER, "", target);
        });

    mainThreads.emplace_back([&]() {
        searchRegistryMultithreaded(HKEY_LOCAL_MACHINE, "", target);
        });

    mainThreads.emplace_back([&]() {
        searchRegistryMultithreaded(HKEY_USERS, "", target);
        });

    std::cout << "Procurando em todas chaves principais IMPORTANTES ta ok??..." << std::endl;

    for (auto& t : mainThreads)
        t.join();

    return 0;
}
