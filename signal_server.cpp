#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <iostream>

// Use a simple server type without TLS for the signaling (Cloudflare handles SSL)
typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;

class signaling_server {
public:
    signaling_server() {
        // Initialize Asio
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&signaling_server::on_open, this, std::placeholders::_1));
        m_server.set_close_handler(bind(&signaling_server::on_close, this, std::placeholders::_1));
        m_server.set_message_handler(bind(&signaling_server::on_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    void run(uint16_t port) {
        m_server.listen(port);
        m_server.start_accept();
        std::cout << ">>> Signaling Server listening on port " << port << std::endl;
        m_server.run();
    }

private:
    typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

    server m_server;
    con_list m_connections;

    void on_open(connection_hdl hdl) {
        m_connections.insert(hdl);
        std::cout << "Client Connected!" << std::endl;
    }

    void on_close(connection_hdl hdl) {
        m_connections.erase(hdl);
        std::cout << "Client Disconnected!" << std::endl;
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        // Broadcast the message to everyone ELSE (not the sender)
        for (auto it : m_connections) {
            // Check if this connection is NOT the sender
            if (it.owner_before(hdl) || hdl.owner_before(it)) {
                try {
                    m_server.send(it, msg->get_payload(), msg->get_opcode());
                } catch (websocketpp::exception const & e) {
                    std::cout << "Send failed: " << e.what() << std::endl;
                }
            }
        }
    }
};

int main() {
    signaling_server s_server;
    s_server.run(8080);
    return 0;
}