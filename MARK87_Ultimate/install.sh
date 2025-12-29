#!/bin/bash
echo "=========================================="
echo "      INSTALLING MARK 87 HUB"
echo "=========================================="

# 1. Force Root (Sudo)
if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] Please run as root (Type: sudo ./install.sh)"
  exit
fi

# Get the real user (who called sudo) to configure Ngrok for THEM, not Root
REAL_USER=$SUDO_USER
USER_HOME=$(eval echo ~${SUDO_USER})

# 2. Install Libraries
echo "[INFO] Installing System Requirements..."
apt-get update -y > /dev/null 2>&1
apt-get install -y libboost-system-dev libboost-thread-dev qrencode curl > /dev/null 2>&1

# 3. Install Ngrok (if missing)
if ! command -v ngrok &> /dev/null; then
    echo "[INFO] Installing Global Tunnel (Ngrok)..."
    curl -s https://ngrok-agent.s3.amazonaws.com/ngrok.asc | tee /etc/apt/trusted.gpg.d/ngrok.asc >/dev/null
    echo "deb https://ngrok-agent.s3.amazonaws.com buster main" | tee /etc/apt/sources.list.d/ngrok.list
    apt-get update -y > /dev/null 2>&1
    apt-get install -y ngrok > /dev/null 2>&1
fi

# 4. Interactive Ngrok Login
echo ""
echo "=========================================="
echo "      NGROK CONFIGURATION (REQUIRED)"
echo "=========================================="
echo "To share files globally, you need a free Ngrok Token."
echo "1. Go to: https://dashboard.ngrok.com/get-started/your-authtoken"
echo "2. Copy the token."
echo ""
read -p "Paste your Token here (or press Enter to skip): " NGROK_TOKEN

if [ ! -z "$NGROK_TOKEN" ]; then
    # We run this command AS the real user, so the config saves to THEIR home folder
    su - $REAL_USER -c "ngrok config add-authtoken $NGROK_TOKEN"
    echo "[SUCCESS] Ngrok configured successfully for $REAL_USER."
else
    echo "[WARNING] Skipped. You will need to configure Ngrok manually later."
fi

# 5.