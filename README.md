# Mark 3: Hybrid P2P File Sharing Node

![Status](https://img.shields.io/badge/Status-Active-success)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Mobile-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)

**Mark 3** is a high-performance, secure, and cross-platform Peer-to-Peer (P2P) file transfer tool. It features a unique **Hybrid Architecture** that runs two concurrent servers to ensure maximum speed and universal compatibility.

---

## 🌟 Key Features

### 1. 🚀 Hybrid Dual-Core Architecture

* **Port 5001 (Secure Core):** Dedicated High-Speed TCP channel for Laptop-to-Laptop transfers. Uses **AES-256 Encryption** + **Diffie-Hellman Key Exchange** for military-grade security.
* **Port 8080 (Web Core):** Embedded HTTP Server for Mobile/Tablet compatibility. No app installation required—works directly in any Web Browser (Chrome/Safari).

### 2. 📱 Modern UI & Experience

* **Dark Mode Web Interface:** A professional, responsive UI for mobile users with drag-and-drop support.
* **Address Book:** Save frequently used peer IPs so you never have to type them again.
* **Real-time Telemetry:** Dashboard displays Transfer Speed, Packet Loss, and Latency.

### 3. 🛡️ Security

* **End-to-End Encryption:** Data sent via the C++ client is encrypted before leaving the device.
* **Ephemeral Storage:** No cloud servers. Data moves directly from Device A to Device B.

---

## 🛠️ Prerequisites

To build this project, you need a C++ compiler and the OpenSSL development libraries.

### Fedora / RedHat / CentOS

```bash
sudo dnf install openssl-devel gcc-c++ git
```

### Ubuntu / Debian / Kali

```bash
sudo apt update
sudo apt install libssl-dev g++ git
```

---

## ⚙️ Installation & Compilation

### 1. Clone the Repository

```bash
git clone <YOUR_REPO_URL>
cd mark3-p2p
```

### 2. Compile the Hybrid Node (Receiver + Web Server)

Builds the main application that acts as the server for both mobile and laptop peers.

```bash
g++ mark3.cpp -o mark3 -lssl -lcrypto -pthread -std=c++17 -Wno-deprecated-declarations
```

### 3. Compile the Sender Tool (Client)

Builds the tool used to send files securely to other Mark 3 nodes.

```bash
g++ sender_dh.cpp -o sender_dh -lssl -lcrypto -Wno-deprecated-declarations
```

---

## 🔥 Network Setup (Important!)

Since this application listens on specific ports, you must allow them through your firewall.

### On Fedora / RedHat

```bash
sudo firewall-cmd --add-port=8080/tcp
sudo firewall-cmd --add-port=5001/tcp
```

### On Ubuntu (UFW)

```bash
sudo ufw allow 8080/tcp
sudo ufw allow 5001/tcp
```

---

## 📘 How to Use

### 1. Start the Application

Run the main hybrid node:

```bash
./mark3
```

You will see:

```
[WEB] Server running on Port 8080
[TCP] Secure Server listening on Port 5001
```

### 2. Connect via Mobile (iPhone/Android)

* Ensure your phone is on the same Wi-Fi as your laptop.
* Open your phone browser.
* Enter the IP shown in your terminal: `http://<YOUR_LAPTOP_IP>:8080`
* Upload/download files instantly.

### 3. Connect via Another Laptop (Secure Mode)

Use the sender tool:

```bash
./sender_dh
```

Choose:

* Option 1 (Send File)
* Enter peer IP
* Enter file path

Transfer begins with full encryption.

---

## 🌍 Connecting Over the Internet (WAN)

If devices are not on the same network:

### ✔️ Method 1: Mobile Hotspot (Recommended)

Connect laptop to mobile hotspot.

### ✔️ Method 2: SSH Tunneling (Advanced)

```bash
ssh -R 80:localhost:8080 nokey@localhost.run
```

Share generated link like:

```
https://random.localhost.run
```

---

## 📂 Project Structure

```
mark3.cpp        - Main Hybrid Node (Web Server + Secure TCP Server)
sender_dh.cpp    - Secure file sending tool
dh_helper.h      - Diffie-Hellman + AES helper library
httplib.h        - Single-header HTTP library
address_book.txt - Saved peer IPs
```

---

## 📜 License

This project is open-source and available for educational and personal use.
