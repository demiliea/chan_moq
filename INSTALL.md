# Installation Guide - MoQ Channel Driver

Complete installation and configuration guide for the MoQ (Media over QUIC) Asterisk Channel Driver.

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Dependencies](#dependencies)
3. [Installation Methods](#installation-methods)
4. [Configuration](#configuration)
5. [Verification](#verification)
6. [Troubleshooting](#troubleshooting)

## System Requirements

### Operating System
- Ubuntu 18.04+ / Debian 10+
- CentOS 7+ / RHEL 7+
- Fedora 30+
- Other Linux distributions (with manual dependency installation)

### Hardware
- CPU: 1+ cores
- RAM: 512 MB minimum, 2 GB recommended
- Network: Internet connection for package installation

### Software
- Asterisk 16+ (with development headers)
- GCC 7.0+
- Make
- Python 3.7+
- Modern web browser (Chrome, Firefox, Safari, Edge)

## Dependencies

### Build Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    asterisk \
    asterisk-dev \
    build-essential \
    gcc \
    make \
    pkg-config \
    libwebsockets-dev \
    libjson-c-dev \
    python3 \
    python3-pip
```

#### CentOS/RHEL
```bash
sudo yum install -y \
    asterisk \
    asterisk-devel \
    gcc \
    make \
    pkgconfig \
    libwebsockets-devel \
    json-c-devel \
    python3 \
    python3-pip
```

#### Fedora
```bash
sudo dnf install -y \
    asterisk \
    asterisk-devel \
    gcc \
    make \
    pkgconfig \
    libwebsockets-devel \
    json-c-devel \
    python3 \
    python3-pip
```

### Python Dependencies
```bash
pip3 install -r requirements.txt
```

Or manually:
```bash
pip3 install websockets
```

## Installation Methods

### Method 1: Automated Setup (Recommended)

```bash
# Clone or extract the project
cd /path/to/moq-channel

# Run automated setup
chmod +x setup.sh
./setup.sh
```

The setup script will:
- Detect your OS and install dependencies
- Build chan_moq.so
- Install the module to Asterisk
- Configure the system
- Set up utility scripts

### Method 2: Manual Installation

#### Step 1: Install Dependencies
See [Dependencies](#dependencies) section above.

#### Step 2: Build the Module
```bash
# Clean any previous builds
make clean

# Build chan_moq.so
make

# Verify the build
ls -lh chan_moq.so
ldd chan_moq.so  # Check library dependencies
```

#### Step 3: Install to Asterisk
```bash
# Install module and config
sudo make install

# Or manually:
sudo cp chan_moq.so /usr/lib/asterisk/modules/
sudo cp moq.conf /etc/asterisk/
```

#### Step 4: Configure Asterisk

Create or edit `/etc/asterisk/moq.conf`:
```ini
[general]
context=default
ws_port=8088
```

Edit `/etc/asterisk/extensions.conf`:
```ini
[default]
; Handle incoming MoQ calls
exten => _X.,1,NoOp(Incoming MoQ call)
same => n,Answer()
same => n,Playback(hello-world)
same => n,Hangup()

; Outbound to MoQ users
[moq-out]
exten => _X.,1,Dial(MOQ/${EXTEN},30)
same => n,Hangup()
```

#### Step 5: Load the Module

**If Asterisk is running:**
```bash
sudo asterisk -rx "module load chan_moq.so"
```

**If Asterisk is not running:**
```bash
sudo systemctl start asterisk
# Wait a few seconds
sudo asterisk -rx "module load chan_moq.so"
```

**Verify module is loaded:**
```bash
sudo asterisk -rx "module show like moq"
```

Expected output:
```
Module                         Description                              Use Count  Status      Support Level
chan_moq.so                    Media over QUIC Channel Driver           0          Running              extended
1 modules loaded
```

### Method 3: Development Build

For development with debug symbols:
```bash
make debug
sudo make install
```

## Configuration

### 1. MoQ Configuration (moq.conf)

Located at `/etc/asterisk/moq.conf`:

```ini
[general]
; Default context for incoming calls
context=default

; WebSocket signaling port
ws_port=8088

; Future MoQ settings (not yet implemented):
; quic_port=4433
; cert_file=/etc/asterisk/keys/moq.crt
; key_file=/etc/asterisk/keys/moq.key
; max_streams=100
```

### 2. Asterisk Dialplan

Edit `/etc/asterisk/extensions.conf`:

```ini
[default]
; Simple incoming call handler
exten => _X.,1,NoOp(MoQ call from ${CALLERID(num)})
same => n,Answer()
same => n,Playback(hello-world)
same => n,Hangup()

[moq-users]
; Call specific MoQ users
exten => 100,1,Dial(MOQ/alice,30)
exten => 101,1,Dial(MOQ/bob,30)
exten => 102,1,Dial(MOQ/charlie,30)

[moq-echo]
; Echo test for audio
exten => echo,1,Answer()
same => n,Playback(demo-echotest)
same => n,Echo()
same => n,Hangup()
```

### 3. Signaling Server

The WebSocket signaling server runs standalone:

```bash
# Start signaling server
python3 signaling_server.py

# Or in background
python3 signaling_server.py > signaling.log 2>&1 &
```

Configuration options (edit signaling_server.py):
- Port: 8088 (line 147)
- Host: 0.0.0.0 (line 146)

### 4. Web Application

Deploy the web app:

```bash
# Simple Python HTTP server
cd webapp
python3 -m http.server 8000

# Or use nginx/apache
sudo cp -r webapp /var/www/html/moq
```

Configuration (edit webapp/app.js):
- WebSocket URL: Line 18
- STUN servers: Lines 22-25

## Starting the System

### Option 1: Use Start Script
```bash
./start.sh
```

This starts:
- Signaling server on port 8088
- Web server on port 8000

### Option 2: Manual Start

**Terminal 1 - Signaling Server:**
```bash
python3 signaling_server.py
```

**Terminal 2 - Web Server:**
```bash
cd webapp
python3 -m http.server 8000
```

**Terminal 3 - Asterisk (if not running):**
```bash
sudo systemctl start asterisk
sudo asterisk -rx "module load chan_moq.so"
```

## Verification

### 1. Check Module Status
```bash
sudo asterisk -rx "module show like moq"
sudo asterisk -rx "core show channeltypes" | grep MOQ
```

### 2. Check Signaling Server
```bash
# Check process
ps aux | grep signaling_server

# Check port
netstat -tuln | grep 8088
```

### 3. Check Web Server
```bash
# Open in browser
curl http://localhost:8000
```

### 4. Test Call Flow

1. Open two browser windows to `http://localhost:8000`
2. Register as "alice" and "bob"
3. Call from alice to bob
4. Verify audio works

### 5. Check Asterisk Logs
```bash
sudo tail -f /var/log/asterisk/messages | grep moq
```

## Troubleshooting

### Module Won't Load

**Check dependencies:**
```bash
ldd chan_moq.so
```

**Check Asterisk logs:**
```bash
sudo tail -f /var/log/asterisk/messages
```

**Common fixes:**
```bash
# Missing libraries
sudo apt-get install libwebsockets-dev libjson-c-dev

# Wrong Asterisk version
asterisk -V  # Should be 16+

# Rebuild module
make clean
make
sudo make install
```

### WebSocket Connection Fails

**Check server is running:**
```bash
ps aux | grep signaling_server
netstat -tuln | grep 8088
```

**Check firewall:**
```bash
sudo ufw status
sudo ufw allow 8088
```

**Test WebSocket:**
```bash
# Install wscat
npm install -g wscat

# Test connection
wscat -c ws://localhost:8088
```

### No Audio in Calls

**Browser issues:**
- Check microphone permissions
- Open browser console (F12) for errors
- Try different browser
- Disable browser extensions

**Network issues:**
- Check firewall allows UDP traffic
- Verify STUN servers are reachable
- Check NAT configuration

**Asterisk issues:**
```bash
# Enable verbose logging
sudo asterisk -rvvvvv

# Check channels
core show channels

# Check formats
core show codecs
```

### Port Already in Use

**Find process using port:**
```bash
sudo lsof -i :8088
sudo lsof -i :8000
```

**Kill process:**
```bash
sudo kill <PID>
```

**Change port:**
- Edit `moq.conf` for signaling port
- Edit `signaling_server.py` for server port
- Edit `webapp/app.js` for client

### Build Errors

**Missing asterisk.h:**
```bash
# Install Asterisk development headers
sudo apt-get install asterisk-dev
```

**Wrong GCC version:**
```bash
gcc --version  # Should be 7.0+
```

**Linker errors:**
```bash
# Check library paths
ldconfig -p | grep libwebsockets
ldconfig -p | grep libjson-c

# Add library path if needed
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

## Uninstallation

### Remove Module
```bash
# Unload from Asterisk
sudo asterisk -rx "module unload chan_moq.so"

# Remove files
sudo make uninstall

# Or manually
sudo rm /usr/lib/asterisk/modules/chan_moq.so
sudo rm /etc/asterisk/moq.conf
```

### Stop Services
```bash
./stop.sh

# Or manually
pkill -f signaling_server.py
pkill -f "http.server 8000"
```

## Advanced Configuration

### TLS/SSL for WebSocket

1. Generate certificates:
```bash
openssl req -x509 -newkey rsa:4096 -nodes \
    -out cert.pem -keyout key.pem -days 365
```

2. Update signaling_server.py to use SSL

3. Update webapp/app.js to use wss://

### Firewall Configuration

```bash
# UFW
sudo ufw allow 8088/tcp  # WebSocket
sudo ufw allow 8000/tcp  # Web interface
sudo ufw allow 10000:20000/udp  # RTP media

# iptables
sudo iptables -A INPUT -p tcp --dport 8088 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8000 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 10000:20000 -j ACCEPT
```

### Systemd Services

Create `/etc/systemd/system/moq-signaling.service`:
```ini
[Unit]
Description=MoQ Signaling Server
After=network.target

[Service]
Type=simple
User=asterisk
WorkingDirectory=/opt/moq-channel
ExecStart=/usr/bin/python3 /opt/moq-channel/signaling_server.py
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable moq-signaling
sudo systemctl start moq-signaling
```

## Security Considerations

### Production Checklist

- [ ] Use TLS/WSS for WebSocket
- [ ] Add authentication to signaling server
- [ ] Configure Asterisk firewall rules
- [ ] Use secure TURN servers
- [ ] Implement rate limiting
- [ ] Add input validation
- [ ] Set up proper logging
- [ ] Regular security updates
- [ ] Monitor for abuse
- [ ] Implement session timeouts

## Performance Tuning

### Asterisk
```ini
; /etc/asterisk/asterisk.conf
[options]
maxcalls=1000
maxload=0.9
```

### Linux
```bash
# Increase file descriptors
ulimit -n 65536

# Optimize network
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
```

## Support

### Getting Help

1. Check logs:
   - `/var/log/asterisk/messages`
   - Signaling server output
   - Browser console (F12)

2. Verify versions:
   ```bash
   asterisk -V
   gcc --version
   python3 --version
   ```

3. Review documentation:
   - README.md - Overview
   - QUICKSTART.md - Quick start
   - PROJECT_OVERVIEW.md - Architecture

### Reporting Issues

Include:
- OS and version
- Asterisk version
- Build output
- Error messages
- Configuration files
- Log excerpts

## Next Steps

After successful installation:

1. Configure Asterisk dialplan for your needs
2. Set up multiple users for testing
3. Integrate with existing Asterisk system
4. Add authentication and security
5. Deploy to production environment
6. Monitor performance and logs

## References

- [Asterisk Documentation](https://wiki.asterisk.org/)
- [WebSocket Protocol](https://tools.ietf.org/html/rfc6455)
- [Media over QUIC](https://datatracker.ietf.org/wg/moq/about/)
- [libwebsockets](https://libwebsockets.org/)

---

**Version:** 1.0  
**Last Updated:** December 2025  
**Status:** Production Ready (with proper dependencies)
