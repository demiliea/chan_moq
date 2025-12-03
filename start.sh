#!/bin/bash
# Startup script for MoQ system

echo "Starting MoQ Phone System..."

# Check if signaling server is running
if pgrep -f "signaling_server.py" > /dev/null; then
    echo "Signaling server already running"
else
    echo "Starting signaling server..."
    python3 signaling_server.py &
    SIGNALING_PID=$!
    echo "Signaling server started (PID: $SIGNALING_PID)"
fi

# Check if web server is running
if pgrep -f "http.server" > /dev/null; then
    echo "Web server already running"
else
    echo "Starting web server on port 8000..."
    cd webapp
    python3 -m http.server 8000 &
    WEB_PID=$!
    cd ..
    echo "Web server started (PID: $WEB_PID)"
fi

echo ""
echo "MoQ Phone System is running!"
echo "================================"
echo "Signaling Server: ws://localhost:8088"
echo "Web Interface: http://localhost:8000"
echo ""
echo "To stop the servers, run: ./stop.sh"
