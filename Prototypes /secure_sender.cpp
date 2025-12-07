#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define PORT 5001
#define BUFFER_SIZE 4096
#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

// FIX 1: Just declare the array size here. Do NOT assign the string yet.
unsigned char key[AES_KEY_SIZE]; 

void handle_error() {
    std::cerr << "[-] Encryption error" << std::endl;
    exit(1);
}

int main(int argc, char const *argv[]) {
    // FIX 2: Manually copy the 32-byte key into the array.
    // This avoids the "const char*" vs "unsigned char*" type error.
    memcpy(key, "01234567890123456789012345678901", 32);

    if (argc < 3) {
        std::cerr << "Usage: ./sender_secure <IP> <File>" << std::endl;
        return -1;
    }

    const char* server_ip = argv[1];
    const char* filename = argv[2];
    
    // 1. Generate a random IV
    unsigned char iv[AES_BLOCK_SIZE];
    if (!RAND_bytes(iv, AES_BLOCK_SIZE)) handle_error();

    // Setup Socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IP from text to binary
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        std::cerr << "[-] Invalid address or Address not supported" << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[-] Connection Failed. Is the receiver running?" << std::endl;
        return -1;
    }

    // 2. Send the IV first so the receiver knows how to decrypt
    send(sock, iv, AES_BLOCK_SIZE, 0);

    // 3. Initialize Encryption Context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handle_error();

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) handle_error();

    std::ifstream infile(filename, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "[-] Error opening file: " << filename << std::endl;
        return -1;
    }

    unsigned char buffer[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE + AES_BLOCK_SIZE]; // Ciphertext can be larger
    int len;

    // 4. Read File -> Encrypt -> Send
    std::cout << "[*] Encrypting and sending..." << std::endl;
    while (infile.read((char*)buffer, BUFFER_SIZE) || infile.gcount() > 0) {
        int bytes_read = infile.gcount();
        if (bytes_read == 0) break;

        // Encrypt this chunk
        if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, buffer, bytes_read)) handle_error();
        
        // Send encrypted data
        send(sock, ciphertext, len, 0);
    }

    // 5. Finalize Encryption (Handle padding for the last block)
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext, &len)) handle_error();
    send(sock, ciphertext, len, 0);

    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    infile.close();
    close(sock);

    std::cout << "[+] File sent securely." << std::endl;
    return 0;
}