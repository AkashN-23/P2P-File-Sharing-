#include <iostream>
#include <string>
#include <map>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp> // Ensure you have this header for JSON support

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using json = nlohmann::json;

// --- CONFIGURATION ---
// The local port the server listens on. Playit.gg forwards external traffic to this port.
const int LOCAL_PORT = 9000; 
// --- END CONFIGURATION ---

// Structure to hold a client's ID and public endpoint (IP:Port)
struct ClientInfo {
    udp::endpoint endpoint;
    string id;
};

// Map to store registered clients: Key is the client ID (string), Value is ClientInfo
map<string, ClientInfo> client_registry;
// Buffer for incoming UDP data
array<char, 512> recv_buffer;

/**
 * @brief Parses an incoming JSON message and handles the request (REGISTER or PEER_REQUEST).
 * @param message_json The parsed JSON object.
 * @param sender_endpoint The public endpoint of the sender (as seen by the server).
 * @param socket The server's UDP socket.
 */
void handle_message(const json& message_json, const udp::endpoint& sender_endpoint, udp::socket& socket) {
    if (!message_json.contains("type")) return;

    string type = message_json["type"].get<string>();

    if (type == "REGISTER") {
        if (!message_json.contains("id")) return;

        string client_id = message_json["id"].get<string>();
        string sender_ip = sender_endpoint.address().to_string();
        int sender_port = sender_endpoint.port();

        // 1. Store the client's information
        client_registry[client_id] = {sender_endpoint, client_id};

        cout << "[SERVER] Registered Client: ID=" << client_id
             << ", Public Endpoint=" << sender_ip << ":" << sender_port << endl;
        
        // 2. Respond with a success message including their public endpoint
        json response = {
            {"type", "REGISTER_ACK"},
            {"status", "SUCCESS"},
            {"public_ip", sender_ip},
            {"public_port", sender_port}
        };

        socket.send_to(buffer(response.dump()), sender_endpoint);

    } else if (type == "PEER_REQUEST") {
        if (!message_json.contains("requester_id") || !message_json.contains("target_id")) return;

        string requester_id = message_json["requester_id"].get<string>();
        string target_id = message_json["target_id"].get<string>();

        cout << "[SERVER] Peer Request: " << requester_id << " -> " << target_id << endl;

        auto target_it = client_registry.find(target_id);
        auto requester_it = client_registry.find(requester_id);

        if (target_it != client_registry.end() && requester_it != client_registry.end()) {
            // Target peer found: Initiate the exchange of coordinates (IP:Port)
            const ClientInfo& target_info = target_it->second;
            const ClientInfo& requester_info = requester_it->second;

            // 1. Send the target's info to the requester (Client A)
            json response_to_requester = {
                {"type", "PEER_INFO"},
                {"peer_id", target_id},
                {"peer_ip", target_info.endpoint.address().to_string()},
                {"peer_port", target_info.endpoint.port()}
            };
            socket.send_to(buffer(response_to_requester.dump()), requester_info.endpoint);

            // 2. Send the requester's info to the target (Client B)
            json response_to_target = {
                {"type", "PEER_INFO"},
                {"peer_id", requester_id},
                {"peer_ip", requester_info.endpoint.address().to_string()},
                {"peer_port", requester_info.endpoint.port()}
            };
            socket.send_to(buffer(response_to_target.dump()), target_info.endpoint);

            cout << "[SERVER] Sent peer information to both " << requester_id << " and " << target_id << endl;

        } else {
            // Target peer not found
            json response = {
                {"type", "PEER_INFO"},
                {"status", "ERROR"},
                {"message", "Target ID not found or Requester ID mismatch."}
            };
            socket.send_to(buffer(response.dump()), sender_endpoint);
            cerr << "[SERVER] Target ID " << target_id << " not found." << endl;
        }

    } // Other types are ignored or treated as P2P data
}

int main() {
    try {
        io_context io_context;
        // Bind the socket to the local port (9000)
        udp::socket socket(io_context, udp::endpoint(udp::v4(), LOCAL_PORT));

        cout << "[SERVER] Mark 8 Signaling Server Online on Port " << LOCAL_PORT << endl;
        

        udp::endpoint sender_endpoint;

        for (;;) {
            size_t len = socket.receive_from(buffer(recv_buffer), sender_endpoint);
            string received_data(recv_buffer.data(), len);

            try {
                json message_json = json::parse(received_data);
                handle_message(message_json, sender_endpoint, socket);

            } catch (const json::parse_error& e) {
                // If it's not JSON, it might be a direct P2P packet, which the server ignores.
                // We'll just print a notification if it's too long to be ignored.
                if (len > 5) {
                    cerr << "[SERVER] Received non-JSON data (possibly P2P) from "
                         << sender_endpoint.address().to_string() << ":" << sender_endpoint.port() << endl;
                }
            }
        }
    } catch (const exception& e) {
        cerr << "[SERVER] Fatal Exception: " << e.what() << endl;
        return 1;
    }
    return 0;
}