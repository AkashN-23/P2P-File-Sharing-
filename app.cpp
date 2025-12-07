#include <iostream>
#include <thread>
#include <atomic>
#include <limits>
#include "discovery.h" // Includes our radar logic

// Helper to clear input buffer
void clear_cin() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void start_receiver_mode() {
    std::atomic<bool> keeping_alive(true);
    // Start listening for discovery broadcasts in the background
    std::thread discovery_thread(run_discovery_listener, std::ref(keeping_alive));

    std::cout << "\n=== RECEIVER MODE (Visible to Room) ===" << std::endl;
    // Call your existing receiver binary
    // ensure receiver_dh is in the same folder!
    system("./receiver_dh");

    keeping_alive = false; // Stop the listener
    discovery_thread.join(); // Wait for thread to close
}

void start_sender_mode() {
    std::cout << "\n=== SENDER MODE ===" << std::endl;
    
    // 1. Auto-Scan
    std::vector<Peer> peers = scan_for_peers();

    if (peers.empty()) {
        std::cout << "[-] No peers found! Is the receiver running on another machine?" << std::endl;
        std::cout << "[?] For testing, run './receiver_dh' in another terminal first." << std::endl;
        return;
    }

    // 2. Display List
    std::cout << "\nFound Peers:" << std::endl;
    for (size_t i = 0; i < peers.size(); ++i) {
        std::cout << " [" << i + 1 << "] " << peers[i].hostname << " (" << peers[i].ip << ")" << std::endl;
    }

    // 3. Select User
    int choice;
    std::cout << "\nSelect Peer #: ";
    std::cin >> choice;
    clear_cin();

    if (choice < 1 || choice > peers.size()) {
        std::cout << "Invalid choice." << std::endl;
        return;
    }
    std::string target_ip = peers[choice - 1].ip;

    // 4. Input File
    std::string filepath;
    std::cout << "Drag & Drop file to send: ";
    std::getline(std::cin, filepath);

    // Remove quotes if the terminal added them (e.g. 'file.jpg')
    if (filepath.size() >= 2 && (filepath.front() == '\'' || filepath.front() == '"')) {
        filepath = filepath.substr(1, filepath.size() - 2);
    }
    // Remove trailing space (common in drag-drop)
    if (!filepath.empty() && filepath.back() == ' ') {
        filepath.pop_back();
    }

    // 5. Launch Sender
    std::string cmd = "./sender_dh " + target_ip + " \"" + filepath + "\"";
    system(cmd.c_str());
}

int main() {
    while (true) {
        std::cout << "\n==========================" << std::endl;
        std::cout << "   SECURE P2P SHARE v2.0  " << std::endl;
        std::cout << "==========================" << std::endl;
        std::cout << "1. Receive File" << std::endl;
        std::cout << "2. Send File" << std::endl;
        std::cout << "3. Exit" << std::endl;
        std::cout << "> ";

        int opt;
        std::cin >> opt;
        clear_cin();

        if (opt == 1) start_receiver_mode();
        else if (opt == 2) start_sender_mode();
        else if (opt == 3) break;
    }
    return 0;
}