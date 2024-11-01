#pragma once
#include <string>
#include <cstdint>

// 包头大小
const uint32_t PACKET_HEADER_SIZE = 8;

// 消息类型
enum class MessageType : uint16_t {
    UNKNOWN = 0,
    GET_NOTICE = 1,        // 获取通知
    NOTICE_RESPONSE = 2,   // 通知响应
    GET_FILE = 3,         // 获取文件
    FILE_RESPONSE = 4,    // 文件响应
    ERROR_RESPONSE = 999   // 错误响应
};

// 包头结构
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t messageType;  // 消息类型
    uint32_t bodyLength;   // 消息体长度
    uint16_t version;      // 协议版本号

    PacketHeader() : messageType(0), bodyLength(0), version(1) {}
};
#pragma pack(pop)

