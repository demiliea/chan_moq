#!/bin/bash
# CI/CD Setup Validation Script
# This script checks if the CI/CD pipeline is properly configured

set -e

echo "=========================================="
echo "CI/CD Setup Validation"
echo "=========================================="
echo ""

ERRORS=0
WARNINGS=0

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

check() {
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $1"
    else
        echo -e "${RED}✗${NC} $1"
        ((ERRORS++))
    fi
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARNINGS++))
}

info() {
    echo -e "${GREEN}ℹ${NC} $1"
}

echo "1. Checking GitHub Actions Workflow Files..."
echo "--------------------------------------------"

if [ -f ".github/workflows/build.yml" ]; then
    check "build.yml exists"
else
    check "build.yml missing"
fi

if [ -f ".github/workflows/ci.yml" ]; then
    check "ci.yml exists"
else
    check "ci.yml missing"
fi

echo ""
echo "2. Checking Documentation..."
echo "--------------------------------------------"

if [ -f ".github/README.md" ]; then
    check ".github/README.md exists"
else
    check ".github/README.md missing"
fi

if [ -f ".github/SETUP.md" ]; then
    check ".github/SETUP.md exists"
else
    warn ".github/SETUP.md missing (optional)"
fi

if [ -f ".github/RELEASE_PROCESS.md" ]; then
    check ".github/RELEASE_PROCESS.md exists"
else
    warn ".github/RELEASE_PROCESS.md missing (optional)"
fi

echo ""
echo "3. Checking Source Files..."
echo "--------------------------------------------"

if [ -f "chan_moq.c" ]; then
    check "chan_moq.c exists"
else
    check "chan_moq.c missing"
fi

if [ -f "Makefile" ]; then
    check "Makefile exists"
else
    check "Makefile missing"
fi

if [ -f "moq.conf" ]; then
    check "moq.conf exists"
else
    warn "moq.conf missing"
fi

echo ""
echo "4. Testing Build System..."
echo "--------------------------------------------"

# Check if make is available
if command -v make &> /dev/null; then
    check "make command available"
    
    # Try to build (if dependencies are installed)
    info "Attempting to build..."
    if make clean &> /dev/null && make &> /dev/null; then
        check "Build successful"
        
        if [ -f "chan_moq.so" ]; then
            check "chan_moq.so created"
            SIZE=$(stat -c%s chan_moq.so 2>/dev/null || stat -f%z chan_moq.so 2>/dev/null)
            info "Build size: $SIZE bytes"
        fi
    else
        warn "Build failed (may need dependencies)"
        info "Run: sudo apt-get install build-essential libwebsockets-dev libjson-c-dev"
    fi
else
    warn "make not available"
fi

echo ""
echo "5. Checking Git Configuration..."
echo "--------------------------------------------"

if [ -d ".git" ]; then
    check "Git repository initialized"
    
    # Check if remote is configured
    if git remote get-url origin &> /dev/null; then
        REMOTE=$(git remote get-url origin)
        check "Git remote configured"
        info "Remote: $REMOTE"
        
        # Extract username and repo from remote URL
        if [[ $REMOTE =~ github.com[:/]([^/]+)/([^/.]+) ]]; then
            USERNAME="${BASH_REMATCH[1]}"
            REPO="${BASH_REMATCH[2]}"
            info "GitHub User: $USERNAME"
            info "Repository: $REPO"
            
            # Check if README needs updating
            if grep -q "YOUR_USERNAME/YOUR_REPO" README.md 2>/dev/null; then
                warn "README.md contains placeholder YOUR_USERNAME/YOUR_REPO"
                info "Update with: sed -i 's/YOUR_USERNAME/$USERNAME/g; s/YOUR_REPO/$REPO/g' README.md"
            else
                check "README.md paths updated"
            fi
        fi
    else
        warn "No git remote configured"
        info "Add remote: git remote add origin https://github.com/USERNAME/REPO.git"
    fi
    
    # Check for unpushed changes
    if git diff-index --quiet HEAD -- 2>/dev/null; then
        check "No uncommitted changes"
    else
        warn "Uncommitted changes detected"
        info "Commit: git add . && git commit -m 'Setup CI/CD'"
    fi
else
    check "Not a git repository"
fi

echo ""
echo "6. Workflow Validation..."
echo "--------------------------------------------"

if [ -f ".github/workflows/build.yml" ]; then
    # Check for common issues in workflow files
    if grep -q "ubuntu-version:" .github/workflows/build.yml; then
        check "Matrix build configured"
    fi
    
    if grep -q "softprops/action-gh-release" .github/workflows/build.yml; then
        check "Release action configured"
    fi
    
    if grep -q "GITHUB_TOKEN" .github/workflows/build.yml; then
        check "GitHub token usage configured"
    fi
fi

echo ""
echo "=========================================="
echo "Validation Summary"
echo "=========================================="

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Push to GitHub: git push origin main"
    echo "2. Check Actions tab on GitHub"
    echo "3. Create a test release: git tag v0.1.0 && git push origin v0.1.0"
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ Validation passed with $WARNINGS warning(s)${NC}"
    echo ""
    echo "CI/CD should work, but review warnings above."
else
    echo -e "${RED}✗ Validation failed with $ERRORS error(s) and $WARNINGS warning(s)${NC}"
    echo ""
    echo "Please fix the errors above before proceeding."
    exit 1
fi

echo ""
echo "For more information, see:"
echo "- .github/README.md - CI/CD overview"
echo "- .github/SETUP.md - Setup instructions"
echo "- .github/RELEASE_PROCESS.md - How to create releases"
echo ""
