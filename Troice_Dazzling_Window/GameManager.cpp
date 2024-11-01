#include "GameManager.h"
#include <string>
#include <vector>
#include <asio.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>

// 定义 ServerInfo 的静态成员变量
std::string ServerInfo::ip;
std::string ServerInfo::port;
std::string ServerInfo::name;
std::string ServerInfo::notice;
bool ServerInfo::isConnected = false;

// 定义全局 io_context
asio::io_context global_io_context;

// 定义全局客户端指针
std::shared_ptr<Client> g_client;

void ConvertAndShowMessage(const std::string& cmdContent)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmdContent.c_str(), -1, NULL, 0);
    if (wlen > 0) {
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, cmdContent.c_str(), -1, &wstr[0], wlen);
        MessageBoxW(NULL, wstr.c_str(), L"客户端到消息", MB_OK);
    }
}

// 客户端类实现
Client::Client()
    : socket_(global_io_context), buffer_(1024) {}

void Client::start(const std::string& server_ip, const std::string& server_port) {
    asio::ip::tcp::resolver resolver(global_io_context);
    auto endpoints = resolver.resolve(server_ip, server_port);
    
    // 准备要发送的消息
    pending_message_ = "INIT_SERVER_INFO| N/A <END_OF_MESSAGE>";
    
    asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](const asio::error_code& error, const asio::ip::tcp::endpoint&) {
            if (!error) {
                // 先启动读取
                self->do_read();
                
                // 使用成员变量发送消息
                asio::async_write(
                    self->socket_,
                    asio::buffer(self->pending_message_),
                    [self](const asio::error_code& error, std::size_t /*bytes_transferred*/) {
                        if (error) {
                            // 处理发送错误
                        }
                    }
                );
            }
        });
}

void Client::send_request(const std::string& request) {
    // 将请求保存到成员变量中
    pending_message_ = request;
    
    // 使用成员变量发送数据
    asio::async_write(socket_, 
        asio::buffer(pending_message_),
        [self = shared_from_this()](const asio::error_code& error, std::size_t /*length*/) {
            if (error) {
                // 处理发送错误
            }
        });
}

void Client::do_read() {
    if (!stream_buffer_) {
        stream_buffer_ = std::make_shared<asio::streambuf>();    }
    
    asio::async_read_until(
        socket_,
        *stream_buffer_,
        "<END_OF_MESSAGE>",
        [self = shared_from_this()](const asio::error_code& error, std::size_t bytes_transferred) {

            self->handle_read(error, bytes_transferred);
        }
    );
}

void Client::handle_read(const asio::error_code& error, size_t bytes_transferred) {
    if (!error) {
        // 从 buffer 中提取所有数据
        std::string data{
            asio::buffers_begin(stream_buffer_->data()),
            asio::buffers_begin(stream_buffer_->data()) + bytes_transferred
        };
        stream_buffer_->consume(bytes_transferred);  // 清空缓冲区

        // 查找消息结束标记
        size_t endPos = data.find("<END_OF_MESSAGE>");
        if (endPos != std::string::npos) {
            // 提取有效消息内容
            std::string command = data.substr(0, endPos);
            
            //ConvertAndShowMessage(command);
            // 处理命令
            process_message(command);
        }

        // 继续读下一个消息
        do_read();
    }
}

// 解析消息，返回分割后的部分
std::vector<std::string> Client::parse_message(const std::string& message) {
    std::vector<std::string> parts;
    size_t pos = 0;
    size_t prev = 0;
    
    while ((pos = message.find('|', prev)) != std::string::npos) {
        parts.push_back(message.substr(prev, pos - prev));
        prev = pos + 1;
    }
    parts.push_back(message.substr(prev));
    
    return parts;
}

// 处理服务器信息
void Client::handle_server_info(const std::vector<std::string>& parts) {
    if (parts.size() >= 5 && parts[0] == "SERVER_INFO") {
        std::string ip = parts[1];
        std::string port = parts[2];
        std::string name = parts[3];
        std::string notice = parts[4];

        // 处理通知中的换行符
        std::string::size_type pos = 0;
        while ((pos = notice.find("\\n", pos)) != std::string::npos) {
            notice.replace(pos, 2, "\n");
            pos += 1;
        }

        // 更新服务器信息
        ServerInfo::ip = ip;
        ServerInfo::port = port;
        ServerInfo::name = name;
        ServerInfo::notice = notice;
        ServerInfo::isConnected = true;
    }
}

void Client::handle_delete_files(const std::vector<std::string>& files) {
    std::string data_path = ".\\Data\\";
    
    for (const auto& filename : files) {
        std::string full_path = data_path + filename;
        
        try {
            if (std::filesystem::exists(full_path)) {
                std::filesystem::remove(full_path);
            }
        }
        catch (const std::filesystem::filesystem_error& e) {

        }
    }
}

void Client::handle_update_files(const std::string& filename, const std::vector<char>& content) {
    std::string data_path = ".\\Data\\";
    std::string full_path = data_path + filename;
    
    try {

        ConvertAndShowMessage("文件内容大小: " + std::to_string(content.size()) + " 字节");

        // 确保目录存在
        if (!std::filesystem::exists(data_path)) {
            std::filesystem::create_directory(data_path);
            ConvertAndShowMessage("创建Data目录");
        }

        // 写入文件
        std::ofstream file(full_path, std::ios::binary | std::ios::trunc);
        if (file.is_open()) {
            file.write(content.data(), content.size());
            
            if (file.good()) {
                ConvertAndShowMessage("文件写入成功");
                
                // 验证文件大小
                file.flush();
                file.close();
                auto written_size = std::filesystem::file_size(full_path);
                ConvertAndShowMessage("已写入文件大小: " + std::to_string(written_size) + " 字节");
            } else {
                ConvertAndShowMessage("文件写入失败");
            }
        }
        else {
            ConvertAndShowMessage("无法打开文件进行写入: " + full_path);
        }
    }
    catch (const std::exception& e) {
        ConvertAndShowMessage("更新文件失败，错误: " + std::string(e.what()));
    }
}

void Client::process_message(const std::string& message) {
    if (message.find(Command::SERVER_INFO) == 0) {
        std::vector<std::string> parts = parse_message(message);
        handle_server_info(parts);
    } 
    else if (message.find(Command::CHECK_PATCHES) == 0) {
        // 处理补丁检查
    } 
    else if (message.find(Command::DELETE_FILES) == 0) {
        std::vector<std::string> parts = parse_message(message);
        parts.erase(parts.begin());
        handle_delete_files(parts);
    } 
    else if (message.find(Command::UPDATE_FILES) == 0) {
        ConvertAndShowMessage("收到更新文件命令");
        
        // 查找分隔符
        size_t first_sep = message.find('|');
        if (first_sep == std::string::npos) {
            ConvertAndShowMessage("未找到第一个分隔符");
            return;
        }

        size_t second_sep = message.find('|', first_sep + 1);
        if (second_sep == std::string::npos) {
            ConvertAndShowMessage("未找到第二个分隔符");
            return;
        }

        size_t third_sep = message.find('|', second_sep + 1);
        if (third_sep == std::string::npos) {
            ConvertAndShowMessage("未找到第三个分隔符");
            return;
        }

        // 提取文件名和大小
        std::string filename = message.substr(first_sep + 1, second_sep - first_sep - 1);
        std::streamsize filesize = std::stoll(message.substr(second_sep + 1, third_sep - second_sep - 1));
        
        ConvertAndShowMessage("文件名: " + filename);
        ConvertAndShowMessage("预期文件大小: " + std::to_string(filesize) + " 字节");

        // 查找内容边界标记
        std::string start_marker = "|<START_CONTENT>|";
        std::string end_marker = "|<END_CONTENT>|";
        
        size_t content_start = message.find(start_marker);
        size_t content_end = message.find(end_marker);
        
        if (content_start == std::string::npos) {
            ConvertAndShowMessage("未找到内容开始标记");
            return;
        }
        if (content_end == std::string::npos) {
            ConvertAndShowMessage("未找到内容结束标记");
            return;
        }

        // 计算实际内容的起始位置和大小（从开始标记的最后一个字符到结束标记的第一个字符）
        content_start += start_marker.length();
        size_t content_size = content_end - content_start;
        
        ConvertAndShowMessage("实际内容大小: " + std::to_string(content_size) + " 字节");

        // 验证文件大小
        if (content_size != static_cast<size_t>(filesize)) {
            ConvertAndShowMessage("文件大小不匹配！预期: " + std::to_string(filesize) + 
                                " 实际: " + std::to_string(content_size));
            return;
        }

        // 确保目录存在
        std::string data_path = ".\\Data";
        if (!std::filesystem::exists(data_path)) {
            std::filesystem::create_directory(data_path);
        }

        // 写入文件
        std::string full_path = data_path + "\\" + filename;
        std::ofstream outfile(full_path, std::ios::binary);
        if (!outfile) {
            ConvertAndShowMessage("无法打开文件进行写入: " + full_path);
            return;
        }

        // 写入文件内容
        outfile.write(message.data() + content_start, content_size);
        
        if (outfile.good()) {
            ConvertAndShowMessage("文件写入成功: " + full_path);
            
            // 验证写入的文件大小
            outfile.flush();
            outfile.close();
            auto written_size = std::filesystem::file_size(full_path);
            ConvertAndShowMessage("已写入文件大小: " + std::to_string(written_size) + " 字节");
        } else {
            ConvertAndShowMessage("文件写入失败");
            outfile.close();
        }
    }
}

// 初始化服务器信息
void initialize_server_info() {
    asio::io_context::work work(global_io_context); // 确保不退出

    g_client = std::make_shared<Client>();  // 初始化全局客户端
    g_client->start("127.0.0.1", "12345");

    std::thread t([]() {
        global_io_context.run();
    });

    t.detach();

}

void check_and_start_game(HWND hwnd) {
    // 检查 Data 目录
    std::string data_path = ".\\Data";
    if (!std::filesystem::exists(data_path)) {
        return;
    }

    // 存储补丁文件信息
    std::vector<PatchFileInfo> patch_files;

    // 遍历 Data 目录
    for (const auto& entry : std::filesystem::directory_iterator(data_path)) {
        std::string filename = entry.path().filename().string();
        
        // 检查是否是补丁文件
        if (filename.find("patch-") == 0 && filename.find(".mpq") != std::string::npos) {
            // 跳过 patch-1.mpq 到 patch-9.mpq
            if (filename.length() == 10 && filename[6] >= '1' && filename[6] <= '9') {
                continue;
            }

            // 打开文件
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) continue;

            // 获取文件大小
            file.seekg(0, std::ios::end);
            size_t filesize = file.tellg();
            file.seekg(0, std::ios::beg);

            // 计算 CRC
            const size_t buffer_size = 8192;
            std::vector<char> buffer(buffer_size);
            std::hash<std::string_view> hasher;
            size_t crc = 0;

            while (file) {
                file.read(buffer.data(), buffer_size);
                std::streamsize count = file.gcount();
                if (count > 0) {
                    crc ^= hasher(std::string_view(buffer.data(), count));
                }
            }
            file.close();

            // 添加到补丁文件列表
            patch_files.push_back({filename, filesize, crc});
        }
    }

    // 构造请求消息
    std::string request = "CHECK_PATCHES|\n";
    for (const auto& file : patch_files) {
        request += file.filename + "|" + std::to_string(file.crc) + "|\n";
    }
    request += "<END_OF_MESSAGE>";

    //ConvertAndShowMessage(request);

    // 使用全局客户端发送请求
    if (g_client) {
        g_client->send_request(request);
    }
}

// 其他函数实现...