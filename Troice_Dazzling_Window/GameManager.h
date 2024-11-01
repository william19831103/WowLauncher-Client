#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <asio.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <memory>

// 命令定义
namespace Command {
    const std::string SERVER_INFO = "SERVER_INFO|";  // 服务器初始化信息
    const std::string CHECK_PATCHES = "CHECK_PATCHES|";        // 校验补丁
    const std::string DELETE_FILES = "DELETE_FILES|";          // 删除文件命令
    const std::string UPDATE_FILES = "UPDATE_FILES|";          // 更新文件命令
}

// 全局服务器信息
struct ServerInfo {
    static std::string ip;
    static std::string port;
    static std::string name;
    static std::string notice;
    static bool isConnected;
};

// 补丁文件信息结构
struct PatchFileInfo {
    std::string filename;
    size_t filesize;
    size_t crc;
};

// 声明全局 io_context
extern asio::io_context global_io_context;

// 前向声明
class Client;

// 添加全局客户端指针
extern std::shared_ptr<Client> g_client;

// 添加辅助函数声明
void ConvertAndShowMessage(const std::string& message);

// 客户端类
class Client : public std::enable_shared_from_this<Client> {
public:
    Client();
    void start(const std::string& server_ip, const std::string& server_port);
    void send_request(const std::string& request);

private:
    void do_read();
    void handle_read(const asio::error_code& error, size_t bytes_transferred);
    void process_message(const std::string& message);
    std::vector<std::string> parse_message(const std::string& message);
    void handle_server_info(const std::vector<std::string>& parts);
    void handle_delete_files(const std::vector<std::string>& files);
    void handle_update_files(const std::string& filename, const std::vector<char>& content);

    asio::ip::tcp::socket socket_;
    std::vector<char> buffer_;
    std::string accumulated_data_;
    std::shared_ptr<asio::streambuf> stream_buffer_;
    std::string pending_message_;
};

// 函数声明
void initialize_server_info();
void check_for_updates();
void download_and_update();
void download_file(const std::string& filename);
void launch_game();
void check_and_start_game(HWND hwnd);

