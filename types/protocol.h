#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstring>


// Network Configurations
constexpr int DEFAULT_PORT = 8080;
constexpr int MAX_RETRIES = 50;
constexpr int RETRY_INTERVAL_MS = 500;
constexpr int SLEEP_TIME = 2000; // in ms
constexpr int TIMEOUT_SEC = 1;


enum class MessageType : int {
    HELLO = 1,
    ACK = 2 // Aka Acknowledgment
};

struct Message {
    MessageType type;
    unsigned senderId; // between 1 and n
    unsigned sequence;

    Message() : type(MessageType::HELLO), senderId(0), sequence(0) {}

    Message(MessageType t, unsigned id, unsigned seq) : type(t), senderId(id), sequence(seq) {}

    bool isHello() const { return type == MessageType::HELLO; }
    bool isAck() const { return type == MessageType::ACK; }
} __attribute__((packed));


#endif