#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <random> 
#include <algorithm> 
#include <cstdlib> 
#include <iomanip>
#include <chrono>
#include <boost/asio.hpp>

// --- CROSS PLATFORM MACROS ---
#ifdef _WIN32
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
    #define OPEN_BROWSER_CMD "start "
    #define SLEEP_MS(x) Sleep(x)
    const std::string SLASH = "\\";
#else
    #include <unistd.h>
    #define CLEAR_SCREEN "clear"
    #define OPEN_BROWSER_CMD "xdg-open "
    #define SLEEP_MS(x) usleep(x * 1000)
    const std::string SLASH = "/";
#endif

// --- NAMESPACES ALIASES ---
namespace fs = std::filesystem;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = boost::asio::ip::udp;

// --- CONFIGURATION ---
const int PORT_WEB = 8080;       
const size_t BUFFER_SIZE = 1048576; 

// GLOBAL VARIABLES
asio::io_context io;
std::string SECURITY_TOKEN = ""; 
std::string GLOBAL_URL = ""; 
std::string STORAGE_PATH = ""; // Will be set to Desktop/Mark87_Shared

// --- HELPER: GET DESKTOP PATH ---
std::string get_storage_path() {
    std::string home_dir;
    #ifdef _WIN32
        home_dir = std::getenv("USERPROFILE");
    #else
        home_dir = std::getenv("HOME");
    #endif
    
    if (home_dir.empty()) return "Shared"; // Fallback
    
    // Create "Mark87_Shared" on Desktop
    std::string path = home_dir + SLASH + "Desktop" + SLASH + "Mark87_Shared";
    return path;
}

// --- HELPER: PROGRESS BAR ---
void print_progress(size_t current, size_t total, std::chrono::steady_clock::time_point start_time) {
    auto now = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time).count();
    if (elapsed_sec < 0.1) return; 

    double speed_mbps = (current / 1024.0 / 1024.0) / elapsed_sec;
    int percent = (int)((current * 100.0) / total);
    
    int bar_width = 30;
    int pos = (bar_width * percent) / 100;
    
    std::cout << "\r [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << percent << "% | " << std::fixed << std::setprecision(1) << speed_mbps << " MB/s   " << std::flush;
}

// --- HELPER: FILENAME EXTRACTOR ---
std::string extract_filename(std::string content) {
    std::string filename = "uploaded_file.bin"; 
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
    size_t pos = lower_content.find("filename=");
    if (pos == std::string::npos) return filename;
    pos += 9; 
    if (pos < content.length() && (content[pos] == '\"' || content[pos] == '\'')) {
        pos++; 
        size_t end = content.find_first_of("\"'", pos);
        if (end != std::string::npos) filename = content.substr(pos, end - pos);
    } else {
        size_t end = content.find_first_of(";\r\n", pos);
        filename = content.substr(pos, end - pos);
    }
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) filename = filename.substr(last_slash + 1);
    return filename;
}

std::string generate_token() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    return std::to_string(dis(gen));
}

std::string get_local_ip() {
    try {
        udp::socket socket(io, udp::endpoint(udp::v4(), 0));
        socket.connect(udp::endpoint(asio::ip::address::from_string("8.8.8.8"), 53));
        return socket.local_endpoint().address().to_string();
    } catch(...) { return "127.0.0.1"; }
}

// --- DASHBOARD UI ---
std::string get_html_page() {
    std::string display_host = (GLOBAL_URL.empty()) ? get_local_ip() : "GLOBAL TUNNEL ACTIVE";
    
    std::string html = R"HTML(
    <!DOCTYPE html>
    <html>
    <head>
        <title>MARK 87 HUB</title>
        <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
        <style>
            :root { --primary: #00ff9d; --bg: #050505; --card: #111; }
            body { font-family: sans-serif; text-align: center; padding: 20px; background: var(--bg); color: #e0e0e0; }
            .container { max-width: 800px; margin: auto; }
            h1 { color: var(--primary); letter-spacing: 3px; text-transform: uppercase; }
            .box { background: var(--card); padding: 25px; border-radius: 16px; margin-bottom: 25px; border: 1px solid #333; text-align: left; }
            button { width: 100%; background: var(--primary); color: #000; border: none; padding: 15px; font-weight: bold; border-radius: 8px; cursor: pointer; margin-top: 10px; }
            .file-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap: 15px; }
            .file-card { background: #1a1a1a; padding: 15px; border-radius: 10px; text-align: center; color: #ccc; text-decoration: none; border: 1px solid #333; display: flex; flex-direction: column; }
            .f-icon { font-size: 2.5rem; display: block; margin-bottom: 10px; }
            .f-name { font-size: 0.9rem; word-break: break-all; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Mark 87</h1>
            <div style="background:#222; display:inline-block; padding:5px 15px; border-radius:20px; margin-bottom:20px;">HOST: )HTML" + display_host + R"HTML(</div>
            
            <div class="box" style="border-top: 3px solid #00ff9d;">
                <h3>📤 UPLOAD TO HUB</h3>
                <form action="/upload" method="post" enctype="multipart/form-data">
                    <input type="file" name="file" required style="color:white; margin-bottom:10px;">
                    <button type="submit">UPLOAD</button>
                </form>
            </div>
            <div class="box">
                <h3>📂 FILES ON DESKTOP</h3>
                <div class="file-grid">
    )HTML";

    size_t pos_token = html.find("action=\"/upload\"");
    if(pos_token != std::string::npos) html.replace(pos_token, 16, "action=\"/upload?token=" + SECURITY_TOKEN + "\"");

    // SCAN ONLY THE DESKTOP FOLDER
    if (!fs::exists(STORAGE_PATH)) fs::create_directory(STORAGE_PATH);
    
    for (const auto& entry : fs::directory_iterator(STORAGE_PATH)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if(name[0] != '.') {
                std::string icon = "📄";
                if(name.find(".jpg") != std::string::npos || name.find(".png") != std::string::npos) icon = "🖼️";
                else if(name.find(".mp4") != std::string::npos) icon = "🎬";
                else if(name.find(".pdf") != std::string::npos) icon = "📕";
                html += "<a class='file-card' href='/download/" + name + "?token=" + SECURITY_TOKEN + "'>";
                html += "<span class='f-icon'>" + icon + "</span>";
                html += "<span class='f-name'>" + name + "</span>";
                html += "</a>";
            }
        }
    }
    html += "</div></div></div></body></html>";
    return html;
}

// --- WEB ENGINE ---
void handle_web_client(tcp::socket socket) {
    try {
        asio::streambuf request_buf;
        size_t header_end = asio::read_until(socket, request_buf, "\r\n\r\n");
        std::string header_str(asio::buffers_begin(request_buf.data()), asio::buffers_begin(request_buf.data()) + header_end);
        
        std::string boundary = "";
        std::stringstream hss(header_str);
        std::string line;
        size_t content_length = 0;
        
        while(std::getline(hss, line)) {
            std::string lower_line = line;
            std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
            if(lower_line.find("content-length:") != std::string::npos) {
                try { content_length = std::stoull(line.substr(line.find(":") + 1)); } catch(...) {}
            }
            if(lower_line.find("boundary=") != std::string::npos) {
                boundary = line.substr(line.find("boundary=") + 9);
                boundary.erase(std::remove_if(boundary.begin(), boundary.end(), ::isspace), boundary.end());
            }
        }
        std::string stop_sign = "\r\n--" + boundary;
        request_buf.consume(header_end); 
        
        std::stringstream ss(header_str);
        std::string method, uri;
        ss >> method >> uri;

        if (uri.find("token=" + SECURITY_TOKEN) == std::string::npos) return; 

        if (method == "GET" && (uri.find("/index") != std::string::npos || uri.size() < 25)) { 
            std::string content = get_html_page();
            std::string response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
            asio::write(socket, asio::buffer(response));
        }
        else if (method == "GET" && uri.find("/download/") != std::string::npos) {
            size_t start = uri.find("/download/") + 10;
            size_t end = uri.find("?");
            std::string filename = uri.substr(start, end - start);
            size_t pos; while((pos = filename.find("%20")) != std::string::npos) filename.replace(pos, 3, " ");
            
            // READ FROM DESKTOP FOLDER
            std::string full_path = STORAGE_PATH + SLASH + filename;
            std::ifstream infile(full_path, std::ios::binary | std::ios::ate);
            
            if(infile) {
                size_t size = infile.tellg();
                infile.seekg(0);
                std::string head = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + std::to_string(size) + 
                              "\r\nContent-Disposition: attachment; filename=\"" + filename + "\"\r\n\r\n";
                asio::write(socket, asio::buffer(head));
                std::vector<char> buf(BUFFER_SIZE); 
                while(infile.read(buf.data(), buf.size()) || infile.gcount() > 0) 
                    asio::write(socket, asio::buffer(buf.data(), infile.gcount()));
            }
        }
        else if (method == "POST" && uri.find("/upload") != std::string::npos) {
            size_t bytes_in_buf = request_buf.size();
            std::string processing_buffer(asio::buffers_begin(request_buf.data()), asio::buffers_end(request_buf.data()));
            request_buf.consume(bytes_in_buf);

            char temp_buf[8192];
            bool file_start_found = false;
            size_t file_start_pos = 0;
            
            while (!file_start_found) {
                size_t pos = processing_buffer.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    file_start_found = true;
                    file_start_pos = pos + 4;
                    break;
                }
                boost::system::error_code err;
                size_t len = socket.read_some(asio::buffer(temp_buf), err);
                if (err || len == 0) break;
                processing_buffer.append(temp_buf, len);
            }

            if (!file_start_found) return;

            std::string filename = extract_filename(processing_buffer.substr(0, file_start_pos));
            std::cout << "\n[Web] Incoming: " << filename << std::endl;

            // SAVE TO DESKTOP FOLDER
            if (!fs::exists(STORAGE_PATH)) fs::create_directory(STORAGE_PATH);
            std::ofstream outfile(STORAGE_PATH + SLASH + filename, std::ios::binary);
            
            std::string data_buffer = processing_buffer.substr(file_start_pos);
            std::vector<char> read_chunk(BUFFER_SIZE); 
            bool stop_found = false;
            size_t total_written = 0;
            auto start_time = std::chrono::steady_clock::now();

            while (!stop_found) {
                size_t stop_pos = data_buffer.find(stop_sign);
                if (stop_pos != std::string::npos) {
                    outfile.write(data_buffer.data(), stop_pos);
                    total_written += stop_pos;
                    stop_found = true;
                } else {
                    if (data_buffer.size() > stop_sign.length()) {
                        size_t safe_to_write = data_buffer.size() - stop_sign.length();
                        outfile.write(data_buffer.data(), safe_to_write);
                        total_written += safe_to_write;
                        data_buffer = data_buffer.substr(safe_to_write);
                    }
                    boost::system::error_code err;
                    size_t len = socket.read_some(asio::buffer(read_chunk.data(), read_chunk.size()), err);
                    if (err || len == 0) break; 
                    data_buffer.append(read_chunk.data(), len);
                    
                    if (content_length > 0) print_progress(total_written, content_length - file_start_pos, start_time);
                }
            }
            outfile.close();
            std::cout << "\r                                                                        \r";
            std::string resp = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nUpload Complete!";
            asio::write(socket, asio::buffer(resp));
            std::cout << "[Success] Saved to Desktop/" << filename << std::endl;
        }
    } catch (...) {}
}

void start_web_server() {
    try {
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), PORT_WEB));
        while (true) {
            tcp::socket socket(io);
            acceptor.accept(socket);
            std::thread(handle_web_client, std::move(socket)).detach();
        }
    } catch(...) {}
}

void open_browser(std::string url) {
    std::string cmd = std::string(OPEN_BROWSER_CMD) + "\"" + url + "\"";
    #ifndef _WIN32
        cmd += " > /dev/null 2>&1 &"; 
    #endif
    system(cmd.c_str());
}

int main() {
    // --- SET STORAGE TO DESKTOP ---
    STORAGE_PATH = get_storage_path();
    if (!fs::exists(STORAGE_PATH)) {
        try { fs::create_directory(STORAGE_PATH); }
        catch(...) { STORAGE_PATH = "Shared"; fs::create_directory(STORAGE_PATH); } // Fallback
    }

    SECURITY_TOKEN = generate_token(); 
    std::thread t_web(start_web_server);
    t_web.detach();

    system(CLEAR_SCREEN);
    std::cout << "\n==============================================" << std::endl;
    std::cout << "        MARK 87 HUB (V6 - DESKTOP SYNC)       " << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << " [1] Local Network" << std::endl;
    std::cout << " [2] Global Tunnel (Ngrok)" << std::endl;
    std::cout << "\n Enter Choice [1/2]: ";
    int choice;
    if (!(std::cin >> choice)) { choice = 1; }

    std::string final_url;
    
    if (choice == 2) {
        std::cout << "\n [INPUT] Paste your Ngrok URL: ";
        std::string tunnel_url;
        std::cin >> tunnel_url;
        if(tunnel_url.find("/index") != std::string::npos) 
            tunnel_url = tunnel_url.substr(0, tunnel_url.find("/index"));
        GLOBAL_URL = tunnel_url;
        final_url = GLOBAL_URL + "/index?token=" + SECURITY_TOKEN;
    } else {
        std::string ip = get_local_ip();
        final_url = "http://" + ip + ":" + std::to_string(PORT_WEB) + "/index?token=" + SECURITY_TOKEN;
    }

    system(CLEAR_SCREEN);
    std::cout << "\n==============================================" << std::endl;
    std::cout << "        MARK 87 ONLINE  [SECURE]" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << " [INFO]  Sync Folder: " << STORAGE_PATH << std::endl;
    std::cout << " [INFO]  Token: " << SECURITY_TOKEN << std::endl;
    std::cout << "\n [STEP 1] Share this URL:" << std::endl;
    std::cout << "          " << final_url << std::endl;
    std::cout << "\n [STEP 2] Scan this QR Code:" << std::endl;
    
    #ifndef _WIN32
        std::string qr = "qrencode -t ANSIUTF8 \"" + final_url + "\"";
        system(qr.c_str());
    #endif
    
    open_browser(final_url);

    while(true) std::this_thread::sleep_for(std::chrono::hours(1));
    return 0;
}