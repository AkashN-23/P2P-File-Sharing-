#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include "file_structs.hpp"

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

int main() {
    try {
        io_context io_context;
        // Listen on Port 9000 (LAN and WAN)
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 9000));

        // Enable Broadcasting (Essential for LAN discovery)
        socket.set_option(socket_base::broadcast(true));
        
        // Large buffer for speed
        socket.set_option(socket_base::receive_buffer_size(8 * 1024 * 1024));

        cout << "=== SMART RECEIVER ONLINE (PORT 9000) ===" << endl;
        cout << "Ready for LAN or WAN connections..." << endl;

        FilePacket packet;
        udp::endpoint sender_ep;
        ofstream outfile;
        uint32_t packets_received = 0;

        while (true) {
            size_t len = socket.receive_from(buffer(&packet, sizeof(FilePacket)), sender_ep);

            // 1. LAN DISCOVERY PING
            if (packet.type == 99) { // 99 = "Are you there?"
                // Reply immediately to the sender so they know our LAN IP
                FilePacket pong = {100, 0, 0}; // 100 = "I am here!"
                socket.send_to(buffer(&pong, sizeof(pong)), sender_ep);
                cout << "[*] Discovered by Local Peer: " << sender_ep.address().to_string() << endl;
                continue;
            }

            // 2. FILE TRANSFER LOGIC (Same as before)
            if (packet.type == START_TRANSFER) {
                string filename(packet.data, packet.data_len);
                outfile.open("received_" + filename, ios::binary);
                packets_received = 0;
                cout << "\n[+] Incoming File: " << filename << endl;
                
                FilePacket ack = {ACK, 0, 0};
                socket.send_to(buffer(&ack, sizeof(ack)), sender_ep);
            }
            else if (packet.type == FILE_DATA) {
                if (outfile.is_open()) {
                    outfile.write(packet.data, packet.data_len);
                    packets_received++;
                    if (packets_received % 1000 == 0) cout << "." << flush;
                }
            }
            else if (packet.type == END_TRANSFER) {
                cout << "\n[+] Transfer Complete!" << endl;
                if (outfile.is_open()) outfile.close();
            }
        }
    } catch (std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}