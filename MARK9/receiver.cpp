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
        // Listen on Port 9000 (Matches your Playit configuration)
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 9000));

        // Increase OS Buffer Size to prevent drops during high speed
        boost::asio::socket_base::receive_buffer_size option(1024 * 1024 * 8); // 8MB Buffer
        socket.set_option(option);

        cout << "=== TURBO RECEIVER ONLINE (PORT 9000) ===" << endl;
        cout << "Waiting for connection..." << endl;

        FilePacket packet;
        udp::endpoint sender_ep;
        ofstream outfile;
        uint32_t packets_received = 0;

        while (true) {
            size_t len = socket.receive_from(buffer(&packet, sizeof(FilePacket)), sender_ep);

            if (packet.type == START_TRANSFER) {
                string filename(packet.data, packet.data_len);
                outfile.open("received_" + filename, ios::binary);
                packets_received = 0;
                cout << "\n[+] Incoming File: " << filename << endl;
                
                // Send Ready ACK
                FilePacket ack = {ACK, 0, 0};
                socket.send_to(buffer(&ack, sizeof(ack)), sender_ep);
            }
            else if (packet.type == FILE_DATA) {
                if (outfile.is_open()) {
                    // DECRYPT HERE IF NEEDED
                    outfile.write(packet.data, packet.data_len);
                    packets_received++;
                    
                    if (packets_received % 500 == 0) {
                        cout << "\r[+] Received " << (packets_received * 4) / 1024 << " KB" << flush;
                    }
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