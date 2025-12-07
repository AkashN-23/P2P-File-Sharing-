#include "dh_helper.h" // Include our new helper class
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/rand.h>

#define PORT 5001
#define BUFFER_SIZE 32768
#define AES_BLOCK_SIZE 16

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./sender <IP> <File>" << std::endl;
        return -1;
    }

    // --- SETUP NETWORK ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[-] Connection Failed" << std::endl;
        return -1;
    }

    // --- DH KEY EXCHANGE START ---
    std::cout << "[*] Negotiating Encryption Keys..." << std::endl;
    DiffieHellman dh;
    dh.init();
    dh.generate_keys();

    // 1. Send My Public Key
    std::vector<unsigned char> my_pub = dh.get_public_key();
    int pub_len = my_pub.size();
    send(sock, &pub_len, sizeof(int), 0);
    send(sock, my_pub.data(), pub_len, 0);

    // 2. Receive Peer's Public Key
    int peer_len;
    recv(sock, &peer_len, sizeof(int), 0);
    std::vector<unsigned char> peer_pub(peer_len);
    recv(sock, peer_pub.data(), peer_len, 0);

    // 3. Compute Shared Secret
    std::vector<unsigned char> shared_secret = dh.compute_shared_secret(peer_pub);
    unsigned char key[32];
    memcpy(key, shared_secret.data(), 32);
    std::cout << "[+] Keys Exchanged! Shared Secret Generated." << std::endl;
    // --- DH KEY EXCHANGE END ---


    // --- NORMAL FILE TRANSFER (Using the generated key) ---
    // Prepare Filename
    std::string filepath = argv[2];
    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    int filename_len = filename.length();

    // Generate IV
    unsigned char iv[AES_BLOCK_SIZE];
    RAND_bytes(iv, AES_BLOCK_SIZE);

    // Send Header: IV + NameLen + Name
    send(sock, iv, AES_BLOCK_SIZE, 0);
    send(sock, &filename_len, sizeof(int), 0);
    send(sock, filename.c_str(), filename_len, 0);

    // Encrypt & Send
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    std::ifstream infile(filepath, std::ios::binary);
    unsigned char buffer[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE + AES_BLOCK_SIZE];
    int len;

    std::cout << "[*] Sending encrypted file: " << filename << std::endl;
    while (infile.read((char*)buffer, BUFFER_SIZE) || infile.gcount() > 0) {
        int bytes_read = infile.gcount();
        if (bytes_read == 0) break;
        EVP_EncryptUpdate(ctx, ciphertext, &len, buffer, bytes_read);
        send(sock, ciphertext, len, 0);
    }
    EVP_EncryptFinal_ex(ctx, ciphertext, &len);
    send(sock, ciphertext, len, 0);

    EVP_CIPHER_CTX_free(ctx);
    infile.close();
    close(sock);
    std::cout << "[+] Done." << std::endl;
    return 0;
}