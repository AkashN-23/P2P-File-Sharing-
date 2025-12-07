#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 5001
#define BUFFER_SIZE 4096

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 1. Create Socket File Descriptor
    // AF_INET = IPv4, SOCK_STREAM = TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return -1;
    }

    // 2. Bind the socket to the port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on 0.0.0.0
    address.sin_port = htons(PORT);       // Host to Network Short (Big Endian)

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }

    // 3. Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        return -1;
    }

    std::cout << "[*] Listening on port " << PORT << "..." << std::endl;

    // 4. Accept a connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        return -1;
    }
    std::cout << "[+] Connection accepted." << std::endl;

    // 5. Receive File Logic
    // For this basic phase, we assume the stream is purely file data.
    // (In the next phase, we will handle filename metadata properly).
    std::ofstream outfile("received_file.bin", std::ios::binary);
    
    int valread;
    long total_bytes = 0;
    while ((valread = read(new_socket, buffer, BUFFER_SIZE)) > 0) {
        outfile.write(buffer, valread);
        total_bytes += valread;
    }

    std::cout << "[+] File received successfully (" << total_bytes << " bytes)." << std::endl;

    // 6. Close everything
    outfile.close();
    close(new_socket);
    close(server_fd);

    return 0;
}