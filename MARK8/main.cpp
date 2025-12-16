#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <boost/array.hpp>  

using boost::asio::ip::udp;
using json = nlohmann::json;

// CONFIGURATION
const int DISCOVERY_PORT = 8888;
const std::string DEVICE_NAME = "Fedora-Commander"; // Change this for your other device

class LanDiscovery {
public:
    LanDiscovery(boost::asio::io_context& io_context)
        : socket_(io_context, udp::endpoint(udp::v4(), DISCOVERY_PORT)) {
        
        // Enable broadcasting (Essential for LAN discovery)
        socket_.set_option(boost::asio::socket_base::broadcast(true));

        // Start listening immediately
        start_receive();
    }

    // The "Beacon": Broadcasts existence to the network
    void start_broadcasting() {
        std::thread([this]() {
            while (true) {
                try {
                    // Create the JSON payload
                    json payload = {
                        {"type", "DISCOVERY"},
                        {"name", DEVICE_NAME},
                        {"status", "ONLINE"}
                    };
                    std::string msg = payload.dump();

                    // Broadcast to 255.255.255.255 (The entire LAN)
                    udp::endpoint broadcast_endpoint(boost::asio::ip::address_v4::broadcast(), DISCOVERY_PORT);
                    
                    socket_.send_to(boost::asio::buffer(msg), broadcast_endpoint);
                    // std::cout << "[Sent Beacon]" << std::endl; // Uncomment to see outgoing pings
                } catch (std::exception& e) {
                    std::cerr << "Broadcast Error: " << e.what() << std::endl;
                }

                // Wait 2 seconds before next ping
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }).detach(); // Run in background thread
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    handle_message(std::string(recv_buffer_.data(), bytes_recvd));
                }
                start_receive(); // Loop back to listen again
            });
    }

    void handle_message(std::string raw_msg) {
        try {
            // Parse JSON
            auto j = json::parse(raw_msg);

            // Filter out our own messages (Optional: check against own IP)
            if (j["name"] == DEVICE_NAME) return;

            std::cout << "\n[!] FOUND PEER: " 
                      << j["name"] << " at " << remote_endpoint_.address().to_string() 
                      << std::endl;

        } catch (const std::exception&) {
            // Ignore malformed packets
        }
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    boost::array<char, 1024> recv_buffer_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        
        std::cout << "--- MARK 8: DISCOVERY SERVICE STARTED ---" << std::endl;
        std::cout << "Listening on Port " << DISCOVERY_PORT << "..." << std::endl;

        LanDiscovery service(io_context);
        service.start_broadcasting();

        io_context.run(); // Keeps the app running
    } catch (std::exception& e) {
        std::cerr << "Main Error: " << e.what() << std::endl;
    }
    return 0;
}