#include <iostream>
#include <limits>
#include <thread>
#include <atomic>
#include "dh_helper.h"
#include "discovery.h"

// We need to declare the main functions (we will copy-paste your logic into functions here)
// Instead of full copy-paste, I will call system() to keep it simple for you, 
// OR we can integrate the code properly. Let's integrate for cleaner C++.

// --- COPY YOUR SENDER/RECEIVER LOGIC HERE ---
// For brevity, I will abstract the network logic into functions.
// YOU MUST PASTE YOUR PREVIOUS main() CODE FROM sender_dh.cpp INTO start_sender()
// AND receiver_dh.cpp INTO start_receiver().

// =========================================================
//  SECTION 1: RECEIVER FUNCTION
// =========================================================
void start_receiver() {
    std::atomic<bool> discovery_running(true);
    // 1. Start the UDP Discovery Thread so people can find us
    std::thread discovery_thread(run_discovery_listener, std::ref(discovery_running));
    discovery_thread.detach(); // Let it run in background

    // 2. Run the TCP Receiver Logic (Paste your receiver_dh.cpp logic here)
    // NOTE: Remove "int main()" wrapper. Just put the body here.
    // Ensure you use the same headers as before.
    
    std::cout << "\n=== RECEIVER MODE ACTIVE ===" << std::endl;
    std::cout << "[*] Waiting for incoming connections..." << std::endl;
    std::cout << "[*] (My IP is broadcasted automatically)" << std::endl;

    // --- PASTE receiver_dh.cpp BODY HERE ---
    // (For this example, I'll simulate it calling your existing binary)
    // In a real merged app, you'd put the C++ code here.
    system("./receiver_dh"); 
    // ---------------------------------------

    discovery_running = false; // Stop discovery when done
}

// =========================================================
//  SECTION 2: SENDER FUNCTION
// =========================================================
void start_sender() {
    std::cout << "\n=== SENDER MODE ===" << std::endl;
    
    // 1. Scan for Peers
    std::vector<Peer> peers = scan_for_peers();

    if (peers.empty()) {
        std::cout << "[-] No peers found. Is the receiver running?" << std::endl;
        return;
    }

    // 2. Show Menu
    std::cout << "\nAvailable Peers:" << std::endl;
    for (size_t i = 0; i < peers.size(); ++i) {
        std::cout << " " << i + 1 << ". " << peers[i].hostname << " (" << peers[i].ip << ")" << std::endl;
    }

    int choice;
    std::cout << "\nSelect Peer (1-" << peers.size() << "): ";
    std::cin >> choice;

    if (choice < 1 || choice > peers.size()) {
        std::cout << "Invalid choice." << std::endl;
        return;
    }
    
    std::string target_ip = peers[choice - 1].ip;
    
    // 3. Get File Path
    std::string filepath;
    std::cout << "Drag and Drop file here to send: ";
    std::cin >> filepath;
    
    // Clean up drag-drop quotes if present (e.g. 'file.jpg')
    if (filepath.front() == '\'' || filepath.front() == '"') {
        filepath = filepath.substr(1, filepath.length() - 2);
    }

    // 4. Run Sender Logic
    // Construct command to call your existing sender binary
    std::string command = "./sender_dh " + target_ip + " \"" + filepath + "\"";
    system(command.c_str());
}

// =========================================================
//  MAIN MENU
// =========================================================
int main() {
    while (true) {
        std::cout << "\n==========================" << std::endl;
        std::cout << "  SECURE P2P FILE SHARE   " << std::endl;
        std::cout << "==========================" << std::endl;
        std::cout << "1. Receive File (Be Visible)" << std::endl;
        std::cout << "2. Send File (Scan Network)" << std::endl;
        std::cout << "3. Exit" << std::endl;
        std::cout << "Select option: ";

        int opt;
        std::cin >> opt;

        if (opt == 1) {
            start_receiver();
        } else if (opt == 2) {
            start_sender();
        } else if (opt == 3) {
            break;
        } else {
            std::cout << "Invalid Option" << std::endl;
        }
    }
    return 0;
}