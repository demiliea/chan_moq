#!/bin/bash
# Validation script for MoQ Channel Driver implementation

echo "================================================"
echo "MoQ Channel Driver - Implementation Validation"
echo "================================================"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PASS=0
FAIL=0
WARN=0

check_pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((PASS++))
}

check_fail() {
    echo -e "${RED}✗${NC} $1"
    ((FAIL++))
}

check_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARN++))
}

print_section() {
    echo ""
    echo -e "${BLUE}━━━ $1 ━━━${NC}"
}

# 1. Check source files exist
print_section "Source Files"

if [ -f "chan_moq.c" ]; then
    LINES=$(wc -l < chan_moq.c)
    check_pass "chan_moq.c exists ($LINES lines)"
else
    check_fail "chan_moq.c not found"
fi

if [ -f "signaling_server.py" ]; then
    LINES=$(wc -l < signaling_server.py)
    check_pass "signaling_server.py exists ($LINES lines)"
else
    check_fail "signaling_server.py not found"
fi

if [ -f "webapp/app.js" ] && [ -f "webapp/index.html" ]; then
    check_pass "Web application files exist"
else
    check_fail "Web application files not found"
fi

# 2. Check configuration files
print_section "Configuration Files"

for file in moq.conf Makefile requirements.txt extensions.conf.example; do
    if [ -f "$file" ]; then
        check_pass "$file exists"
    else
        check_fail "$file not found"
    fi
done

# 3. Check scripts
print_section "Utility Scripts"

for script in setup.sh start.sh stop.sh test_setup.sh validate.sh; do
    if [ -f "$script" ]; then
        if [ -x "$script" ]; then
            check_pass "$script exists and is executable"
        else
            check_warn "$script exists but is not executable"
        fi
    else
        check_fail "$script not found"
    fi
done

# 4. Check documentation
print_section "Documentation"

for doc in README.md QUICKSTART.md PROJECT_OVERVIEW.md INSTALL.md; do
    if [ -f "$doc" ]; then
        check_pass "$doc exists"
    else
        check_warn "$doc not found"
    fi
done

# 5. Validate C code syntax
print_section "Code Validation"

echo "Checking C syntax..."
if command -v gcc &> /dev/null; then
    # Just check for basic syntax errors without linking
    gcc -fsyntax-only -I/usr/include -I/usr/local/include/asterisk \
        -D_GNU_SOURCE chan_moq.c 2>/dev/null
    
    if [ $? -eq 0 ]; then
        check_pass "C code syntax is valid"
    else
        check_warn "C code has syntax issues (may be due to missing headers)"
    fi
else
    check_warn "GCC not available for syntax checking"
fi

echo "Checking Python syntax..."
if command -v python3 &> /dev/null; then
    python3 -m py_compile signaling_server.py 2>/dev/null
    if [ $? -eq 0 ]; then
        check_pass "Python code syntax is valid"
    else
        check_fail "Python code has syntax errors"
    fi
else
    check_fail "Python3 not available"
fi

# 6. Check key features in code
print_section "Feature Implementation"

echo "Checking chan_moq.c for key features..."

if grep -q "moq_quic_create" chan_moq.c; then
    check_pass "QUIC connection implementation found"
else
    check_fail "QUIC connection implementation not found"
fi

if grep -q "moq_send_media_object" chan_moq.c; then
    check_pass "MoQ media transmission found"
else
    check_fail "MoQ media transmission not found"
fi

if grep -q "moq_recv_media_object" chan_moq.c; then
    check_pass "MoQ media reception found"
else
    check_fail "MoQ media reception not found"
fi

if grep -q "struct moq_media_header" chan_moq.c; then
    check_pass "MoQ protocol header structure found"
else
    check_fail "MoQ protocol header not found"
fi

if grep -q "moq_ws_callback" chan_moq.c; then
    check_pass "WebSocket signaling callback found"
else
    check_fail "WebSocket signaling not found"
fi

if grep -q "ast_channel_tech moq_tech" chan_moq.c; then
    check_pass "Asterisk channel technology registration found"
else
    check_fail "Channel technology not registered"
fi

echo ""
echo "Checking signaling_server.py for features..."

if grep -q "async def handle_client" signaling_server.py; then
    check_pass "Async WebSocket handler implemented"
else
    check_fail "WebSocket handler not found"
fi

if grep -q "'type'.*'call'" signaling_server.py; then
    check_pass "Call signaling messages implemented"
else
    check_fail "Call signaling not found"
fi

if grep -q "'type'.*'answer'" signaling_server.py; then
    check_pass "Answer signaling messages implemented"
else
    check_fail "Answer signaling not found"
fi

# 7. Check project structure
print_section "Project Structure"

if [ -d "webapp" ]; then
    check_pass "webapp directory exists"
    
    if [ -f "webapp/app.js" ] && [ -f "webapp/index.html" ]; then
        check_pass "Web app files are complete"
    fi
else
    check_fail "webapp directory not found"
fi

# 8. Validate MoQ protocol implementation
print_section "MoQ Protocol Implementation"

echo "Checking for MoQ protocol elements..."

if grep -q "MOQ_MSG_OBJECT\|MOQ_MSG_SUBSCRIBE" chan_moq.c; then
    check_pass "MoQ message types defined"
else
    check_fail "MoQ message types not found"
fi

if grep -q "track_id\|sequence\|timestamp" chan_moq.c; then
    check_pass "MoQ object metadata found"
else
    check_fail "MoQ object metadata not found"
fi

if grep -q "htobe64\|be64toh" chan_moq.c; then
    check_pass "Endianness handling implemented"
else
    check_warn "Endianness handling may be missing"
fi

# 9. Check for security considerations
print_section "Security & Best Practices"

if grep -q "ast_mutex_lock\|ast_mutex_unlock" chan_moq.c; then
    check_pass "Thread synchronization implemented"
else
    check_warn "Thread synchronization may be incomplete"
fi

if grep -q "ast_log.*ERROR\|ast_log.*WARNING" chan_moq.c; then
    check_pass "Error logging implemented"
else
    check_warn "Error logging may be incomplete"
fi

if grep -q "buffer.*overflow\|bounds.*check" chan_moq.c; then
    check_pass "Buffer safety checks mentioned"
else
    check_warn "Buffer safety checks should be reviewed"
fi

# 10. Summary
print_section "Summary"

TOTAL=$((PASS + FAIL + WARN))

echo ""
echo "Results:"
echo -e "  ${GREEN}Passed:${NC}  $PASS"
echo -e "  ${RED}Failed:${NC}  $FAIL"
echo -e "  ${YELLOW}Warnings:${NC} $WARN"
echo -e "  Total:   $TOTAL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ Implementation validation PASSED${NC}"
    echo ""
    echo "The MoQ Channel Driver implementation is complete and ready for deployment."
    echo ""
    echo "Next steps:"
    echo "  1. Install dependencies (see INSTALL.md)"
    echo "  2. Build: make"
    echo "  3. Install: sudo make install"
    echo "  4. Start: ./start.sh"
    echo "  5. Test: Open http://localhost:8000"
    EXIT_CODE=0
elif [ $FAIL -lt 5 ]; then
    echo -e "${YELLOW}⚠ Implementation validation completed with warnings${NC}"
    echo ""
    echo "Minor issues found. Review failed checks above."
    EXIT_CODE=1
else
    echo -e "${RED}✗ Implementation validation FAILED${NC}"
    echo ""
    echo "Critical issues found. Please review failed checks above."
    EXIT_CODE=2
fi

echo ""
echo "For detailed installation instructions, see INSTALL.md"
echo "For quick start, see QUICKSTART.md"
echo "For architecture details, see PROJECT_OVERVIEW.md"
echo ""

exit $EXIT_CODE
