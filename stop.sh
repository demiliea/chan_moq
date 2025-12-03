#!/bin/bash
# Stop script for MoQ system

echo "Stopping MoQ Phone System..."

# Stop signaling server
if pgrep -f "signaling_server.py" > /dev/null; then
    echo "Stopping signaling server..."
    pkill -f "signaling_server.py"
    echo "Signaling server stopped"
else
    echo "Signaling server not running"
fi

# Stop web server
if pgrep -f "http.server 8000" > /dev/null; then
    echo "Stopping web server..."
    pkill -f "http.server 8000"
    echo "Web server stopped"
else
    echo "Web server not running"
fi

echo "MoQ Phone System stopped"
