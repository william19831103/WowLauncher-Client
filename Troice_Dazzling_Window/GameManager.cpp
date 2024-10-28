#include "GameManager.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;  // 添加命名空间别名

// 定义服务器配置
const char* ServerConfig::SERVER_IP = "127.0.0.1";
const unsigned short ServerConfig::SERVER_PORT = 12345;

// 定义全局服务器信息变量
std::string ServerInfo::ip;
std::string ServerInfo::port;
std::string ServerInfo::name;
std::string ServerInfo::notice = "正在获取服务器通知...";  // 添加通知变量定义并设置初始值
bool ServerInfo::isConnected = false;

GameUpdater::GameUpdater(const std::string& server_ip, unsigned short server_port, HWND hwnd)
    : socket_(io_context_)
    , server_ip_(server_ip)
    , server_port_(server_port)
    , hwnd_(hwnd)
{}

std::vector<PatchFileInfo> GameUpdater::parse_update_info(const std::string& update_info) {
    std::vector<PatchFileInfo> files;
    std::istringstream iss(update_info);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        size_t delimiter_pos = line.find('|');
        if (delimiter_pos != std::string::npos) {
            PatchFileInfo file;
            file.filename = line.substr(0, delimiter_pos);
            file.filesize = std::stoull(line.substr(delimiter_pos + 1));
            files.push_back(file);
        }
    }
    
    return files;
}

void GameUpdater::start_update_check() {
    try {
        asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(server_ip_), server_port_);
        socket_.connect(endpoint);

        // 发送检查更新请求
        std::string request = "CHECK_UPDATE\n";
        asio::write(socket_, asio::buffer(request));

        // 读取响应
        asio::streambuf response;
        asio::read_until(socket_, response, "\n\n");  // 读取到空行为止

        std::string response_str((std::istreambuf_iterator<char>(&response)), 
                               std::istreambuf_iterator<char>());

        if (response_str.find("UPDATE_NEEDED") != std::string::npos) {
            download_update();
        } else {
            launch_game();
        }
    }
    catch (std::exception& e) {
        std::wstring error_msg = L"连接错误: " + std::wstring(e.what(), e.what() + strlen(e.what()));
        MessageBoxW(NULL, error_msg.c_str(), L"错误", MB_OK | MB_ICONERROR);
    }
}

void GameUpdater::download_update() {
    try {
        // 读取文件列表
        asio::streambuf list_buf;
        asio::read_until(socket_, list_buf, "\n\n");
        std::string list_str((std::istreambuf_iterator<char>(&list_buf)), 
                           std::istreambuf_iterator<char>());
        
        // 解析文件列表
        std::vector<PatchFileInfo> files = parse_update_info(list_str);
        
        // 下载每个文件
        for (const auto& file : files) {
            download_file(file);
        }

        launch_game();
    }
    catch (std::exception& e) {
        MessageBoxW(NULL, L"下载错误", L"", MB_OK | MB_ICONERROR);
    }
}

void GameUpdater::download_file(const PatchFileInfo& file_info) {
    try {
        // 创建或覆盖文件
        std::ofstream file(file_info.filename, std::ios::binary | std::ios::trunc);
        std::vector<char> buffer(8192);
        size_t total_received = 0;

        // 发送准备接收文件的信号
        std::string ready_signal = "READY_TO_RECEIVE\n";
        asio::write(socket_, asio::buffer(ready_signal));

        while (total_received < file_info.filesize) {
            size_t to_read = (buffer.size() < (file_info.filesize - total_received)) ? 
                            buffer.size() : (file_info.filesize - total_received);
            
            size_t n = socket_.read_some(asio::buffer(buffer, to_read));
            file.write(buffer.data(), n);
            total_received += n;

            // 更新进度
            float progress = (float)total_received / file_info.filesize * 100;
            // TODO: 示下载进度
        }

        file.close();
    }
    catch (std::exception& e) {
        throw std::runtime_error("下载文件 " + file_info.filename + " 失败: " + e.what());
    }
}

void GameUpdater::launch_game() {
    if (GetFileAttributesA("wow.exe") == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"找不到游戏程序", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA("wow.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        ShowWindow(hwnd_, SW_MINIMIZE);
    } else {
        MessageBoxW(NULL, L"启动游戏失败", L"错误", MB_OK | MB_ICONERROR);
    }
}

// 全局函数实现
void check_and_start_game(HWND hwnd) {
    // 使用配置中的服务器信息
    GameUpdater* updater = new GameUpdater(ServerConfig::SERVER_IP, ServerConfig::SERVER_PORT, hwnd);
    
    std::thread([updater]() {
        try {
            updater->start_update_check();
            delete updater;
        }
        catch (std::exception& e) {
            MessageBoxW(NULL, L"错误", L"", MB_OK | MB_ICONERROR);
            delete updater;
        }
    }).detach();
}

// 添加新的处理函数
void GameUpdater::init_server_info(ServerInfoCallback server_callback) {
    server_info_callback_ = server_callback;
    
    std::thread([this]() {
        try {
            if (!socket_.is_open()) {
                asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(server_ip_), server_port_);
                socket_.connect(endpoint);
            }

            // 发送初始化命令
            std::string request = Command::INIT_SERVER_INFO + "\n";
            asio::write(socket_, asio::buffer(request));

            // 读取响应
            std::vector<char> buffer(1024 * 1024);
            std::string response_str;
            
            while (true) {
                try {
                    if (!socket_.is_open()) break;

                    size_t bytes_read = socket_.read_some(asio::buffer(buffer));
                    if (bytes_read > 0) {
                        response_str.append(buffer.data(), bytes_read);
                        
                        if (response_str.find("<END_OF_MESSAGE>") != std::string::npos) {
                            break;
                        }
                    }
                }
                catch (const asio::system_error& e) {
                    break;
                }
            }

            // 处理响应
            size_t startPos = 0;
            if (response_str.length() >= 3 && 
                response_str[0] == (char)0xEF && 
                response_str[1] == (char)0xBB && 
                response_str[2] == (char)0xBF) {
                startPos = 3;  // 跳过 BOM
            }

            size_t endPos = response_str.find("<END_OF_MESSAGE>", startPos);
            if (endPos != std::string::npos) {
                std::string content = response_str.substr(startPos, endPos - startPos);
                
                // 解析 "SERVER_INFO|IP|PORT|NAME|NOTICE" 格式
                std::vector<std::string> parts;
                size_t pos = 0;
                size_t prev = 0;
                while ((pos = content.find('|', prev)) != std::string::npos) {
                    parts.push_back(content.substr(prev, pos - prev));
                    prev = pos + 1;
                }
                parts.push_back(content.substr(prev));

                if (parts.size() >= 5 && parts[0] == "SERVER_INFO") {
                    std::string ip = parts[1];
                    std::string port = parts[2];
                    std::string name = parts[3];
                    std::string notice = parts[4];

                    // 还原通知中的换行符
                    std::string::size_type pos = 0;
                    while ((pos = notice.find("\\n", pos)) != std::string::npos) {
                        notice.replace(pos, 2, "\n");
                        pos += 1;
                    }

                    // 更新全局服务器信息
                    ServerInfo::ip = ip;
                    ServerInfo::port = port;
                    ServerInfo::name = name;  // 保持 UTF-8 编码
                    ServerInfo::notice = notice;  // 直接更新通知内容
                    ServerInfo::isConnected = true;

                    // 调用服务器信息回调
                    if (server_info_callback_) {
                        server_info_callback_(ip, port, name);
                    }
                }
            }

            try {
                if (socket_.is_open()) {
                    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
                    socket_.close();
                }
            }
            catch (...) {
                // 忽略关闭时的错误
            }
        }
        catch (std::exception& e) {
            try {
                if (socket_.is_open()) {
                    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
                    socket_.close();
                }
            }
            catch (...) {
                // 忽略关闭时的错误
            }
            MessageBoxW(NULL, L"初始化服务器信息失败", L"错误", MB_OK | MB_ICONERROR);
        }
    }).detach();
}

// 修改 update_server_info 函数
void update_server_info(HWND hwnd) 
{
    GameUpdater* updater = new GameUpdater(ServerConfig::SERVER_IP, ServerConfig::SERVER_PORT, hwnd);
    
    updater->init_server_info(
        // 服务器信息回调
        [updater](const std::string& ip, const std::string& port, const std::string& name) {
            delete updater;
        }
    );
}