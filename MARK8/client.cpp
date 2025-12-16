#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic> 
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using json = nlohmann::json;

// ==================================================================================
// --- CONFIGURATION SECTION ---
// ==================================================================================

// OPTION 1: SINGLE LAPTOP TESTING (Uncomment these 2 lines to test alone)
// const std::string SERVER_IP = "127.0.0.1";
// const int SERVER_PORT = 9000;

// OPTION 2: REAL INTERNET / PLAYIT (Uncomment these 2 lines for real use)
const std::string SERVER_IP = "147.185.221.27";  // Your Playit Numeric IP
const int SERVER_PORT = 9442;                    // Your Playit External Port

// ALWAYS KEEP THIS 0 (Lets the OS pick a random open port)
const int LOCAL_PORT = 0; 
// ==================================================================================


// Global Variables
udp::endpoint peer_endpoint;
std::atomic<bool> is_connected{false};
std::atomic<bool> peer_found{false}; 

void listen_for_messages(udp::socket& socket) {
    std::array<char, 1024> recv_buffer;
    udp::endpoint sender_endpoint;

    for (;;) {
        try {
            size_t len = socket.receive_from(buffer(recv_buffer), sender_endpoint);
            string received_data(recv_buffer.data(), len);

            try {
                // 1. Try to parse as JSON (Server Messages)
                json message_json = json::parse(received_data);
                string type = message_json.value("type", "UNKNOWN");

                if (type == "REGISTER_ACK") {
                    cout << "[System] Registered Successfully!" << endl;
                } 
                else if (type == "PEER_INFO") {
                    string peer_ip = message_json.value("peer_ip", "N/A");
                    int peer_port = message_json.value("peer_port", 0);

                    // Safety Check
                    if (peer_ip == "N/A" || peer_port == 0) {
                        cout << "[System] Peer NOT FOUND. Server could not locate ID." << endl;
                        continue; 
                    }

                    // Save Peer Info
                    peer_endpoint = udp::endpoint(address::from_string(peer_ip), peer_port);
                    peer_found = true; 
                    
                    cout << "\n[System] Found Peer at " << peer_ip << ":" << peer_port << endl;
                    cout << "[System] Punching hole through firewall..." << endl;
                    
                    // AUTOMATIC PUNCH: Send immediate greeting to open the NAT
                    string punch_msg = "HOLE_PUNCH_GREETING";
                    socket.send_to(buffer(punch_msg), peer_endpoint);
                } 

            } catch (const json::parse_error&) {
                // 2. IF NOT JSON -> IT IS A CHAT MESSAGE FROM THE PEER!
                
                if (received_data == "HOLE_PUNCH_GREETING") {
                    if (!is_connected) {
                        is_connected = true;
                        cout << "\n=================================================" << endl;
                        cout << ">>> CONNECTION ESTABLISHED! YOU CAN CHAT NOW <<<" << endl;
                        cout << "=================================================" << endl;
                    }
                }
                else {
                    // It's a real text message
                    cout << "\n[Peer]: " << received_data << endl;
                    cout << "[Me]: " << flush; 
                }
            }
        } catch (const std::exception& e) {
             // Ignore read errors (keep listener alive)
        }
    }
}

int main() {
    try {
        io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), LOCAL_PORT));
        
        // Resolve the Server IP (works for both Localhost and Internet IPs)
        udp::endpoint server_endpoint(address::from_string(SERVER_IP), SERVER_PORT);

        string client_id, target_id;

        cout << "--- MARK 8 CHAT CLIENT ---" << endl;
        cout << "Mode: " << (SERVER_IP == "127.0.0.1" ? "LOCAL TEST" : "INTERNET/PLAYIT") << endl;
        cout << "Enter your Client ID (e.g. Kakashi): ";
        getline(cin, client_id);

        // Start the listener thread
        std::thread listener_thread(listen_for_messages, std::ref(socket));
        
        // 1. Register with Server
        json register_req = {{"type", "REGISTER"}, {"id", client_id}};
        socket.send_to(buffer(register_req.dump()), server_endpoint);
        std::this_thread::sleep_for(std::chrono::seconds(1)); 

        // 2. Loop until we find a valid peer
        while (!peer_found) {
            cout << "\nEnter Target Peer ID to connect (e.g. Obito): ";
            getline(cin, target_id);

            // Send Request
            json peer_req = {
                {"type", "PEER_REQUEST"},
                {"requester_id", client_id},
                {"target_id", target_id}
            };
            socket.send_to(buffer(peer_req.dump()), server_endpoint);
            
            cout << "[System] Asking server for " << target_id << "..." << endl;
            
            // Wait 2 seconds for response
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (peer_found) break; // Exit loop if found
        }

        // 3. CHAT MODE
        cout << "Type a message and press Enter (or 'exit' to quit)." << endl;
        while (true) {
            string msg;
            getline(cin, msg); 
            if (msg == "exit") break;

            if (is_connected) {
                socket.send_to(buffer(msg), peer_endpoint);
            } else {
                cout << "[System] Still connecting... (Sending retry packet)" << endl;
                // Force a retry packet if they type before connection is ready
                socket.send_to(buffer("HOLE_PUNCH_GREETING"), peer_endpoint);
            }
        }

        socket.close();
        if (listener_thread.joinable()) listener_thread.join();

    } catch (const std::exception& e) {
        cerr << "Fatal Error: " << e.what() << endl;
    }
    return 0;
}