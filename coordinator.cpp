#include "types/coordinator.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <chrono>
#include <thread>

UDPCoordinator::UDPCoordinator(const std::string& configFilePath)
    : socket_fd(-1), ready(false), shouldStop(false), messageSequence(0), configFilePath_(configFilePath) {
    if (!parseConfigFile(configFilePath)) {
        fprintf(stderr, "Error: Failed to parse host file (%s)\n", configFilePath.c_str());
        exit(EXIT_FAILURE);
    }

    int myId = getMyIdFromConfig();
    if (myId == -1) {
        fprintf(stderr, "Error: Could not find our ID in config file\n");
        exit(EXIT_FAILURE);
    }

    tracker = std::make_unique<PeerTracker>(myId, tracker->getTotalProcesses());

    std::ifstream file(configFilePath);
    std::string hostname;
    int id = 1;
    while (std::getline(file, hostname)) {
        tracker->addPeer(id++, hostname);
    }

    fprintf(stderr, "Info: Process %d initialized from hostfile with %d total processes\n", myId, tracker->getTotalProcesses());
}

bool UDPCoordinator::parseConfigFile(const std::string& configFilePath) {
    std::ifstream file(configFilePath);
    
    if (!file.is_open()) {
        fprintf(stderr, "Error: Could not open config file %s\n", configFilePath.c_str());
        return false;
    }

    std::string hostname;
    int count = 0;
    while (std::getline(file, hostname)) {
        if (!hostname.empty()) {
            count++;
        }
    }

    tracker = std::make_unique<PeerTracker>(0, count); // Temporarily set myId to 0, will update later
    return count > 0;
}

int UDPCoordinator::getMyIdFromConfig() {
    char hostname[256];

    // gethostname gets the name of the current machine (e.g. "Evans Laptop")
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return -1;
    }

    char* dot = strchr(hostname, '.');
    if (dot) *dot = '\0'; // Remove domain part if present

    std::ifstream file(configFilePath_);

    if (!file.is_open()) {
        fprintf(stderr, "Error: Could not open config file %s\n", configFilePath_);
        return -1;
    }

    std::string line;
    int id = 1;

    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        if (line == hostname) { 
            fprintf(stderr, "Found myself at line %d\n", id);
            return id;
        }

        if (!line.empty()) {
            id++;
        }
    }

    fprintf(stderr, "Error: Could not find hostname %s in config file\n", hostname);
    return -1; // Not found
}

bool UDPCoordinator::createSocket() {
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
 
    if (socket_fd == -1) {
        fprintf(stderr, "Error: Failed Socket Creator for %d", tracker->getMyId());
        return false;
    }

    // For socket timeout
    struct timeval tv;

    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0; 
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return true;
}

bool UDPCoordinator::bindSocket(unsigned port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // Bind the socket to a specific local address (IP and port)
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("failed to bind socket");
        fprintf(stderr, "Error: Failed to bind socket for %d", tracker->getMyId());
        return false;
    }

    fprintf(stderr, "Info: Socket bound to port %d for %d\n", port, tracker->getMyId());

    return true;
}

bool UDPCoordinator::resolvePeerAddresses() {
    auto& peers = tracker->getAllPeers();
    
    // Add retry logic for DNS resolution
    for (auto& peer : peers) {
        int retryCount = 0;
        const int maxRetries = 5;
        
        while (retryCount < maxRetries) {
            struct hostent* host = gethostbyname(peer.hostname.c_str());
            if (host != nullptr) {
                peer.addr.sin_family = AF_INET;
                peer.addr.sin_port = htons(DEFAULT_PORT);
                memcpy(&peer.addr.sin_addr, host->h_addr, host->h_length);
                peer.addrResolved = true;
                
                fprintf(stderr, "Process %d: Resolved %s to %s\n", 
                        tracker->getMyId(), peer.hostname.c_str(),
                        inet_ntoa(peer.addr.sin_addr));
                break;
            }
            
            retryCount++;
            if (retryCount < maxRetries) {
                fprintf(stderr, "Process %d: DNS resolution failed for %s, retry %d/%d\n",
                        tracker->getMyId(), peer.hostname.c_str(), 
                        retryCount, maxRetries);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        if (!peer.addrResolved) {
            fprintf(stderr, "ERROR: Failed to resolve %s after %d attempts\n", 
                    peer.hostname.c_str(), maxRetries);
            return false;
        }
    }
    return true;
}

void UDPCoordinator::sendMessage(const Peer& peer, const Message& msg) {
    size_t sent = sendto(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr*)&peer.addr, sizeof(peer.addr));
    if (sent < 0) { perror("sendto failed"); }
    else {
        fprintf(stderr, "Process %d: Sent %s to Process %d (seq: %d)\n",
            tracker->getMyId(),
            msg.type == MessageType::HELLO ? "HELLO" : "ACK",
            peer.id,
            msg.sequence);
    }
}

void UDPCoordinator::broadcastHello() {
    auto unacked = tracker->getUnackPeers();
    for (int peerId : unacked) {
        const Peer* peer = tracker->getPeer(peerId);
        if (peer && peer->addrResolved) {
            Message msg(MessageType::HELLO, tracker->getMyId(), messageSequence++);
            sendMessage(*peer, msg);
        }
    }
}

void UDPCoordinator::sendAck(unsigned peerId) {
    const Peer* peer = tracker->getPeer(peerId);
    if (peer && peer->addrResolved) {
        Message msg(MessageType::ACK, tracker->getMyId(), messageSequence++);
        sendMessage(*peer, msg);
    }
}

void UDPCoordinator::listenerThread() {
    fprintf(stderr, "Info: Listener thread started for %d\n", tracker->getMyId());

    while (!shouldStop) {
        Message msg;
        struct sockaddr_in senderAddr;
        socklen_t addr_len = sizeof(senderAddr);

        ssize_t received = recvfrom(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr*)&senderAddr, &addr_len);

        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recvfrom failed");
            }
            // Timeout or error, just continue
            continue;
        }

        if (received != sizeof(msg)) {
            fprintf(stderr, "Warning: Incomplete message received\n");
            continue;
        }

        fprintf(stderr, "Process %d: Received %s from Process %d (seq: %d)\n",
            tracker->getMyId(),
            msg.isHello() ? "HELLO" : "ACK",
            msg.senderId,
            msg.sequence);

        if (msg.isHello()) {
            tracker->recordHello(msg.senderId);
            sendAck(msg.senderId);
        }

        else if (msg.isAck()) {
            tracker->recordAck(msg.senderId);
        }


        // Check if coordinator is done
        if (tracker->allPeersReady() && !ready) {
            ready = true;
            fprintf(stderr, "READY\n");
        }
    }
}

void UDPCoordinator::senderThread() {
    fprintf(stderr, "Info: Sender thread started for %d\n", tracker->getMyId());

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));

    int attempts = 0;

    while (!ready && attempts < MAX_RETRIES) {
        broadcastHello();
        attempts++;

        if (attempts % 10 == 0) {
            tracker->printStatus();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
    }

    if (!ready) {
        fprintf(stderr, "ERROR: Process %d timeout waiting for peers\n", tracker->getMyId());
    }
}

bool UDPCoordinator::coordinate() {
    // Setup netowrk
    if (!createSocket() || !bindSocket(DEFAULT_PORT)) {
        return false;
    }

    if (!resolvePeerAddresses()) {
        return false;
    }

    std::thread listener(&UDPCoordinator::listenerThread, this);
    std::thread sender(&UDPCoordinator::senderThread, this);

    sender.join();

    shouldStop = true;

    listener.join();

    cleanup();
    return ready.load();
}

void UDPCoordinator::cleanup() {
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
}

UDPCoordinator::~UDPCoordinator() {
    cleanup();
}
