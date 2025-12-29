#include "dh_helper.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/rand.h>
#include <chrono>           // For timing
#include <netinet/tcp.h>    // For Linux TCP metrics
#include <iomanip>          // For nice printing

#define PORT 5001
#define BUFFER_SIZE 32768
#define AES_BLOCK_SIZE 16

// --- METRIC HELPER FUNCTIONS ---

// 2. Get Packet Loss (Linux Specific)
void print_tcp_metrics(int sock) {
    struct tcp_info info;
    socklen_t len = sizeof(info);
    
    if (getsockopt(sock, SOL_TCP, TCP_INFO, &info, &len) == 0) {
        std::cout << "\n--- 📡 NETWORK HEALTH REPORT ---" << std::endl;
        std::cout << "Packet Loss/Retransmits: " << info.tcpi_total_retrans << " packets" << std::endl;
        std::cout << "Current RTT (Kernel):    " << info.tcpi_rtt / 1000.0 << " ms" << std::endl;
        std::cout << "Congestion Window:       " << info.tcpi_snd_cwnd << std::endl;
        std::cout << "--------------------------------" << std::endl;
    } else {
        perror("\n[-] Could not fetch TCP metrics");
    }
}

int main(int argc, char const *argv[]) {
    // 1. Check arguments
    std::string target_ip;
    int target_port = PORT;
    std::string filepath;

    if (argc < 3) {
        std::cerr << "Usage: ./sender_dh <IP> <File> [Optional: Port]" << std::endl;
        return -1;
    }
    
    target_ip = argv[1];
    filepath = argv[2];
    if (argc == 4) target_port = std::stoi(argv[3]); 

    // --- SETUP NETWORK ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target_port); 
    
    if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "[-] Invalid IP address." << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[-] Connection Failed" << std::endl;
        return -1;
    }

    // --- DH KEY EXCHANGE ---
    std::cout << "[*] Negotiating Encryption Keys..." << std::endl;
    DiffieHellman dh;
    dh.init();
    dh.generate_keys();

    std::vector<unsigned char> my_pub = dh.get_public_key();
    int pub_len = my_pub.size();
    send(sock, &pub_len, sizeof(int), 0);
    send(sock, my_pub.data(), pub_len, 0);

    int peer_len;
    recv(sock, &peer_len, sizeof(int), 0);
    std::vector<unsigned char> peer_pub(peer_len);
    recv(sock, peer_pub.data(), peer_len, 0);

    std::vector<unsigned char> shared_secret = dh.compute_shared_secret(peer_pub);
    unsigned char key[32];
    memcpy(key, shared_secret.data(), 32);
    std::cout << "[+] Keys Exchanged!" << std::endl;

    // --- FILE TRANSFER ---
    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    int filename_len = filename.length();

    unsigned char iv[AES_BLOCK_SIZE];
    RAND_bytes(iv, AES_BLOCK_SIZE);
    send(sock, iv, AES_BLOCK_SIZE, 0);
    send(sock, &filename_len, sizeof(int), 0);
    send(sock, filename.c_str(), filename_len, 0);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    std::ifstream infile(filepath, std::ios::binary);
    infile.seekg(0, std::ios::end);
    long filesize = infile.tellg();
    infile.seekg(0, std::ios::beg);

    unsigned char buffer[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE + AES_BLOCK_SIZE];
    int len;
    long total_sent = 0;

    // --- METRIC 2: SPEEDOMETER START ---
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "[*] Sending " << filename << " (" << filesize / 1024 << " KB)..." << std::endl;
    
    while (infile.read((char*)buffer, BUFFER_SIZE) || infile.gcount() > 0) {
        int bytes_read = infile.gcount();
        if (bytes_read == 0) break;
        EVP_EncryptUpdate(ctx, ciphertext, &len, buffer, bytes_read);
        send(sock, ciphertext, len, 0);
        total_sent += bytes_read;

        if (total_sent % (BUFFER_SIZE * 100) == 0) { 
             auto current_time = std::chrono::high_resolution_clock::now();
             std::chrono::duration<double> elapsed = current_time - start_time;
             double speed_mbps = (total_sent / (1024.0 * 1024.0)) / elapsed.count();
             
             std::cout << "\r[=>] Sent: " << (total_sent * 100 / filesize) << "% " 
                       << "| Speed: " << std::fixed << std::setprecision(2) << speed_mbps << " MB/s" << std::flush;
        }
    }
    
    EVP_EncryptFinal_ex(ctx, ciphertext, &len);
    send(sock, ciphertext, len, 0);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_duration = end_time - start_time;
    double avg_speed = (filesize / (1024.0 * 1024.0)) / total_duration.count();

    std::cout << "\n\n[+] Transfer Complete!" << std::endl;
    std::cout << "    Time:  " << total_duration.count() << " seconds" << std::endl;
    std::cout << "    Avg Speed: " << avg_speed << " MB/s" << std::endl;

    // --- METRIC 3: PRINT PACKET LOSS FROM KERNEL ---
    print_tcp_metrics(sock);

    EVP_CIPHER_CTX_free(ctx);
    infile.close();
    close(sock);
    return 0;
}