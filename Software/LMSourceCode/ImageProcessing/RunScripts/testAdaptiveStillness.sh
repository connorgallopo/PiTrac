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

# Create a test script that measures startup and detection timing
cat > /tmp/test_timing.sh << 'EOF'
#!/bin/bash
START=$(date +%s%N)

# Run pitrac_lm in test mode for a short duration
timeout 5 "$1" --camera1-test 2>&1 | tee /tmp/pitrac_test.log &
PID=$!

# Wait a moment for initialization
sleep 2

# Check if process is running
if ps -p $PID > /dev/null; then
    echo "Process started successfully"
    kill $PID 2>/dev/null
else
    echo "Process failed to start"
fi

END=$(date +%s%N)
ELAPSED=$((($END - $START) / 1000000))
echo "Test duration: ${ELAPSED}ms"

# Check for adaptive stillness messages in log
if grep -q "AdaptiveStillnessDetector" /tmp/pitrac_test.log; then
    echo -e "\033[0;32m✓ Adaptive stillness detector initialized\033[0m"
else
    echo -e "\033[1;33m⚠ No adaptive stillness messages found\033[0m"
fi

if grep -q "Using adaptive ball stabilization" /tmp/pitrac_test.log; then
    echo -e "\033[0;32m✓ System using adaptive mode\033[0m"
else
    if grep -q "Using fixed timer ball stabilization" /tmp/pitrac_test.log; then
        echo -e "\033[1;33m⚠ System using fixed timer mode\033[0m"
    fi
fi

# Check for YOLO detection
if grep -q "Using ONNX detection\|YOLO" /tmp/pitrac_test.log; then
    echo -e "\033[0;32m✓ YOLO/ONNX detection enabled\033[0m"
else
    echo -e "\033[1;33m⚠ Using legacy Hough circles detection\033[0m"
fi
EOF

chmod +x /tmp/test_timing.sh
/tmp/test_timing.sh "$BINARY" | tee -a "$LOG_FILE"

echo ""
echo "Checking for adaptive components..."
echo "------------------------------------"

# Check if adaptive stillness symbols are in the binary
echo -n "Checking binary for adaptive symbols... "
if nm "$BINARY" 2>/dev/null | grep -q "AdaptiveStillnessDetector"; then
    echo -e "${GREEN}✓ Found${NC}"
    
    # Count the symbols
    COUNT=$(nm "$BINARY" 2>/dev/null | grep -c "AdaptiveStillnessDetector" || true)
    echo "  Found $COUNT AdaptiveStillnessDetector symbols"
else
    echo -e "${RED}✗ Not found${NC}"
    echo "  The binary may not include adaptive stillness support"
    echo "  Please ensure adaptive_stillness_detector.cpp is in meson.build"
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