#!/bin/bash
# Complete setup script for MoQ Channel Driver

set -e

echo "============================================"
echo "MoQ Channel Driver - Complete Setup"
echo "============================================"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# Detect OS
print_status "Detecting operating system..."
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    print_success "Detected: $PRETTY_NAME"
else
    print_error "Cannot detect OS"
    exit 1
fi

# Install system dependencies
print_status "Installing system dependencies..."

case "$OS" in
    ubuntu|debian)
        print_status "Installing packages for Ubuntu/Debian..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            gcc \
            make \
            python3 \
            python3-pip \
            libwebsockets-dev \
            libjson-c-dev \
            pkg-config
        
        # Asterisk (optional)
        if ! command -v asterisk &> /dev/null; then
            print_warning "Asterisk not found. Install? (y/n)"
            read -r install_asterisk
            if [ "$install_asterisk" = "y" ]; then
                sudo apt-get install -y asterisk asterisk-dev
            fi
        fi
        ;;
        
    centos|rhel|fedora)
        print_status "Installing packages for CentOS/RHEL/Fedora..."
        sudo yum install -y \
            gcc \
            make \
            python3 \
            python3-pip \
            libwebsockets-devel \
            json-c-devel \
            pkg-config
        ;;
        
    *)
        print_warning "Unsupported OS: $OS"
        print_warning "Please install dependencies manually:"
        print_warning "  - gcc, make, python3"
        print_warning "  - libwebsockets-dev, libjson-c-dev"
        print_warning "  - asterisk-dev (optional)"
        ;;
esac

print_success "System dependencies installed"

# Install Python dependencies
print_status "Installing Python dependencies..."
pip3 install --user -r requirements.txt
print_success "Python dependencies installed"

# Build chan_moq.so
print_status "Building chan_moq.so..."
make clean
make

if [ -f chan_moq.so ]; then
    print_success "chan_moq.so built successfully"
else
    print_error "Failed to build chan_moq.so"
    exit 1
fi

# Check if should install
if command -v asterisk &> /dev/null; then
    print_status "Asterisk found. Install module? (y/n)"
    read -r install_module
    if [ "$install_module" = "y" ]; then
        print_status "Installing chan_moq.so..."
        sudo make install
        print_success "Module installed to Asterisk"
        
        print_status "Load module now? (y/n)"
        read -r load_module
        if [ "$load_module" = "y" ]; then
            sudo asterisk -rx "module load chan_moq.so"
            print_success "Module loaded in Asterisk"
        fi
    fi
else
    print_warning "Asterisk not installed - skipping module installation"
    print_warning "The module is built and ready for manual installation"
fi

# Configure
print_status "Setting up configuration..."
if [ -d /etc/asterisk ] && [ ! -f /etc/asterisk/moq.conf ]; then
    sudo cp moq.conf /etc/asterisk/
    print_success "Configuration installed"
fi

# Make scripts executable
print_status "Setting up utility scripts..."
chmod +x start.sh stop.sh test_setup.sh 2>/dev/null || true
print_success "Scripts ready"

echo ""
echo "============================================"
echo "Setup Complete!"
echo "============================================"
echo ""
print_success "MoQ Channel Driver is ready to use"
echo ""
echo "Next steps:"
echo "  1. Start services:    ./start.sh"
echo "  2. Open browser:      http://localhost:8000"
echo "  3. View logs:         tail -f /var/log/asterisk/messages"
echo ""
echo "For Asterisk integration:"
echo "  - Edit /etc/asterisk/extensions.conf"
echo "  - Add: exten => _X.,1,Dial(MOQ/\${EXTEN})"
echo ""

print_success "Setup complete!"
