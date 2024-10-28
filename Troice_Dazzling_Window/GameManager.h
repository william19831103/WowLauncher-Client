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
    const std::string INIT_SERVER_INFO = "INIT_SERVER_INFO";  // 合并的命令
    const std::string GET_FILE = "GET_FILE";
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
    void init_server_info(ServerInfoCallback server_callback);
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

    ServerInfoCallback server_info_callback_;
};

// 全局函数声明
void check_and_start_game(HWND hwnd);
void update_server_info(HWND hwnd);

// 全局服务器信息
struct ServerInfo {
    static std::string ip;
    static std::string port;
    static std::string name;
    static std::string notice;  // 添加通知字段
    static bool isConnected;
};
