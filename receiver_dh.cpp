#include "dh_helper.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 5001
#define BUFFER_SIZE 32768
#define AES_BLOCK_SIZE 16

int main() {
    // --- SETUP NETWORK ---
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    std::cout << "[*] Waiting for connection..." << std::endl;
    int sock = accept(server_fd, NULL, NULL);

    // --- DH KEY EXCHANGE START ---
    std::cout << "[*] Performing Key Handshake..." << std::endl;
    DiffieHellman dh;
    dh.init();
    dh.generate_keys();

    // 1. Receive Peer's Public Key
    int peer_len;
    recv(sock, &peer_len, sizeof(int), 0);
    std::vector<unsigned char> peer_pub(peer_len);
    recv(sock, peer_pub.data(), peer_len, 0);

    // 2. Send My Public Key
    std::vector<unsigned char> my_pub = dh.get_public_key();
    int pub_len = my_pub.size();
    send(sock, &pub_len, sizeof(int), 0);
    send(sock, my_pub.data(), pub_len, 0);

    // 3. Compute Shared Secret
    std::vector<unsigned char> shared_secret = dh.compute_shared_secret(peer_pub);
    unsigned char key[32];
    memcpy(key, shared_secret.data(), 32);
    std::cout << "[+] Handshake Complete. Secure Channel Established." << std::endl;
    // --- DH KEY EXCHANGE END ---


    // --- NORMAL RECEIVE LOGIC ---
    unsigned char iv[AES_BLOCK_SIZE];
    recv(sock, iv, AES_BLOCK_SIZE, 0);

    int filename_len;
    recv(sock, &filename_len, sizeof(int), 0);

    char filename[256] = {0};
    recv(sock, filename, filename_len, 0);
    std::cout << "[+] Receiving: " << filename << std::endl;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    std::ofstream outfile(filename, std::ios::binary);
    unsigned char buffer[BUFFER_SIZE];
    unsigned char decrypted[BUFFER_SIZE + AES_BLOCK_SIZE];
    int len, bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        EVP_DecryptUpdate(ctx, decrypted, &len, buffer, bytes_received);
        outfile.write((char*)decrypted, len);
    }
    EVP_DecryptFinal_ex(ctx, decrypted, &len);
    outfile.write((char*)decrypted, len);

    EVP_CIPHER_CTX_free(ctx);
    outfile.close();
    close(sock);
    close(server_fd);
    std::cout << "[+] File saved." << std::endl;
    return 0;
}