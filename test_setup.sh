#!/bin/bash
# Test script to verify MoQ Phone System setup

echo "========================================"
echo "MoQ Phone System - Setup Verification"
echo "========================================"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check functions
check_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

check_fail() {
    echo -e "${RED}✗${NC} $1"
}

check_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Check Python
echo "Checking Python..."
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    check_pass "Python3 found: $PYTHON_VERSION"
else
    check_fail "Python3 not found"
fi

# Check Python websockets module
echo ""
echo "Checking Python dependencies..."
if python3 -c "import websockets" 2>/dev/null; then
    check_pass "websockets module installed"
else
    check_fail "websockets module not installed (run: pip3 install -r requirements.txt)"
fi

# Check build tools
echo ""
echo "Checking build tools..."
if command -v gcc &> /dev/null; then
    check_pass "GCC compiler found"
else
    check_fail "GCC not found"
fi

if command -v make &> /dev/null; then
    check_pass "Make found"
else
    check_fail "Make not found"
fi

# Check Asterisk
echo ""
echo "Checking Asterisk..."
if command -v asterisk &> /dev/null; then
    ASTERISK_VERSION=$(asterisk -V 2>/dev/null)
    check_pass "Asterisk found: $ASTERISK_VERSION"
    
    # Check if running
    if pgrep asterisk > /dev/null; then
        check_pass "Asterisk is running"
    else
        check_warn "Asterisk is not running"
    fi
else
    check_warn "Asterisk not found (optional - for Asterisk integration)"
fi

# Check Asterisk headers
echo ""
echo "Checking development libraries..."
if [ -d "/usr/include/asterisk" ]; then
    check_pass "Asterisk headers found"
else
    check_warn "Asterisk headers not found (needed for building chan_moq.so)"
fi

# Check for libwebsockets
if ldconfig -p | grep -q libwebsockets; then
    check_pass "libwebsockets found"
else
    check_warn "libwebsockets not found (needed for building chan_moq.so)"
fi

# Check for json-c
if ldconfig -p | grep -q libjson-c; then
    check_pass "libjson-c found"
else
    check_warn "libjson-c not found (needed for building chan_moq.so)"
fi

# Check project files
echo ""
echo "Checking project files..."
if [ -f "chan_moq.c" ]; then
    check_pass "chan_moq.c found"
else
    check_fail "chan_moq.c not found"
fi

if [ -f "Makefile" ]; then
    check_pass "Makefile found"
else
    check_fail "Makefile not found"
fi

if [ -f "signaling_server.py" ]; then
    check_pass "signaling_server.py found"
else
    check_fail "signaling_server.py not found"
fi

if [ -f "webapp/index.html" ]; then
    check_pass "Web app found"
else
    check_fail "Web app not found"
fi

# Check if built
echo ""
echo "Checking build status..."
if [ -f "chan_moq.so" ]; then
    check_pass "chan_moq.so built"
else
    check_warn "chan_moq.so not built yet (run: make)"
fi

# Check ports
echo ""
echo "Checking port availability..."
if ! lsof -i:8088 &> /dev/null; then
    check_pass "Port 8088 available (WebSocket signaling)"
else
    check_warn "Port 8088 already in use"
fi

if ! lsof -i:8000 &> /dev/null; then
    check_pass "Port 8000 available (Web interface)"
else
    check_warn "Port 8000 already in use"
fi

# Summary
echo ""
echo "========================================"
echo "Summary"
echo "========================================"
echo ""

# Determine readiness
READY=true

if ! command -v python3 &> /dev/null; then
    READY=false
fi

if ! python3 -c "import websockets" 2>/dev/null; then
    READY=false
fi

if [ ! -f "signaling_server.py" ] || [ ! -f "webapp/index.html" ]; then
    READY=false
fi

if $READY; then
    echo -e "${GREEN}✓ System is ready to run!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Build Asterisk module: make"
    echo "  2. Install module: sudo make install"
    echo "  3. Start system: ./start.sh"
    echo "  4. Open browser: http://localhost:8000"
else
    echo -e "${RED}✗ System is NOT ready${NC}"
    echo ""
    echo "Please install missing dependencies:"
    echo "  - Python3: sudo apt-get install python3"
    echo "  - websockets: pip3 install -r requirements.txt"
    echo "  - Build tools: sudo apt-get install build-essential"
    echo ""
    echo "For Asterisk module (optional):"
    echo "  - sudo apt-get install asterisk-dev libwebsockets-dev libjson-c-dev"
fi

echo ""
