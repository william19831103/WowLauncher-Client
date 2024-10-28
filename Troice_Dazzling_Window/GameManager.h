#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <asio.hpp>
#include <functional>

// 服务器配置
struct ServerConfig {
    static const char* SERVER_IP;
    static const unsigned short SERVER_PORT;
};

// 命令定义
namespace Command {
    const std::string GET_NOTICE = "GET_NOTICE";
    const std::string GET_FILE = "GET_FILE";
    const std::string GET_SERVER_INFO = "GET_SERVER_INFO";  // 添加新命令
}

// 补丁文件信息结构
struct PatchFileInfo {
    std::string filename;
    size_t filesize;
};

// 通知回调函数类型
using NoticeCallback = std::function<void(const std::string&)>;

// 修改服务器信息回调函数类型，添加端口参数
using ServerInfoCallback = std::function<void(const std::string& ip, const std::string& port, const std::string& name)>;

class GameUpdater {
public:
    GameUpdater(const std::string& server_ip, unsigned short server_port, HWND hwnd);
    void start_update_check();
    void get_notice(NoticeCallback callback);
    void get_server_info(ServerInfoCallback callback);  // 添加新方法
    HWND hwnd_;

private:
    void download_update();
    void download_file(const PatchFileInfo& file_info);
    void launch_game();
    std::vector<PatchFileInfo> parse_update_info(const std::string& update_info);

    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    std::string server_ip_;
    unsigned short server_port_;
    NoticeCallback notice_callback_;
    ServerInfoCallback server_info_callback_;  // 添加新的回调
};

// 全局函数声明
void check_and_start_game(HWND hwnd);
void update_server_notice(HWND hwnd, const std::string& notice);
void update_server_info(HWND hwnd);

// 添加全局服务器信息
struct ServerInfo {
    static std::string ip;
    static std::string port;
    static std::string name;
    static bool isConnected;
};
