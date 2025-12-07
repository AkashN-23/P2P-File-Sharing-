#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define PORT 5001
// OPTIMIZATION: Increased buffer to 32KB for faster large file transfers
#define BUFFER_SIZE 32768 
#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

unsigned char key[AES_KEY_SIZE]; 

void handle_error() {
    std::cerr << "[-] Encryption error" << std::endl;
    exit(1);
}

int main(int argc, char const *argv[]) {
    memcpy(key, "01234567890123456789012345678901", 32);

    if (argc < 3) {
        std::cerr << "Usage: ./sender <IP> <File>" << std::endl;
        return -1;
    }

    const char* server_ip = argv[1];
    const char* filepath = argv[2];

    // Extract just the filename from the path (e.g., "data/photo.jpg" -> "photo.jpg")
    std::string filename_str = filepath;
    size_t last_slash = filename_str.find_last_of("/");
    if (last_slash != std::string::npos) {
        filename_str = filename_str.substr(last_slash + 1);
    }
    const char* filename = filename_str.c_str();
    int filename_len = strlen(filename);

    // 1. Generate IV
    unsigned char iv[AES_BLOCK_SIZE];
    if (!RAND_bytes(iv, AES_BLOCK_SIZE)) handle_error();

    // 2. Connect
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[-] Connection Failed" << std::endl;
        return -1;
    }

    std::cout << "[*] Sending " << filename << " (" << filename_len << " bytes name)..." << std::endl;

    // 3. SEND METADATA (Protocol Handshake)
    // A. Send IV
    send(sock, iv, AES_BLOCK_SIZE, 0);
    // B. Send Filename Length (so receiver knows how much to read next)
    send(sock, &filename_len, sizeof(int), 0);
    // C. Send Filename
    send(sock, filename, filename_len, 0);

    // 4. ENCRYPT & SEND FILE CONTENT
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handle_error();
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) handle_error();

    std::ifstream infile(filepath, std::ios::binary);
    unsigned char buffer[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE + AES_BLOCK_SIZE]; 
    int len;

    while (infile.read((char*)buffer, BUFFER_SIZE) || infile.gcount() > 0) {
        int bytes_read = infile.gcount();
        if (bytes_read == 0) break;
        
        if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, buffer, bytes_read)) handle_error();
        send(sock, ciphertext, len, 0);
    }

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext, &len)) handle_error();
    send(sock, ciphertext, len, 0);

    EVP_CIPHER_CTX_free(ctx);
    infile.close();
    close(sock);

    std::cout << "[+] File sent successfully!" << std::endl;
    return 0;
}