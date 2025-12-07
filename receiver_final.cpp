#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

#define PORT 5001
#define BUFFER_SIZE 32768 // Matching the sender's speed
#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

unsigned char key[AES_KEY_SIZE]; 

void handle_error() {
    std::cerr << "[-] Decryption error" << std::endl;
    exit(1);
}

int main() {
    memcpy(key, "01234567890123456789012345678901", 32);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    unsigned char buffer[BUFFER_SIZE];
    unsigned char decrypted_buffer[BUFFER_SIZE + AES_BLOCK_SIZE]; 
    unsigned char iv[AES_BLOCK_SIZE]; 

    // Socket Setup
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { perror("Socket failed"); exit(1); }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); exit(1); }
    if (listen(server_fd, 3) < 0) { perror("Listen failed"); exit(1); }

    std::cout << "[*] Ready to receive..." << std::endl;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { perror("Accept failed"); exit(1); }

    // 1. READ METADATA
    // A. Read IV
    if (recv(new_socket, iv, AES_BLOCK_SIZE, 0) != AES_BLOCK_SIZE) { std::cerr << "[-] No IV"; return -1; }
    
    // B. Read Filename Length
    int filename_len;
    if (recv(new_socket, &filename_len, sizeof(int), 0) <= 0) { std::cerr << "[-] No Name Len"; return -1; }
    
    // C. Read Filename
    char filename[256] = {0};
    if (recv(new_socket, filename, filename_len, 0) <= 0) { std::cerr << "[-] No Filename"; return -1; }
    
    std::cout << "[+] Receiving file: " << filename << std::endl;

    // 2. DECRYPT & SAVE FILE
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handle_error();
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) handle_error();

    // Use the dynamic filename we received!
    std::ofstream outfile(filename, std::ios::binary);
    
    int len;
    int bytes_received;
    while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (1 != EVP_DecryptUpdate(ctx, decrypted_buffer, &len, buffer, bytes_received)) handle_error();
        outfile.write((char*)decrypted_buffer, len);
    }

    if (1 != EVP_DecryptFinal_ex(ctx, decrypted_buffer, &len)) handle_error();
    outfile.write((char*)decrypted_buffer, len);

    EVP_CIPHER_CTX_free(ctx);
    outfile.close();
    close(new_socket);
    close(server_fd);

    std::cout << "[+] Success! Saved as: " << filename << std::endl;
    return 0;
}