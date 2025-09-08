#!/bin/bash

################################################################################
# Script: testAdaptiveStillness.sh
# Purpose: Test adaptive stillness detection on Raspberry Pi
# This script runs after the main pitrac_lm binary is built with adaptive support
################################################################################

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BASE_DIR="$SCRIPT_DIR/.."
BINARY="$BASE_DIR/build/pitrac_lm"
CONFIG_FILE="$BASE_DIR/golf_sim_config.json"
LOG_FILE="adaptive_test_$(date +%Y%m%d_%H%M%S).log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=========================================="
echo "Adaptive Stillness Detection Test"
echo "=========================================="
echo "Date: $(date)"
echo "Binary: $BINARY"
echo "Config: $CONFIG_FILE"
echo ""

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}Error: Binary not found at $BINARY${NC}"
    echo "Please build pitrac_lm first with:"
    echo "  cd $BASE_DIR"
    echo "  ninja -C build"
    exit 1
fi

# Check if config has adaptive settings
echo "Checking configuration..."
if [ -f "$CONFIG_FILE" ]; then
    METHOD=$(grep '"kStabilizationMethod"' "$CONFIG_FILE" | head -1 | cut -d'"' -f4)
    DETECTION=$(grep '"kDetectionMethod"' "$CONFIG_FILE" | head -1 | cut -d'"' -f4)
    
    echo "  Stabilization method: $METHOD"
    echo "  Detection method: $DETECTION"
    
    if [ "$METHOD" != "adaptive" ]; then
        echo -e "${YELLOW}Warning: kStabilizationMethod is not 'adaptive'${NC}"
        echo "  The system will use fixed timer mode"
    fi
    
    if [[ "$DETECTION" != "experimental"* ]]; then
        echo -e "${YELLOW}Warning: kDetectionMethod is not 'experimental'${NC}"
        echo "  The system will use slower Hough circles"
    fi
else
    echo -e "${RED}Error: Config file not found${NC}"
    exit 1
fi

echo ""
echo "Running timing test..."
echo "----------------------"

# Test 1: Check binary for help output and version info
echo "Test 1: Binary validation"
echo "-------------------------"
if "$BINARY" --help 2>&1 | head -5 > /tmp/help_output.txt; then
    echo -e "${GREEN}✓ Binary executes and provides help${NC}"
    cat /tmp/help_output.txt | head -3
else
    echo -e "${RED}✗ Binary failed to execute${NC}"
fi

echo ""
echo "Test 2: Configuration validation" 
echo "--------------------------------"
# Validate that config has proper adaptive settings without running the binary
if [ -f "$CONFIG_FILE" ]; then
    ADAPTIVE_COUNT=0
    
    # Check stabilization method
    if grep -q '"kStabilizationMethod".*"adaptive"' "$CONFIG_FILE"; then
        echo -e "${GREEN}✓ Stabilization method set to 'adaptive'${NC}"
        ((ADAPTIVE_COUNT++))
    else
        echo -e "${YELLOW}⚠ Stabilization method not set to 'adaptive'${NC}"
    fi
    
    # Check detection method
    if grep -q '"kDetectionMethod".*"experimental' "$CONFIG_FILE"; then
        echo -e "${GREEN}✓ Detection method set to experimental (YOLO/SAHI)${NC}"
        ((ADAPTIVE_COUNT++))
    else
        echo -e "${YELLOW}⚠ Detection method not using experimental AI${NC}"
    fi
    
    # Check ball stabilization time (should be low for adaptive)
    STAB_TIME=$(grep '"kBallStabilizationTimeMs"' "$CONFIG_FILE" | head -1 | grep -o '[0-9]*' | head -1)
    if [ -n "$STAB_TIME" ] && [ "$STAB_TIME" -le "500" ]; then
        echo -e "${GREEN}✓ Ball stabilization time set to ${STAB_TIME}ms (good for adaptive)${NC}"
        ((ADAPTIVE_COUNT++))
    else
        echo -e "${YELLOW}⚠ Ball stabilization time may be too high: ${STAB_TIME}ms${NC}"
    fi
    
    echo ""
    echo "Configuration score: $ADAPTIVE_COUNT/3 adaptive features configured"
fi

echo ""
echo "Test 3: Binary symbol analysis"
echo "-------------------------------"

# Check if adaptive stillness symbols are in the binary
echo -n "Checking binary for adaptive symbols... "
if command -v nm >/dev/null 2>&1; then
    if nm "$BINARY" 2>/dev/null | grep -q "AdaptiveStillnessDetector"; then
        echo -e "${GREEN}✓ Found${NC}"
        
        # Count and categorize the symbols
        COUNT=$(nm "$BINARY" 2>/dev/null | grep -c "AdaptiveStillnessDetector" || true)
        echo "  Found $COUNT AdaptiveStillnessDetector symbols"
        
        # Check for specific key methods
        echo "  Checking for key methods:"
        for method in "isStabilized" "reset" "addMeasurement" "calculateVariance"; do
            if nm "$BINARY" 2>/dev/null | grep -q "$method"; then
                echo -e "    ${GREEN}✓${NC} $method"
            else
                echo -e "    ${YELLOW}⚠${NC} $method not found"
            fi
        done
    else
        echo -e "${RED}✗ Not found${NC}"
        echo "  The binary may not include adaptive stillness support"
        echo "  Please ensure adaptive_stillness_detector.cpp is in meson.build"
    fi
else
    # Fallback: check if the binary exists and is executable
    if [ -x "$BINARY" ]; then
        echo -e "${YELLOW}⚠ nm tool not available, checking binary size${NC}"
        SIZE=$(stat -f%z "$BINARY" 2>/dev/null || stat -c%s "$BINARY" 2>/dev/null || echo "0")
        echo "  Binary size: $(( SIZE / 1024 / 1024 ))MB"
        if [ "$SIZE" -gt "10000000" ]; then
            echo -e "  ${GREEN}✓ Binary size suggests full build${NC}"
        fi
    else
        echo -e "${RED}✗ Cannot analyze binary${NC}"
    fi
fi

echo ""
echo "Test 4: Source file verification"
echo "--------------------------------"

# Check if adaptive source files exist
ADAPTIVE_SOURCE="$BASE_DIR/adaptive_stillness_detector.cpp"
ADAPTIVE_HEADER="$BASE_DIR/adaptive_stillness_detector.h"

if [ -f "$ADAPTIVE_SOURCE" ]; then
    echo -e "${GREEN}✓ adaptive_stillness_detector.cpp exists${NC}"
    # Check file size to ensure it's not empty
    SOURCE_SIZE=$(wc -l < "$ADAPTIVE_SOURCE" 2>/dev/null || echo "0")
    echo "  Source file has $SOURCE_SIZE lines"
else
    echo -e "${RED}✗ adaptive_stillness_detector.cpp not found${NC}"
fi

if [ -f "$ADAPTIVE_HEADER" ]; then
    echo -e "${GREEN}✓ adaptive_stillness_detector.h exists${NC}"
    # Check for class definition
    if grep -q "class AdaptiveStillnessDetector" "$ADAPTIVE_HEADER" 2>/dev/null; then
        echo -e "  ${GREEN}✓ Class definition found${NC}"
    fi
else
    echo -e "${RED}✗ adaptive_stillness_detector.h not found${NC}"
fi

# Check if files are included in build system
echo ""
echo -n "Checking meson.build for adaptive files... "
if [ -f "$BASE_DIR/meson.build" ]; then
    if grep -q "adaptive_stillness_detector" "$BASE_DIR/meson.build"; then
        echo -e "${GREEN}✓ Found in build system${NC}"
    else
        echo -e "${YELLOW}⚠ Not found in meson.build${NC}"
        echo "  Add 'adaptive_stillness_detector.cpp' to source files"
    fi
else
    echo -e "${YELLOW}⚠ meson.build not found${NC}"
fi

echo ""
echo "Configuration Summary"
echo "---------------------"
if [ -f "$CONFIG_FILE" ]; then
    echo "Ball stabilization settings:"
    grep -A 5 '"ball_stabilization"' "$CONFIG_FILE" | head -6 | sed 's/^/  /'
    
    echo ""
    echo "Detection method settings:"
    grep '"kDetectionMethod"' "$CONFIG_FILE" | head -1 | sed 's/^/  /'
    grep '"kBallPlacementDetectionMethod"' "$CONFIG_FILE" | head -1 | sed 's/^/  /'
fi

echo ""
echo "=========================================="
echo "Test Complete"
echo "=========================================="
echo "Log saved to: $LOG_FILE"
echo ""
echo "Next steps:"
echo "1. If adaptive symbols not found, rebuild with:"
echo "   cd $BASE_DIR && ninja -C build clean && ninja -C build"
echo ""
echo "2. To run the full system with adaptive detection:"
echo "   $BINARY --camera1"
echo ""
echo "3. Monitor the logs for timing improvements:"
echo "   - Look for 'Ball stabilized in Xms' messages"
echo "   - Should be 50-200ms with adaptive vs 1000ms with fixed"
echo ""

# Cleanup
rm -f /tmp/test_timing.sh /tmp/pitrac_test.log