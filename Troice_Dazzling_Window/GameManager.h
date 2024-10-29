#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <asio.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>

// 服务器配置
struct ServerConfig {
    static const char* SERVER_IP;
    static const unsigned short SERVER_PORT;
};

// 全局网络相关变量
struct NetworkManager {
    static asio::io_context io_context;
    static asio::ip::tcp::socket socket;
    static bool is_connected;

    static bool Connect();  // 连接服务器
    static void Disconnect();  // 断开连接
};

// 补丁文件信息结构
struct PatchFileInfo {
    std::string filename;
    size_t filesize;
    size_t crc;
};

// 全局服务器信息
struct ServerInfo {
    static std::string ip;
    static std::string port;
    static std::string name;
    static std::string notice;
    static bool isConnected;
};

// 函数声明
void check_for_updates(HWND hwnd);
void initialize_server_info(HWND hwnd);
void download_and_update();
void download_file(const PatchFileInfo& file_info);
void launch_game();
void check_and_start_game(HWND hwnd);
std::vector<PatchFileInfo> parse_update_info(const std::string& update_info);
