#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>

#define DISCOVERY_PORT 8888
#define BROADCAST_IP "255.255.255.255"

struct Peer {
    std::string ip;
    std::string hostname;
};

// --- LISTENER: Runs in background, replies to "P2P_DISCOVERY_REQ" ---
void run_discovery_listener(std::atomic<bool>& running) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    char hostname[256];
    gethostname(hostname, 256); // Get computer name (e.g., "fedora")

    // Set a timeout so the thread can eventually exit
    struct timeval tv;
    tv.tv_sec = 1; 
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while (running) {
        int len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender_addr, &sender_len);
        if (len > 0) {
            buffer[len] = '\0';
            if (strcmp(buffer, "P2P_DISCOVERY_REQ") == 0) {
                // Reply with "P2P_RESP:<hostname>"
                std::string reply = "P2P_RESP:";
                reply += hostname;
                sendto(sock, reply.c_str(), reply.length(), 0, (struct sockaddr*)&sender_addr, sender_len);
            }
        }
    }
    close(sock);
}

// --- SCANNER: Shouts "P2P_DISCOVERY_REQ" and collects replies ---
std::vector<Peer> scan_for_peers() {
    std::vector<Peer> peers;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    struct timeval tv;
    tv.tv_sec = 2; // Listen for 2 seconds
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    inet_pton(AF_INET, BROADCAST_IP, &broadcast_addr.sin_addr);

    // 1. Send Broadcast
    std::string msg = "P2P_DISCOVERY_REQ";
    sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    std::cout << "[*] Scanning room for peers..." << std::endl;

    // 2. Collect Replies
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    while (true) {
        int len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender_addr, &sender_len);
        if (len < 0) break; // Timeout

        buffer[len] = '\0';
        std::string response = buffer;
        if (response.find("P2P_RESP:") == 0) {
            std::string name = response.substr(9);
            std::string ip = inet_ntoa(sender_addr.sin_addr);
            peers.push_back({ip, name});
        }
    }
    close(sock);
    return peers;
}
#endif