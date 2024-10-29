#include "GameManager.h"
#include "Protocol.h"

// 定义 ServerConfig 的静态成员变量
const char* ServerConfig::SERVER_IP = "127.0.0.1";  // 示例 IP 地址
const unsigned short ServerConfig::SERVER_PORT = 12345;  // 示例端口号

// 定义全局变量
asio::io_context NetworkManager::io_context;
asio::ip::tcp::socket NetworkManager::socket(NetworkManager::io_context);
bool NetworkManager::is_connected = false;

// 定义 ServerInfo 的静态成员变量
std::string ServerInfo::ip;
std::string ServerInfo::port;
std::string ServerInfo::name;
std::string ServerInfo::notice;
bool ServerInfo::isConnected = false;

// 实现连接函数
bool NetworkManager::Connect() {
    if (is_connected && socket.is_open()) {
        return true;  // 已经连接
    }

    try {
        if (socket.is_open()) {
            socket.close();
        }

        asio::ip::tcp::endpoint endpoint(
            asio::ip::address::from_string(ServerConfig::SERVER_IP), 
            ServerConfig::SERVER_PORT
        );
        socket.connect(endpoint);
        is_connected = socket.is_open();
        return is_connected;
    }
    catch (const std::exception&) {
        is_connected = false;
        return false;
    }
}

void NetworkManager::Disconnect() {
    if (socket.is_open()) {
        try {
            socket.shutdown(asio::ip::tcp::socket::shutdown_both);
            socket.close();
        }
        catch (...) {}
    }
    is_connected = false;
}

void check_for_updates(HWND hwnd) {
    std::thread([hwnd]() {
        try {
            if (!NetworkManager::Connect()) {
                return;
            }

            std::string data_path = ".\\Data";
            if (!std::filesystem::exists(data_path)) {
                return;
            }

            std::vector<PatchFileInfo> patch_files;

            for (const auto& entry : std::filesystem::directory_iterator(data_path)) {
                std::string filename = entry.path().filename().string();
                
                if (filename.find("patch-") == 0 && filename.find(".mpq") != std::string::npos) {
                    if (filename.length() == 10 && filename[6] >= '1' && filename[6] <= '9') {
                        continue;
                    }

                    std::ifstream file(entry.path(), std::ios::binary);
                    if (!file) continue;

                    file.seekg(0, std::ios::end);
                    size_t filesize = file.tellg();
                    file.seekg(0, std::ios::beg);

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

                    patch_files.push_back({filename, filesize, crc});
                }
            }

            std::string request = "CHECK_PATCHES|\n";
            for (const auto& file : patch_files) {
                request += file.filename + "|" + std::to_string(file.crc) + "|\n";
            }
            request += "<END_OF_MESSAGE>";

            asio::write(NetworkManager::socket, asio::buffer(request));
        }
        catch (std::exception&) {
            NetworkManager::Disconnect();
        }
    }).detach();
}

void initialize_server_info(HWND hwnd) {
    std::thread([hwnd]() {
        try {
            if (!NetworkManager::Connect()) {
                return;
            }

            std::string request = Command::INIT_SERVER_INFO + "N/A" + "<END_OF_MESSAGE>";
            asio::write(NetworkManager::socket, asio::buffer(request));

            std::vector<char> buffer(1024 * 1024);
            std::string response_str;
            
            while (true)
            {
                try {
                    if (!NetworkManager::socket.is_open()) break;

                    size_t bytes_read = NetworkManager::socket.read_some(asio::buffer(buffer));
                    if (bytes_read > 0) {
                        response_str.append(buffer.data(), bytes_read);
                        
                        if (response_str.find("<END_OF_MESSAGE>") != std::string::npos) {
                            break;
                        }
                    }
                }
                catch (const asio::system_error&) {
                    break;
                }
            }

            size_t startPos = 0;
            if (response_str.length() >= 3 && 
                response_str[0] == (char)0xEF && 
                response_str[1] == (char)0xBB && 
                response_str[2] == (char)0xBF) {
                startPos = 3;
            }

            size_t endPos = response_str.find("<END_OF_MESSAGE>", startPos);
            if (endPos != std::string::npos) {
                std::string content = response_str.substr(startPos, endPos - startPos);
                
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

                    std::string::size_type pos = 0;
                    while ((pos = notice.find("\\n", pos)) != std::string::npos) {
                        notice.replace(pos, 2, "\n");
                        pos += 1;
                    }

                    ServerInfo::ip = ip;
                    ServerInfo::port = port;
                    ServerInfo::name = name;
                    ServerInfo::notice = notice;
                    ServerInfo::isConnected = true;
                }
            }
        }
        catch (std::exception&) {
            NetworkManager::Disconnect();
        }
    }).detach();
}

void download_and_update() {
    // 实现下载和更新逻辑
}

void download_file(const PatchFileInfo& file_info) {
    // 实现下载文件逻辑
}

void launch_game() {
    // 实现启动游戏逻辑
}

std::vector<PatchFileInfo> parse_update_info(const std::string& update_info) {
    // 实现解析更新信息逻辑
    return {};
}

void check_and_start_game(HWND hwnd) {
    std::thread([hwnd]() {
        try {
            if (!NetworkManager::Connect()) {
                return;
            }

            // 检查更新
            check_for_updates(hwnd);

            // 启动游戏
            launch_game();
        }
        catch (std::exception&) {
            NetworkManager::Disconnect();
        }
    }).detach();
}