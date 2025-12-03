# MoQ Channel Driver for Asterisk

A channel driver for Asterisk that implements Media over QUIC (MoQ) protocol with WebSocket signaling, along with a modern web-based phone client.

## Overview

This project provides:
- **chan_moq.so**: An Asterisk channel driver implementing MoQ for media transport
- **WebSocket Signaling**: Simple WebSocket-based signaling for call control
- **Web App**: Browser-based phone client with modern UI

## Features

- ✅ WebSocket-based signaling (call, answer, hangup)
- ✅ WebRTC audio streaming
- ✅ Modern, responsive web UI
- ✅ Real-time call duration tracking
- ✅ User registration system
- ✅ Incoming call notifications
- 🚧 MoQ/QUIC media transport (currently using WebRTC as foundation)

## Architecture

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
│   Web Browser   │◄───────►│  WebSocket       │◄───────►│   Asterisk      │
│   (WebRTC)      │         │  Signaling       │         │   chan_moq.so   │
└─────────────────┘         │  Server          │         └─────────────────┘
                            └──────────────────┘
```

## Prerequisites

### For Asterisk Module (chan_moq.so)

- Asterisk development headers (asterisk-dev or equivalent)
- GCC compiler
- libwebsockets-dev
- libjson-c-dev
- pthread library

```bash
# Ubuntu/Debian
sudo apt-get install asterisk-dev libwebsockets-dev libjson-c-dev build-essential

# CentOS/RHEL
sudo yum install asterisk-devel libwebsockets-devel json-c-devel gcc make
```

### For Signaling Server

- Python 3.7+
- websockets library

```bash
pip3 install websockets
```

### For Web App

- Modern web browser (Chrome, Firefox, Safari, Edge)
- Web server (or use Python's built-in HTTP server)

## Installation

### 1. Build and Install Asterisk Module

```bash
# Build the module
make

# Install (requires root/sudo)
sudo make install

# Or manually copy
sudo cp chan_moq.so /usr/lib/asterisk/modules/
sudo cp moq.conf /etc/asterisk/
```

### 2. Configure Asterisk

Edit `/etc/asterisk/moq.conf`:

```ini
[general]
context=default
ws_port=8088
```

Edit `/etc/asterisk/extensions.conf` to add dialplan:

```ini
[default]
exten => _X.,1,NoOp(Incoming MoQ call)
exten => _X.,n,Dial(MOQ/${EXTEN})
exten => _X.,n,Hangup()
```

### 3. Load Module in Asterisk

```bash
# Connect to Asterisk CLI
sudo asterisk -rvvv

# Load the module
module load chan_moq.so

# Verify it's loaded
module show like moq
```

### 4. Start Signaling Server

```bash
# Make executable
chmod +x signaling_server.py

# Run the server
python3 signaling_server.py
```

The signaling server will start on port 8088.

### 5. Deploy Web App

```bash
cd webapp

# Option 1: Use Python HTTP server
python3 -m http.server 8000

# Option 2: Use any web server (nginx, apache, etc.)
# Copy webapp/ contents to your web server directory
```

## Usage

### Web Client

1. Open your browser to `http://localhost:8000` (or your web server URL)
2. Enter a unique User ID (e.g., "alice")
3. Click "Connect"
4. To make a call:
   - Enter the destination User ID (e.g., "bob")
   - Click "Call"
5. To answer an incoming call:
   - Click "Answer" in the popup

### With Asterisk

Once the module is loaded, you can:

1. **Make outbound calls from Asterisk to web clients**:
   ```
   Dial(MOQ/alice)
   ```

2. **Receive calls from web clients in Asterisk**:
   Web clients calling will reach the configured context in extensions.conf

### Testing Between Two Browsers

1. Open two browser windows/tabs
2. In first window:
   - User ID: "alice"
   - Click "Connect"
3. In second window:
   - User ID: "bob"
   - Click "Connect"
4. From alice's window:
   - Enter "bob" in Call User ID
   - Click "Call"
5. Bob's window will show incoming call
   - Click "Answer" to accept

## Configuration

### moq.conf

```ini
[general]
; Default context for incoming calls
context=default

; WebSocket signaling port
ws_port=8088

; Future MoQ-specific settings:
; quic_port=4433
; cert_file=/etc/asterisk/keys/moq.crt
; key_file=/etc/asterisk/keys/moq.key
; max_streams=100
```

### Signaling Server

Edit `signaling_server.py` to change:
- Port: Change `8088` in the `websockets.serve()` call
- Host: Change `0.0.0.0` to bind to specific interface

### Web App

Edit `webapp/app.js`:
- WebSocket URL: Change `this.wsUrl = 'ws://localhost:8088'`
- STUN/TURN servers: Modify `this.rtcConfig.iceServers`

## Architecture Details

### Channel Driver (chan_moq.c)

The Asterisk channel driver implements:
- Channel technology interface (`ast_channel_tech`)
- WebSocket server for signaling (using libwebsockets)
- Media transport layer (currently UDP, designed for QUIC/MoQ)
- Session management
- Call state machine

Key functions:
- `moq_request()` - Create new channel
- `moq_call()` - Initiate outbound call
- `moq_answer()` - Answer incoming call
- `moq_hangup()` - Terminate call
- `moq_read()`/`moq_write()` - Media I/O

### Signaling Protocol

WebSocket messages (JSON format):

**Register**:
```json
{
  "type": "register",
  "user_id": "alice"
}
```

**Call**:
```json
{
  "type": "call",
  "session_id": "session-123",
  "dest": "bob",
  "from": "alice"
}
```

**Answer**:
```json
{
  "type": "answer",
  "session_id": "session-123"
}
```

**Hangup**:
```json
{
  "type": "hangup",
  "session_id": "session-123"
}
```

### Media Transport

Currently implements:
- WebRTC audio (opus/pcm)
- UDP sockets for demonstration

Future MoQ implementation will use:
- QUIC transport protocol
- MoQ object model for media
- Low-latency streaming

## Troubleshooting

### Module won't load

Check Asterisk logs:
```bash
tail -f /var/log/asterisk/messages
```

Verify dependencies:
```bash
ldd chan_moq.so
```

### WebSocket connection fails

Check signaling server is running:
```bash
netstat -tuln | grep 8088
```

Check firewall rules:
```bash
sudo ufw allow 8088
```

### No audio in calls

- Verify microphone permissions in browser
- Check browser console for errors (F12)
- Verify WebRTC connection state in console

### "User not found" error

- Ensure both users are connected and registered
- Check signaling server logs for registration status

## Development

### Building in Debug Mode

```bash
make clean
CFLAGS="-g -O0" make
```

### Testing Module

```bash
# Run Asterisk with verbose logging
sudo asterisk -cvvvvv

# Load module
module load chan_moq.so

# Check for errors
core show channels
```

### Extending Functionality

To add features:
1. Update signaling protocol in `signaling_server.py`
2. Update channel driver in `chan_moq.c`
3. Update web client in `webapp/app.js`

## Roadmap

- [ ] Implement full MoQ protocol support
- [ ] Add QUIC transport layer
- [ ] Support video calls
- [ ] Add call recording
- [ ] Implement authentication
- [ ] Add encryption (TLS/DTLS)
- [ ] Support multiple concurrent calls
- [ ] Add presence/status indicators
- [ ] Implement call transfer
- [ ] Add conferencing support

## Performance Notes

- Current implementation uses WebRTC as the media foundation
- MoQ/QUIC will provide better performance for real-time media
- Signaling server can handle hundreds of concurrent connections
- For production use, consider:
  - Using a robust TURN server
  - Implementing authentication
  - Adding rate limiting
  - Using TLS for WebSocket connections

## Security Considerations

⚠️ **This is a development/demonstration implementation**

For production use:
- [ ] Add authentication to signaling server
- [ ] Use TLS for WebSocket connections
- [ ] Implement authorization in Asterisk dialplan
- [ ] Add rate limiting and DDoS protection
- [ ] Validate all input data
- [ ] Use secure TURN servers
- [ ] Implement session timeouts

## License

GNU General Public License v2

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## Support

For issues and questions:
- Check the troubleshooting section
- Review Asterisk logs
- Check browser console for client-side errors
- Review signaling server output

## References

- [Asterisk Channel Drivers](https://wiki.asterisk.org/wiki/display/AST/Channel+Drivers)
- [Media over QUIC](https://datatracker.ietf.org/wg/moq/about/)
- [WebRTC API](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API)
- [libwebsockets](https://libwebsockets.org/)

## Credits

Created as a modern implementation of VoIP communication using emerging protocols.
