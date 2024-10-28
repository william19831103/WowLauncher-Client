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


// 修改消息处理函
std::string ProcessMessage(const std::string& message) {
    // 跳过 BOM 标记（如果存在）
    size_t startPos = 0;
    if (message.length() >= 3 && 
        message[0] == (char)0xEF && 
        message[1] == (char)0xBB && 
        message[2] == (char)0xBF) {
        startPos = 3;
    }

    // 查找消息结束标记
    size_t endPos = message.find("<END_OF_MESSAGE>", startPos);
    if (endPos == std::string::npos) {
        return "";
    }
    
    std::string content = message.substr(startPos, endPos - startPos);
    
    // 分割类型和内容
    size_t separatorPos = content.find("|");
    if (separatorPos == std::string::npos) {
        return "";
    }
    
    std::string type = content.substr(0, separatorPos);
    std::string messageContent = content.substr(separatorPos + 1);
    
    // 还原换行符
    std::string::size_type pos = 0;
    while ((pos = messageContent.find("\\n", pos)) != std::string::npos) {
        messageContent.replace(pos, 2, "\n");
        pos += 1;
    }
    
    return messageContent;
}

void GameUpdater::get_notice(NoticeCallback callback) {
    notice_callback_ = callback;
    
    std::thread([this]() {
        try {
            if (!socket_.is_open()) {
                asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(server_ip_), server_port_);
                socket_.connect(endpoint);
            }

            // 发送获取通知命令
            std::string request = Command::GET_NOTICE + "\n";
            asio::write(socket_, asio::buffer(request));

            // 读取响应
            std::vector<char> buffer(1024 * 1024); // 1MB buffer
            std::string response_str;
            
            while (true) {
                try {
                    size_t bytes_read = socket_.read_some(asio::buffer(buffer));
                    if (bytes_read > 0) {
                        response_str.append(buffer.data(), bytes_read);
                        
                        if (response_str.find("<END_OF_MESSAGE>") != std::string::npos) {
                            break;
                        }
                    }
                }
                catch (const asio::error_code& e) {
                    MessageBoxW(NULL, L"读取数据失败", L"错误", MB_OK);
                    break;
                }
            }

            // 处理消息
            std::string processed_content = ProcessMessage(response_str);
            if (!processed_content.empty()) {
                if (notice_callback_) {
                    char* notice_copy = _strdup(processed_content.c_str());
                    PostMessage(hwnd_, WM_USER + 1, 0, (LPARAM)notice_copy);
                }
            }

            // 关闭连接
            if (socket_.is_open()) {
                socket_.close();
            }
        }
        catch (std::exception& e) {
            if (socket_.is_open()) {
                socket_.close();
            }
            MessageBoxW(NULL, L"获取通知失败", L"错误", MB_OK | MB_ICONERROR);
        }
    }).detach();
}

void update_server_notice(HWND hwnd, const std::string& notice) {
    GameUpdater* updater = new GameUpdater(ServerConfig::SERVER_IP, ServerConfig::SERVER_PORT, hwnd);
    updater->get_notice([updater](const std::string& server_notice) {
        // 使用 updater 中保存的窗口句柄
        char* notice_copy = _strdup(server_notice.c_str());
        PostMessage(updater->hwnd_, WM_USER + 1, 0, (LPARAM)notice_copy);
        delete updater;
    });
}

void GameUpdater::get_server_info(ServerInfoCallback callback) {
    server_info_callback_ = callback;
    
    std::thread([this]() {
        try {
            if (!socket_.is_open()) {
                asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(server_ip_), server_port_);
                socket_.connect(endpoint);
            }

            // 发送获取服务器信息命令
            std::string request = Command::GET_SERVER_INFO + "\n";
            //MessageBoxW(NULL, L"发送GET_SERVER_INFO命令", L"调试", MB_OK);
            asio::write(socket_, asio::buffer(request));

            // 读取响应
            std::vector<char> buffer(1024);
            std::string response_str;
            
            while (true) {
                try {
                    if (!socket_.is_open()) {
                        MessageBoxW(NULL, L"Socket已关闭", L"错误", MB_OK);
                        break;
                    }

                    size_t bytes_read = socket_.read_some(asio::buffer(buffer));
                    if (bytes_read > 0) {
                        response_str.append(buffer.data(), bytes_read);
                        
                        if (response_str.find("<END_OF_MESSAGE>") != std::string::npos) {
                            break;
                        }
                    }
                }
                catch (const asio::system_error& e) {
                    MessageBoxW(NULL, L"读取数据时发生错误", L"错误", MB_OK);
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
                
                // 解析 "SERVER_INFO|IP|PORT|NAME" 格式
                size_t firstSep = content.find('|');
                if (firstSep != std::string::npos) {
                    size_t secondSep = content.find('|', firstSep + 1);
                    if (secondSep != std::string::npos) {
                        size_t thirdSep = content.find('|', secondSep + 1);
                        if (thirdSep != std::string::npos) {
                            std::string type = content.substr(0, firstSep);
                            std::string ip = content.substr(firstSep + 1, secondSep - firstSep - 1);
                            std::string port = content.substr(secondSep + 1, thirdSep - secondSep - 1);
                            std::string name = content.substr(thirdSep + 1);

                            // 转换服务器名称为Unicode
                            int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, NULL, 0);
                            if (wlen > 0) {
                                std::vector<wchar_t> wstr(wlen);
                                if (MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wstr.data(), wlen) > 0) {
                                    // 显示转换后的数据
                                    //std::wstring parse_result = L"服务器信息:\n"
                                    //    L"IP: " + std::wstring(ip.begin(), ip.end()) + L"\n"
                                    //    L"端口: " + std::wstring(port.begin(), port.end()) + L"\n"
                                    //    L"名称: " + std::wstring(wstr.data());
                                    //MessageBoxW(NULL, parse_result.c_str(), L"服务器信息", MB_OK);

                                    if (type == "SERVER_INFO" && server_info_callback_) {
                                        // 将Unicode转回UTF-8
                                        int utf8len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
                                        if (utf8len > 0) {
                                            std::vector<char> utf8str(utf8len);
                                            if (WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, utf8str.data(), utf8len, NULL, NULL) > 0) {
                                                // 更新全局服务器信息
                                                ServerInfo::ip = ip;
                                                ServerInfo::port = port;
                                                ServerInfo::name = std::string(utf8str.data());
                                                ServerInfo::isConnected = true;
                                                
                                                // 调用回调
                                                server_info_callback_(ip, port, ServerInfo::name);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            try {
                if (socket_.is_open()) {
                    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
                    socket_.close();
                }
            }
            catch (const std::exception& e) {
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
            std::wstring error_msg = L"获取服务器信息错误: " + std::wstring(e.what(), e.what() + strlen(e.what()));
            MessageBoxW(NULL, error_msg.c_str(), L"错误", MB_OK | MB_ICONERROR);
        }
    }).detach();
}

void update_server_info(HWND hwnd) {
    GameUpdater* updater = new GameUpdater(ServerConfig::SERVER_IP, ServerConfig::SERVER_PORT, hwnd);
    updater->get_server_info([updater](const std::string& ip, const std::string& port, const std::string& name) {
        // 显示完整的服务器信息
        //std::wstring info = L"服务器IP: " + std::wstring(ip.begin(), ip.end()) + 
        //                   L"\n端口: " + std::wstring(port.begin(), port.end()) +
        //                   L"\n服务器名称: " + std::wstring(name.begin(), name.end());
        //MessageBoxW(NULL, info.c_str(), L"服务器信息", MB_OK);
        delete updater;
    });
}











