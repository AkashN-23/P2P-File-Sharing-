#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 5001
#define BUFFER_SIZE 4096

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./sender <Receiver_IP> <File_Path>" << std::endl;
        return -1;
    }

    const char* server_ip = argv[1];
    const char* filename = argv[2];
    
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // 1. Create Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    // 2. Connect to Server
    std::cout << "[*] Connecting to " << server_ip << "..." << std::endl;
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Is the receiver running?" << std::endl;
        return -1;
    }
    std::cout << "[+] Connected." << std::endl;

    // 3. Open File
    std::ifstream infile(filename, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "[-] Error opening file: " << filename << std::endl;
        close(sock);
        return -1;
    }

    // 4. Send File Data in Chunks
    std::cout << "[*] Sending file..." << std::endl;
    while (!infile.eof()) {
        infile.read(buffer, BUFFER_SIZE);
        std::streamsize bytes_read = infile.gcount();
        if (bytes_read > 0) {
            send(sock, buffer, bytes_read, 0);
        }
    }

    std::cout << "[+] File sent successfully." << std::endl;

    // 5. Clean up
    infile.close();
    close(sock);
    return 0;
}