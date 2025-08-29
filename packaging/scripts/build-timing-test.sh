#!/bin/bash
# Build script for testing ball stabilization timing improvements
# This script builds PiTrac with the optimized timing parameters

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGING_DIR="$(dirname "${SCRIPT_DIR}")"
PROJECT_ROOT="$(dirname "${PACKAGING_DIR}")"

echo "========================================"
echo "PiTrac Timing Optimization Test Build"
echo "========================================"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -f "${PROJECT_ROOT}/Software/LMSourceCode/ImageProcessing/meson.build" ]; then
    print_error "Cannot find PiTrac source files. Are you in the right directory?"
    exit 1
fi

# Build the application
print_info "Building PiTrac with timing optimizations..."
cd "${PROJECT_ROOT}/Software/LMSourceCode/ImageProcessing"

# Clean previous build if requested
if [ "$1" == "clean" ]; then
    print_info "Cleaning previous build..."
    rm -rf build
fi

# Setup build with meson
if [ ! -d "build" ]; then
    print_info "Setting up build directory..."
    meson setup build --buildtype=release
fi

# Build the binary
print_info "Compiling pitrac_lm..."
ninja -C build pitrac_lm

if [ ! -f "build/pitrac_lm" ]; then
    print_error "Build failed - pitrac_lm binary not found"
    exit 1
fi

print_info "Build successful!"
echo

# Create test configuration with optimized timing
print_info "Creating test configuration..."
TEST_CONFIG="${PROJECT_ROOT}/test_timing_config.json"

# Copy the base config and update timing values
if [ -f "golf_sim_config.json" ]; then
    cp golf_sim_config.json "${TEST_CONFIG}"
    print_info "Test config created at: ${TEST_CONFIG}"
else
    print_warn "Base config not found, will rely on defaults"
fi

echo
echo "========================================"
echo "Build Complete - Testing Instructions"
echo "========================================"
echo
echo "1. DEFAULT TIMING TEST (should be slow ~30s):"
echo "   cd ${PROJECT_ROOT}/Software/LMSourceCode/ImageProcessing"
echo "   ./build/pitrac_lm --system_mode=putting_green"
echo
echo "2. OPTIMIZED TIMING TEST (should be fast ~5s):"
echo "   ./build/pitrac_lm --system_mode=putting_green \\"
echo "       --ball_poll_ms=100 \\"
echo "       --ball_stabilization_time=1 \\"
echo "       --parallel_camera_setup=true \\"
echo "       --ball_movement_tolerance=15 \\"
echo "       --camera2_setup_ms=1500 \\"
echo "       --priming_pulses=8 \\"
echo "       --priming_fps=20"
echo
echo "3. TEST WITH YAML CONFIG:"
echo "   # Edit /etc/pitrac/pitrac.yaml to uncomment timing settings"
echo "   sudo nano /etc/pitrac/pitrac.yaml"
echo "   # Then run:"
echo "   ./build/pitrac_lm --system_mode=putting_green"
echo
echo "4. TIMING COMPARISON SCRIPT:"
echo "   ${SCRIPT_DIR}/test-timing-comparison.sh"
echo
echo "========================================"
echo "Expected Results:"
echo "- Default: Green bar appears after ~30 seconds"
echo "- Optimized: Green bar appears after ~5 seconds"
echo "========================================"

# Make timing comparison script
cat > "${SCRIPT_DIR}/test-timing-comparison.sh" << 'EOF'
#!/bin/bash
# Script to compare timing before and after optimization

BINARY="${PROJECT_ROOT}/Software/LMSourceCode/ImageProcessing/build/pitrac_lm"

echo "Starting timing comparison test..."
echo "================================"

# Test 1: Default timing
echo "Test 1: Default timing (expected ~30s)"
echo "Place ball and measure time to green bar..."
timeout 60 "${BINARY}" --system_mode=putting_green --logging_level=info 2>&1 | grep -E "(green|stabilized|ready)" &
PID1=$!
echo "Press ENTER when green bar appears..."
read
kill $PID1 2>/dev/null
echo

# Test 2: Optimized timing
echo "Test 2: Optimized timing (expected ~5s)"
echo "Place ball and measure time to green bar..."
timeout 60 "${BINARY}" --system_mode=putting_green \
    --ball_poll_ms=100 \
    --ball_stabilization_time=1 \
    --parallel_camera_setup=true \
    --ball_movement_tolerance=15 \
    --camera2_setup_ms=1500 \
    --priming_pulses=8 \
    --priming_fps=20 \
    --logging_level=info 2>&1 | grep -E "(green|stabilized|ready)" &
PID2=$!
echo "Press ENTER when green bar appears..."
read
kill $PID2 2>/dev/null

echo
echo "Test complete!"
echo "If optimization worked correctly:"
echo "- Test 1 should have taken ~30 seconds"
echo "- Test 2 should have taken ~5 seconds"
EOF

chmod +x "${SCRIPT_DIR}/test-timing-comparison.sh"
print_info "Created timing comparison script"

# Create a debug script for detailed logging
cat > "${SCRIPT_DIR}/debug-timing.sh" << 'EOF'
#!/bin/bash
# Debug script to see detailed timing logs

BINARY="${PROJECT_ROOT}/Software/LMSourceCode/ImageProcessing/build/pitrac_lm"

echo "Running with debug logging to see all timing details..."
echo "======================================================="

"${BINARY}" --system_mode=putting_green \
    --ball_poll_ms=100 \
    --ball_stabilization_time=1 \
    --parallel_camera_setup=true \
    --ball_movement_tolerance=15 \
    --camera2_setup_ms=1500 \
    --priming_pulses=8 \
    --priming_fps=20 \
    --logging_level=debug 2>&1 | tee timing_debug.log

echo
echo "Debug log saved to: timing_debug.log"
echo "Look for these key messages:"
echo "  - 'CLI override:' messages showing parameter overrides"
echo "  - 'FSM Timing Configuration:' showing active settings"
echo "  - 'Parallel camera setup enabled' for parallel init"
echo "  - Time stamps between ball detection and green bar"
EOF

chmod +x "${SCRIPT_DIR}/debug-timing.sh"
print_info "Created debug timing script"

echo
print_info "All test scripts created successfully!"
print_info "Start with the timing comparison test to verify the optimization"