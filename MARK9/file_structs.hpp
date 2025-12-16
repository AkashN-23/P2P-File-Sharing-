#ifndef FILE_STRUCTS_HPP
#define FILE_STRUCTS_HPP

#include <cstdint>

const int DATA_SIZE = 4096;

enum PacketType {
    START_TRANSFER = 1,
    FILE_DATA = 2,
    END_TRANSFER = 3,
    ACK = 4,
    PING_LAN = 99,   // New: Are you local?
    PONG_LAN = 100   // New: Yes, I am here!
};

struct FilePacket {
    uint32_t type;
    uint32_t seq_id;
    uint32_t data_len;
    char data[DATA_SIZE];
};

#endif