#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════
//  .PG Launcher v2.0
// ═══════════════════════════════════════════════════════════════

const std::string COMPILER      = "g++";
const std::string INTERPRETER_SRC  = "dot.pg.cpp";
const std::string INTERPRETER_EXE  = "dot.pg.exe";

// ── Colors ─────────────────────────────────────────────────────
const std::string RESET   = "\033[0m";
const std::string BOLD    = "\033[1m";
const std::string RED     = "\033[1;31m";
const std::string GREEN   = "\033[1;32m";
const std::string YELLOW  = "\033[1;33m";
const std::string CYAN    = "\033[1;36m";
const std::string MAGENTA = "\033[1;35m";
const std::string DIM     = "\033[2m";

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printBanner() {
    std::cout << std::endl;
    std::cout << CYAN;
    std::cout << "  ╔═══════════════════════════════════════╗" << std::endl;
    std::cout << "  ║         .PG Language Launcher         ║" << std::endl;
    std::cout << "  ║              v2.0                     ║" << std::endl;
    std::cout << "  ╚═══════════════════════════════════════╝" << std::endl;
    std::cout << RESET << std::endl;
}

void printLine() {
    std::cout << DIM << "  ─────────────────────────────────────" << RESET << std::endl;
}

// ── Compilation ────────────────────────────────────────────────
bool compileInterpreter() {
    std::cout << YELLOW << "[.PG]" << RESET << " Compiling interpreter..." << std::endl;
    std::string cmd = COMPILER + " -std=c++17 -o \"" + INTERPRETER_EXE + "\" \"" + INTERPRETER_SRC + "\"";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << RED << "[.PG]" << RESET << " Compilation failed!" << std::endl;
        return false;
    }
    std::cout << GREEN << "[.PG]" << RESET << " Compilation successful." << std::endl;
    return true;
}

void ensureInterpreter() {
    if (!fs::exists(INTERPRETER_EXE)) {
        std::cout << YELLOW << "[.PG]" << RESET << " Interpreter not found. Compiling..." << std::endl;
        if (!compileInterpreter()) exit(1);
        std::cout << std::endl;
    } else if (fs::exists(INTERPRETER_SRC)) {
        auto srcTime = fs::last_write_time(INTERPRETER_SRC);
        auto exeTime = fs::last_write_time(INTERPRETER_EXE);
        if (srcTime > exeTime) {
            std::cout << YELLOW << "[.PG]" << RESET << " Source changed. Recompiling..." << std::endl;
            if (!compileInterpreter()) exit(1);
            std::cout << std::endl;
        }
    }
}

// ── Run script ─────────────────────────────────────────────────
int runScript(const std::string& scriptPath) {
    ensureInterpreter();

    fs::path absExe = fs::absolute(INTERPRETER_EXE);
    fs::path absScript = fs::absolute(scriptPath);

#ifdef _WIN32
    std::wstring cmdLine = L"\"" + absExe.wstring() + L"\" \"" + absScript.wstring() + L"\"";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!ok) {
        std::cerr << RED << "[.PG]" << RESET << " Failed to launch interpreter." << std::endl;
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
#else
    std::string cmd = "\"" + absExe.string() + "\" \"" + absScript.string() + "\"";
    return std::system(cmd.c_str());
#endif
}

// ── Find .pg files ────────────────────────────────────────────
std::vector<fs::path> findPgFiles(const fs::path& dir) {
    std::vector<fs::path> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) return files;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pg") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

// ── Find subdirectories ────────────────────────────────────────
std::vector<fs::path> findSubdirs(const fs::path& dir) {
    std::vector<fs::path> dirs;
    if (!fs::exists(dir) || !fs::is_directory(dir)) return dirs;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            dirs.push_back(entry.path());
        }
    }

    std::sort(dirs.begin(), dirs.end());
    return dirs;
}

// ── Display file list ──────────────────────────────────────────
void displayFiles(const std::vector<fs::path>& pgFiles, const fs::path& baseDir) {
    if (pgFiles.empty()) {
        std::cout << DIM << "  (no .pg scripts found)" << RESET << std::endl;
        return;
    }

    for (size_t i = 0; i < pgFiles.size(); i++) {
        std::string relative = fs::relative(pgFiles[i], baseDir).string();
        std::cout << "  " << GREEN << "[" << (i + 1) << "]" << RESET << " " << relative << std::endl;
    }
}

// ── Display subdirectories ─────────────────────────────────────
void displayDirs(const std::vector<fs::path>& dirs, const fs::path& baseDir, size_t offset) {
    for (size_t i = 0; i < dirs.size(); i++) {
        std::string relative = fs::relative(dirs[i], baseDir).string();
        std::cout << "  " << CYAN << "[" << (i + offset + 1) << "]" << RESET
                  << " " << relative << "/" << std::endl;
    }
}

// ── Interactive mode ───────────────────────────────────────────
int interactiveMode() {
    fs::path currentDir = fs::current_path();

    while (true) {
        clearScreen();
        printBanner();

        // Show current directory
        std::cout << "  " << BOLD << "Current folder:" << RESET << std::endl;
        std::cout << "  " << DIM << currentDir.string() << RESET << std::endl;
        printLine();

        // Find files and dirs
        auto pgFiles = findPgFiles(currentDir);
        auto subDirs = findSubdirs(currentDir);

        // Display .pg files
        if (!pgFiles.empty()) {
            std::cout << std::endl;
            std::cout << "  " << BOLD << "Scripts:" << RESET << std::endl;
            displayFiles(pgFiles, currentDir);
        }

        // Display subdirectories
        if (!subDirs.empty()) {
            std::cout << std::endl;
            std::cout << "  " << BOLD << "Folders:" << RESET << std::endl;
            displayDirs(subDirs, currentDir, pgFiles.size());
        }

        std::cout << std::endl;
        printLine();

        // Menu options
        std::cout << std::endl;
        std::cout << "  " << YELLOW << "[B]" << RESET << " Browse to a folder path" << std::endl;
        if (currentDir.has_parent_path()) {
            std::cout << "  " << YELLOW << "[U]" << RESET << " Go up one directory" << std::endl;
        }
        std::cout << "  " << YELLOW << "[R]" << RESET << " Refresh" << std::endl;
        std::cout << "  " << YELLOW << "[0]" << RESET << " Exit" << std::endl;
        std::cout << std::endl;

        // Prompt
        size_t maxChoice = pgFiles.size() + subDirs.size();
        std::cout << "  " << BOLD << "Select" << RESET;
        if (maxChoice > 0) {
            std::cout << " (1-" << maxChoice << ", B, U, R, 0)";
        } else {
            std::cout << " (B, U, R, 0)";
        }
        std::cout << ": ";

        std::string input;
        if (!std::getline(std::cin, input)) {
            clearScreen();
            return 0;
        }

        if (input.empty()) continue;

        // Handle non-numeric inputs
        std::string upper = input;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        if (upper == "0") {
            clearScreen();
            return 0;
        }

        if (upper == "B") {
            // Browse: type a folder path
            std::cout << std::endl;
            std::cout << "  Enter folder path (or full .pg file path): ";
            std::string path;
            if (!std::getline(std::cin, path)) continue;
            if (path.empty()) continue;

            // Check if it's a .pg file directly
            fs::path inputPath(path);
            if (fs::exists(inputPath) && inputPath.extension() == ".pg") {
                std::cout << std::endl;
                std::cout << "  Running: " << path << std::endl;
                std::cout << std::endl;
                runScript(path);
                std::cout << std::endl;
                std::cout << "  Press Enter to continue...";
                std::getline(std::cin, path);
                continue;
            }

            // Check if it's a directory
            if (fs::exists(inputPath) && fs::is_directory(inputPath)) {
                currentDir = inputPath;
                continue;
            }

            // Try relative to current
            fs::path relative = currentDir / path;
            if (fs::exists(relative) && fs::is_directory(relative)) {
                currentDir = relative;
                continue;
            }
            if (fs::exists(relative) && relative.extension() == ".pg") {
                std::cout << std::endl;
                std::cout << "  Running: " << relative.string() << std::endl;
                std::cout << std::endl;
                runScript(relative.string());
                std::cout << std::endl;
                std::cout << "  Press Enter to continue...";
                std::getline(std::cin, path);
                continue;
            }

            std::cout << RED << "[.PG]" << RESET << " Path not found: " << path << std::endl;
            std::cout << "  Press Enter to continue...";
            std::getline(std::cin, path);
            continue;
        }

        if (upper == "U" && currentDir.has_parent_path()) {
            currentDir = currentDir.parent_path();
            continue;
        }

        if (upper == "R") {
            continue;
        }

        // Numeric selection
        int choice = 0;
        try { choice = std::stoi(input); } catch (...) {
            std::cout << RED << "[.PG]" << RESET << " Invalid input." << std::endl;
            std::cout << "  Press Enter to continue...";
            std::getline(std::cin, input);
            continue;
        }

        if (choice < 1 || choice > (int)maxChoice) {
            std::cout << RED << "[.PG]" << RESET << " Invalid choice." << std::endl;
            std::cout << "  Press Enter to continue...";
            std::getline(std::cin, input);
            continue;
        }

        // File or directory?
        size_t fileCount = pgFiles.size();
        if (choice <= (int)fileCount) {
            // Run script
            fs::path selected = pgFiles[choice - 1];
            std::cout << std::endl;
            std::cout << "  Running: " << fs::relative(selected, currentDir).string() << std::endl;
            std::cout << std::endl;

            int ret = runScript(selected.string());
            if (ret != 0) {
                std::cerr << RED << "[.PG]" << RESET << " Script exited with code: " << ret << std::endl;
            }

            std::cout << std::endl;
            std::cout << "  Press Enter to continue...";
            std::getline(std::cin, input);
        } else {
            // Enter subdirectory
            size_t dirIdx = choice - fileCount - 1;
            if (dirIdx < subDirs.size()) {
                currentDir = subDirs[dirIdx];
            }
        }
    }

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    printBanner();

    // Direct mode: init.exe script.pg
    if (argc >= 2) {
        std::string script = argv[1];
        if (!fs::exists(script)) {
            std::cerr << RED << "[.PG]" << RESET << " File not found: " << script << std::endl;
            return 1;
        }
        return runScript(script);
    }

    return interactiveMode();
}
