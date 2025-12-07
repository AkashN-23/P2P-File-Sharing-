#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

#define PORT 5001
#define BUFFER_SIZE 4096
#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

// FIX 1: Declare empty array
unsigned char key[AES_KEY_SIZE]; 

void handle_error() {
    std::cerr << "[-] Encryption/Decryption error" << std::endl;
    exit(1);
}

int main() {
    // FIX 2: Manually copy the key (Must match the Sender's key!)
    memcpy(key, "01234567890123456789012345678901", 32);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    unsigned char buffer[BUFFER_SIZE];
    unsigned char decrypted_buffer[BUFFER_SIZE + AES_BLOCK_SIZE]; 
    unsigned char iv[AES_BLOCK_SIZE]; 

    // 1. Setup Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[*] Waiting for secure connection..." << std::endl;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "[+] Connected secure channel." << std::endl;

    // 2. Receive the IV first
    int bytes_received = recv(new_socket, iv, AES_BLOCK_SIZE, 0);
    if (bytes_received != AES_BLOCK_SIZE) {
        std::cerr << "[-] Failed to receive IV!" << std::endl;
        return -1;
    }

    // 3. Initialize Decryption Context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handle_error();

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) handle_error();

    std::ofstream outfile("received_secure.txt", std::ios::binary);
    
    int len;

    // 4. Decrypt Loop
    while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (1 != EVP_DecryptUpdate(ctx, decrypted_buffer, &len, buffer, bytes_received)) handle_error();
        outfile.write((char*)decrypted_buffer, len);
    }

    // 5. Finalize
    if (1 != EVP_DecryptFinal_ex(ctx, decrypted_buffer, &len)) handle_error();
    outfile.write((char*)decrypted_buffer, len);

    EVP_CIPHER_CTX_free(ctx);
    outfile.close();
    close(new_socket);
    close(server_fd);

    std::cout << "[+] Secure file received and decrypted successfully." << std::endl;
    return 0;
}