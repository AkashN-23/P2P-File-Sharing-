#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <string>
#include <variant>

using json = nlohmann::json;
using namespace std::chrono_literals;

// CONFIGURATION
const std::string SIGNALING_URL = "ws://localhost:8080"; 
const size_t CHUNK_SIZE = 64 * 1024; // 64 KB chunks

// GLOBAL VARIABLES
std::string incoming_file_name = "received_file";
std::ofstream outfile;
size_t received_bytes = 0;
size_t total_file_size = 0;

// HELPER: Send a File
void sendFile(std::shared_ptr<rtc::DataChannel> dc, std::string filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << ">>> ERROR: Could not open file: " << filepath << std::endl;
        return;
    }

    size_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

    std::cout << ">>> STARTING TRANSFER: " << filename << " (" << filesize / (1024 * 1024) << " MB)" << std::endl;

    json meta = {{"type", "metadata"}, {"name", filename}, {"size", filesize}};
    dc->send(meta.dump());

    std::vector<std::byte> buffer(CHUNK_SIZE);
    size_t sent_bytes = 0;
    
    while (file.read((char*)buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
        size_t n = file.gcount();
        if (n < CHUNK_SIZE) {
            std::vector<std::byte> last_chunk(buffer.begin(), buffer.begin() + n);
            dc->send(last_chunk.data(), last_chunk.size());
        } else {
            dc->send(buffer.data(), buffer.size());
        }
        
        sent_bytes += n;
        if (sent_bytes % (10 * 1024 * 1024) == 0) { 
            std::cout << ">>> Sent: " << sent_bytes / (1024 * 1024) << " MB..." << "\r" << std::flush;
        }
        
        if (dc->bufferedAmount() > 10 * 1024 * 1024) { 
             std::this_thread::sleep_for(100ms);
        }
    }
    std::cout << "\n>>> FILE SENT SUCCESSFULLY!" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: ./p2p_client [initiator|receiver] [filename_to_send]" << std::endl;
        return -1;
    }
    bool is_initiator = (std::string(argv[1]) == "initiator");
    std::string file_to_send = (argc > 2) ? argv[2] : "";

    rtc::Configuration config;
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");
    
    // We make these shared pointers so we can capture them in lambdas
    auto pc = std::make_shared<rtc::PeerConnection>(config);
    auto ws = std::make_shared<rtc::WebSocket>();
    
    // Holds the DataChannel
    std::shared_ptr<rtc::DataChannel> dc;

    // --- DATA CHANNEL SETUP HELPER ---
    auto setupDataChannel = [is_initiator, file_to_send](std::shared_ptr<rtc::DataChannel> channel) {
        channel->onOpen([is_initiator, file_to_send, channel]() {
            std::cout << ">>> DIRECT CONNECTION ESTABLISHED!" << std::endl;
            if (is_initiator && !file_to_send.empty()) {
                std::thread t(sendFile, channel, file_to_send);
                t.detach();
            }
        });

        channel->onMessage([channel](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                std::string msg = std::get<std::string>(data);
                try {
                    auto j = json::parse(msg);
                    if (j.contains("type") && j["type"] == "metadata") {
                        incoming_file_name = "received_" + std::string(j["name"]);
                        total_file_size = j["size"];
                        outfile.open(incoming_file_name, std::ios::binary);
                        received_bytes = 0;
                        std::cout << ">>> INCOMING FILE: " << incoming_file_name << " (" << total_file_size << " bytes)" << std::endl;
                    } else {
                        std::cout << "Peer: " << msg << std::endl;
                    }
                } catch (...) { std::cout << "Msg: " << msg << std::endl; }
            } 
            else if (std::holds_alternative<std::vector<std::byte>>(data)) {
                if (outfile.is_open()) {
                    auto bytes = std::get<std::vector<std::byte>>(data);
                    outfile.write((char*)bytes.data(), bytes.size());
                    received_bytes += bytes.size();
                    
                    if (received_bytes >= total_file_size) {
                        std::cout << ">>> DOWNLOAD COMPLETE! Saved to " << incoming_file_name << std::endl;
                        outfile.close();
                    } else if (received_bytes % (10 * 1024 * 1024) == 0) {
                         std::cout << ">>> Downloading... " << (received_bytes / (1024*1024)) << " MB\r" << std::flush;
                    }
                }
            }
        });
    };

    // --- SIGNALING CALLBACKS ---
    pc->onLocalDescription([ws](rtc::Description description) {
        json j = {{"type", description.typeString()}, {"sdp", std::string(description)}};
        // Sending the Offer/Answer via WebSocket
        if (ws->isOpen()) {
            ws->send(j.dump());
            std::cout << ">>> SDP " << description.typeString() << " sent." << std::endl;
        } else {
            std::cerr << ">>> ERROR: WebSocket not open. Cannot send SDP." << std::endl;
        }
    });

    pc->onLocalCandidate([ws](rtc::Candidate candidate) {
        json j = {{"type", "candidate"}, {"candidate", std::string(candidate)}, {"mid", candidate.mid()}};
        if (ws->isOpen()) ws->send(j.dump());
    });

    // If we are Receiver, we wait for the DataChannel from the Initiator
    if (!is_initiator) {
        pc->onDataChannel([setupDataChannel](std::shared_ptr<rtc::DataChannel> channel) {
            setupDataChannel(channel);
        });
    }

    // --- WEBSOCKET HANDLERS ---
    ws->onOpen([&, is_initiator, pc]() {
        std::cout << ">>> Connected to Signaling Server." << std::endl;
        
        // [FIX IS HERE]: Only create the DataChannel (which creates the Offer)
        // AFTER the WebSocket is successfully Open.
        if (is_initiator) {
            std::cout << ">>> Creating Data Channel (Initiator)..." << std::endl;
            // We must assign to 'dc' so it doesn't go out of scope, but we use a local var to capture
            auto channel = pc->createDataChannel("file-transfer"); 
            setupDataChannel(channel);
            // NOTE: The 'dc' variable in main scope is weak here due to lambda capture complexity
            // but the channel is kept alive by the PeerConnection internal map.
        }
    });

    ws->onMessage([pc](auto data) {
        if (!std::holds_alternative<std::string>(data)) return;
        json j = json::parse(std::get<std::string>(data));
        std::string type = j["type"];

        if (type == "offer") {
            std::cout << ">>> Received Offer." << std::endl;
            pc->setRemoteDescription(rtc::Description(j["sdp"], type));
        } 
        else if (type == "answer") {
            std::cout << ">>> Received Answer." << std::endl;
            pc->setRemoteDescription(rtc::Description(j["sdp"], type));
        } 
        else if (type == "candidate") {
            // std::cout << ">>> Received Candidate." << std::endl; 
            pc->addRemoteCandidate(rtc::Candidate(j["candidate"], j["mid"]));
        }
    });

    ws->open(SIGNALING_URL);

    std::cout << "Waiting... (Press Ctrl+C to quit)" << std::endl;
    // Keep the main thread alive
    while(true) std::this_thread::sleep_for(1s);
    return 0;
}