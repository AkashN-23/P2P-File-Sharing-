// mark3.cpp - The Ultimate P2P Tool (Web + Secure C++)
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <atomic>
#include <map> // NEW: For Address Book
#include <algorithm> // For std::transform

// --- LIBRARIES ---
#include "httplib.h"       // For Web Server
#include "dh_helper.h"     // For Secure Key Exchange
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <arpa/inet.h>
#include <unistd.h>

// --- CONFIGURATION ---
#define WEB_PORT 8080
#define SECURE_PORT 5001
#define UPLOAD_FOLDER "./received_files/"
#define BUFFER_SIZE 32768
#define AES_BLOCK_SIZE 16
#define ADDRESS_BOOK_FILE "address_book.txt" // NEW: Address book file name

namespace fs = std::filesystem;

// NEW: Global Address Book
std::map<std::string, std::string> address_book;

// ====================================================
//  ADDRESS BOOK MANAGEMENT FUNCTIONS
// ====================================================

/**
 * @brief Loads addresses from the file into the global map.
 */
void load_address_book() {
    std::ifstream file(ADDRESS_BOOK_FILE);
    if (!file.is_open()) {
        std::cout << "[INFO] Address book file not found. Creating a new one." << std::endl;
        return;
    }

    std::string name, ip;
    address_book.clear();
    while (file >> name >> ip) {
        address_book[name] = ip;
    }
    file.close();
    std::cout << "[INFO] Loaded " << address_book.size() << " addresses." << std::endl;
}

/**
 * @brief Saves addresses from the global map to the file.
 */
void save_address_book() {
    std::ofstream file(ADDRESS_BOOK_FILE);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not save address book!" << std::endl;
        return;
    }

    for (const auto& pair : address_book) {
        file << pair.first << " " << pair.second << "\n";
    }
    file.close();
}

/**
 * @brief Handles adding, deleting, and viewing addresses.
 */
void manage_addresses() {
    std::cout << "\n--- Address Book Management ---" << std::endl;
    std::cout << "1. View All | 2. Add New | 3. Delete | 4. Back" << std::endl;
    std::cout << "> ";
    int choice;
    std::cin >> choice;

    std::string name, ip;

    switch (choice) {
        case 1: // View
            if (address_book.empty()) {
                std::cout << "[BOOK] Address book is empty." << std::endl;
                break;
            }
            std::cout << "\n--- Saved Peers (" << address_book.size() << ") ---" << std::endl;
            for (const auto& pair : address_book) {
                std::cout << "  > [" << pair.first << "]: " << pair.second << std::endl;
            }
            std::cout << "---------------------------------" << std::endl;
            break;

        case 2: // Add
            std::cout << "Enter Peer Name (no spaces): ";
            std::cin >> name;
            std::cout << "Enter Peer IP Address: ";
            std::cin >> ip;
            address_book[name] = ip;
            save_address_book();
            std::cout << "[BOOK] Successfully added " << name << "." << std::endl;
            break;

        case 3: // Delete
            std::cout << "Enter name of peer to DELETE: ";
            std::cin >> name;
            if (address_book.erase(name)) {
                save_address_book();
                std::cout << "[BOOK] Successfully deleted " << name << "." << std::endl;
            } else {
                std::cout << "[BOOK] Peer not found." << std::endl;
            }
            break;

        case 4: // Back
        default:
            return;
    }
}

// ====================================================
//  PART 1: THE WEB SERVER (Professional UI)
// ====================================================
// ... (run_web_server function remains unchanged) ...
void run_web_server() {
    if (!fs::exists(UPLOAD_FOLDER)) fs::create_directory(UPLOAD_FOLDER);
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html = R"HTML(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
                <title>Intra-Net</title>
                <style>
                    :root { --bg: #0f0f13; --card: #1c1c24; --accent: #6c5ce7; --text: #ffffff; --muted: #a0a0b0; }
                    * { box-sizing: border-box; margin: 0; padding: 0; }
                    body {
                        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
                        background: var(--bg); color: var(--text);
                        display: flex; flex-direction: column; align-items: center;
                        min-height: 100vh; padding: 20px;
                    }
                    .header { margin-bottom: 30px; text-align: center; }
                    h1 { font-size: 24px; font-weight: 700; margin-bottom: 5px; }
                    .badge { background: rgba(108, 92, 231, 0.2); color: var(--accent); padding: 4px 12px; border-radius: 20px; font-size: 12px; font-weight: 600; }
                    
                    /* Card Design */
                    .card {
                        background: var(--card); width: 100%; max-width: 400px;
                        border-radius: 16px; padding: 24px; margin-bottom: 20px;
                        box-shadow: 0 10px 30px rgba(0,0,0,0.2);
                        border: 1px solid rgba(255,255,255,0.05);
                    }
                    h2 { font-size: 18px; margin-bottom: 15px; display: flex; align-items: center; gap: 10px; }
                    
                    /* Custom File Input */
                    .file-drop-area {
                        border: 2px dashed #333; border-radius: 12px; padding: 30px 20px;
                        text-align: center; cursor: pointer; position: relative; transition: 0.2s;
                    }
                    .file-drop-area:hover { border-color: var(--accent); background: rgba(108, 92, 231, 0.05); }
                    input[type=file] { position: absolute; left: 0; top: 0; width: 100%; height: 100%; opacity: 0; cursor: pointer; }
                    .file-msg { color: var(--muted); font-size: 14px; pointer-events: none; }
                    
                    /* Button */
                    .btn-primary {
                        background: var(--accent); color: white; border: none; width: 100%;
                        padding: 14px; border-radius: 12px; font-weight: 600; font-size: 16px;
                        margin-top: 15px; cursor: pointer; transition: transform 0.1s;
                    }
                    .btn-primary:active { transform: scale(0.98); opacity: 0.9; }
                    .empty-state { text-align: center; color: var(--muted); font-size: 14px; padding: 20px; background: rgba(255,255,255,0.02); border-radius: 8px; }
                </style>
                <script>
                    function updateFileName(input) {
                        const msg = document.getElementById('file-msg');
                        if(input.files && input.files.length > 0) {
                            msg.innerText = "Selected: " + input.files[0].name;
                            msg.style.color = "#6c5ce7"; msg.style.fontWeight = "bold";
                        } else { msg.innerText = "Tap to select a file"; }
                    }
                </script>
            </head>
            <body>
                <div class="header">
                    <h1>Mark 3 Node</h1>
                    <span class="badge">SECURE • P2P • FAST</span>
                </div>

                <div class="card">
                    <h2>⬆️ Send to Laptop</h2>
                    <form action="/upload" method="post" enctype="multipart/form-data">
                        <div class="file-drop-area">
                            <span class="file-msg" id="file-msg">Tap to select a file</span>
                            <input type="file" name="file" onchange="updateFileName(this)" required>
                        </div>
                        <button class="btn-primary" type="submit">Transfer Now</button>
                    </form>
                </div>

                <div class="card">
                    <h2>⬇️ Download from Laptop</h2>
                    <div class="empty-state">
                        Files in 'received_files' will appear here.
                    </div>
                </div>
            </body>
            </html>
        )HTML"; 
        res.set_content(html, "text/html");
    });

    // 2. Handle Uploads (Stable API)
    svr.Post("/upload", [](const httplib::Request& req, httplib::Response& res) {
        if (req.has_file("file")) {
            auto file = req.get_file_value("file");
            std::string filename = file.filename;
            if (filename.empty()) filename = "mobile_upload.bin";
            
            std::cout << "[WEB] 📱 Mobile uploading: " << filename << " (" << file.content.size() << " bytes)..." << std::endl;
            
            std::string save_path = UPLOAD_FOLDER + filename;
            std::ofstream out(save_path, std::ios::binary);
            if (out) {
                out.write(file.content.c_str(), file.content.size());
                out.close();
                std::cout << "[WEB] ✅ File Saved: " << save_path << std::endl;
                
                // Success Page
                std::string success_html = R"HTML(
                    <!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
                    <style>body{background:#0f0f13;color:white;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}
                    .card{background:#1c1c24;padding:40px;border-radius:20px} h1{color:#00b894} a{color:#6c5ce7;text-decoration:none;font-weight:bold;font-size:18px}</style>
                    </head><body><div class="card"><h1>✅ Sent!</h1><p>File saved on Laptop.</p><br><a href="/">Send Another</a></div></body></html>
                )HTML";
                res.set_content(success_html, "text/html");
            } else {
                res.status = 500;
                res.set_content("Error saving file.", "text/plain");
            }
        } else {
            res.status = 400;
            res.set_content("No file found.", "text/plain");
        }
    });

    std::cout << "[WEB] Server running on Port " << WEB_PORT << std::endl;
    svr.listen("0.0.0.0", WEB_PORT);
}


// ====================================================
//  PART 2: THE SECURE TCP SERVER (For C++ Clients)
// ====================================================
// ... (run_secure_server function remains unchanged) ...
void run_secure_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SECURE_PORT);
    
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    std::cout << "[TCP] Secure Server listening on Port " << SECURE_PORT << std::endl;

    while (true) {
        int sock = accept(server_fd, NULL, NULL);
        if (sock < 0) continue;

        std::cout << "\n[TCP] 💻 Incoming Secure Connection..." << std::endl;
        
        // --- DH Handshake ---
        DiffieHellman dh;
        dh.init();
        dh.generate_keys();

        int peer_len;
        recv(sock, &peer_len, sizeof(int), 0);
        std::vector<unsigned char> peer_pub(peer_len);
        recv(sock, peer_pub.data(), peer_len, 0);

        std::vector<unsigned char> my_pub = dh.get_public_key();
        int pub_len = my_pub.size();
        send(sock, &pub_len, sizeof(int), 0);
        send(sock, my_pub.data(), pub_len, 0);

        std::vector<unsigned char> shared_secret = dh.compute_shared_secret(peer_pub);
        unsigned char key[32];
        memcpy(key, shared_secret.data(), 32);

        // --- Receive File ---
        unsigned char iv[AES_BLOCK_SIZE];
        recv(sock, iv, AES_BLOCK_SIZE, 0);

        int filename_len;
        recv(sock, &filename_len, sizeof(int), 0);
        char filename[256] = {0};
        recv(sock, filename, filename_len, 0);

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

        std::string save_path = UPLOAD_FOLDER + std::string(filename);
        std::ofstream outfile(save_path, std::ios::binary);
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
        std::cout << "[TCP] Secure File Received: " << filename << std::endl;
    }
}


// ====================================================
//  PART 3: MAIN CONTROLLER (Updated with Address Book)
// ====================================================
int main() {
    load_address_book(); // NEW: Load addresses on startup

    std::cout << "=========================================" << std::endl;
    std::cout << "     MARK 3: HYBRID NODE LAUNCHING...    " << std::endl;
    std::cout << "=========================================" << std::endl;

    std::thread web_thread(run_web_server);
    std::thread tcp_thread(run_secure_server);

    system("hostname -I | awk '{print \"[INFO] Your IP is: \" $1}'");

    while (true) {
        std::cout << "\n[MENU] 1. Send File (Secure) | 2. Manage Addresses | 3. Exit" << std::endl; // UPDATED MENU
        std::cout << "> ";
        int choice;
        std::cin >> choice;

        if (choice == 1) {
            std::string ip, file;
            std::string name_choice;

            // Address Book Selection Logic
            if (!address_book.empty()) {
                std::cout << "\n--- Select Peer ---" << std::endl;
                for (const auto& pair : address_book) {
                    std::cout << "  > [" << pair.first << "] (" << pair.second << ")" << std::endl;
                }
                std::cout << "Enter Peer Name OR a new IP: ";
                std::cin >> name_choice;

                if (address_book.count(name_choice)) {
                    ip = address_book[name_choice];
                    std::cout << "Target IP set to: " << ip << std::endl;
                } else {
                    ip = name_choice; // Assume user entered a raw IP
                    std::cout << "Using raw IP: " << ip << std::endl;
                }
            } else {
                std::cout << "Target IP (or Peer Name if using discovery): "; 
                std::cin >> ip;
            }

            std::cout << "File Path: "; 
            std::cin >> file;
            
            // Invoke the standalone sender binary
            std::string cmd = "./sender_dh " + ip + " " + file;
            system(cmd.c_str());
        } 
        else if (choice == 2) { // NEW: Manage Addresses
            manage_addresses();
        }
        else if (choice == 3) {
            save_address_book(); // NEW: Save before exiting
            exit(0);
        }
    }

    web_thread.join();
    tcp_thread.join();
    return 0;
}