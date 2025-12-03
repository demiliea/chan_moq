# Quick Start Guide - MoQ Phone System

## 5-Minute Setup

### Prerequisites
```bash
# Install Python dependencies
pip3 install -r requirements.txt

# Install Asterisk dependencies (Ubuntu/Debian)
sudo apt-get install asterisk-dev libwebsockets-dev libjson-c-dev build-essential
```

### Step 1: Build Asterisk Module
```bash
make
sudo make install
```

### Step 2: Configure Asterisk
```bash
# Copy config file
sudo cp moq.conf /etc/asterisk/

# Add to extensions.conf (or use the example)
sudo nano /etc/asterisk/extensions.conf
# Add the [default] context from extensions.conf.example

# Restart or load module
sudo asterisk -rx "module load chan_moq.so"
```

### Step 3: Start the System
```bash
./start.sh
```

This will start:
- WebSocket signaling server on port 8088
- Web interface on http://localhost:8000

### Step 4: Test with Two Browsers

**Browser 1:**
1. Open http://localhost:8000
2. Enter User ID: `alice`
3. Click "Connect"

**Browser 2:**
1. Open http://localhost:8000 (new tab/window)
2. Enter User ID: `bob`
3. Click "Connect"

**Make a Call:**
1. In alice's browser, enter `bob` in "Call User ID"
2. Click "Call"
3. Bob will receive incoming call notification
4. Click "Answer" in bob's browser
5. You're connected!

### Stop the System
```bash
./stop.sh
```

## Troubleshooting

**Can't hear audio?**
- Check browser microphone permissions
- Open browser console (F12) to check for errors

**Connection failed?**
- Verify signaling server is running: `ps aux | grep signaling_server`
- Check port 8088 is not blocked: `netstat -tuln | grep 8088`

**Module won't load in Asterisk?**
- Check dependencies: `ldd chan_moq.so`
- Check Asterisk logs: `sudo tail -f /var/log/asterisk/messages`

## Next Steps

- Configure Asterisk dialplan for incoming calls
- Add more users to test
- Integrate with existing Asterisk system
- See full README.md for advanced configuration

## Architecture Summary

```
Browser (WebRTC) <---> Signaling Server <---> Asterisk (chan_moq.so)
     |                                              |
     +---------- Direct Media Connection -----------+
```

## Useful Commands

```bash
# View signaling server logs
journalctl -f | grep signaling

# Check Asterisk module status
sudo asterisk -rx "module show like moq"

# Check active channels
sudo asterisk -rx "core show channels"

# Reload Asterisk module
sudo asterisk -rx "module reload chan_moq.so"

# Asterisk CLI (troubleshooting)
sudo asterisk -rvvvvv
```

## Default Ports

- **8088**: WebSocket signaling
- **8000**: Web interface
- **UDP**: WebRTC media (dynamic ports)

## Security Note

⚠️ This is a development setup. For production:
- Use TLS for WebSocket (wss://)
- Add authentication
- Configure firewall rules
- Use secure TURN servers
