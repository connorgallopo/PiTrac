#!/bin/bash

################################################################################
# Script: runAdaptiveStillnessTest.sh
# Purpose: Test adaptive stillness detection on Raspberry Pi with timing validation
# Usage: ./runAdaptiveStillnessTest.sh [options]
# Options:
#   --verbose    Enable verbose output
#   --benchmark  Run extended performance benchmarks
#   --images     Path to test images directory
#   --config     Path to custom config file
################################################################################

# Set script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BASE_DIR="$SCRIPT_DIR/.."

# Default values
VERBOSE=""
BENCHMARK=""
IMAGE_DIR="$BASE_DIR/TestImages/TeedBalls"
CONFIG_FILE="$BASE_DIR/golf_sim_config.json"
BUILD_DIR="$BASE_DIR/build"
LOG_FILE="adaptive_stillness_test_$(date +%Y%m%d_%H%M%S).log"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE="--verbose"
            shift
            ;;
        --benchmark)
            BENCHMARK="--benchmark"
            shift
            ;;
        --images)
            IMAGE_DIR="$2"
            shift 2
            ;;
        --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --verbose, -v    Enable verbose output"
            echo "  --benchmark      Run extended performance benchmarks"
            echo "  --images <dir>   Directory containing test ball images"
            echo "  --config <file>  Path to config file"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "Adaptive Stillness Detection Test"
echo "=========================================="
echo "Date: $(date)"
echo "Platform: $(uname -m)"
echo "Config: $CONFIG_FILE"
echo "Images: $IMAGE_DIR"
echo "Log: $LOG_FILE"
echo ""

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Warning: Config file not found: $CONFIG_FILE"
    echo "Using default configuration"
fi

# Check if test image directory exists
if [ ! -d "$IMAGE_DIR" ]; then
    echo "Warning: Test image directory not found: $IMAGE_DIR"
    echo "Tests will run without real images"
fi

# Build the test if needed
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Check if we need to rebuild
if [ ! -f "test_adaptive_stillness" ] || [ "$BASE_DIR/test_adaptive_stillness.cpp" -nt "test_adaptive_stillness" ]; then
    echo "Building test executable..."
    
    # Compile test program
    g++ -std=c++17 \
        -I"$BASE_DIR" \
        -I/usr/include/opencv4 \
        -I/usr/local/include \
        -O2 -g \
        "$BASE_DIR/test_adaptive_stillness.cpp" \
        "$BASE_DIR/adaptive_stillness_detector.cpp" \
        "$BASE_DIR/golf_ball.cpp" \
        "$BASE_DIR/gs_config.cpp" \
        "$BASE_DIR/logging_tools.cpp" \
        "$BASE_DIR/gs_automated_testing.cpp" \
        "$BASE_DIR/ball_image_proc.cpp" \
        -o test_adaptive_stillness \
        -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui \
        -lboost_system -lboost_filesystem -lboost_timer \
        -lpthread
    
    if [ $? -ne 0 ]; then
        echo "Build failed!"
        exit 1
    fi
    echo "Build successful!"
else
    echo "Using existing test executable"
fi

# Set environment variables
export PITRAC_CONFIG="$CONFIG_FILE"
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Run the test
echo ""
echo "Running tests..."
echo "----------------"

# Capture both stdout and stderr to log file
./test_adaptive_stillness --images "$IMAGE_DIR" $VERBOSE $BENCHMARK 2>&1 | tee "$LOG_FILE"

TEST_RESULT=${PIPESTATUS[0]}

echo ""
echo "=========================================="
echo "Test Results Summary"
echo "=========================================="

# Extract summary from log
grep -A 5 "Test Summary:" "$LOG_FILE"

if [ $TEST_RESULT -eq 0 ]; then
    echo ""
    echo "✓ All tests PASSED!"
    echo ""
    
    # Show performance metrics
    echo "Performance Metrics:"
    echo "--------------------"
    grep "Single call time:" "$LOG_FILE" | tail -1
    grep "Full detection time:" "$LOG_FILE" | tail -1
    grep "Adaptive:" "$LOG_FILE" | grep -A 1 "Fixed:" | head -2
else
    echo ""
    echo "✗ Some tests FAILED!"
    echo ""
    echo "Failed tests:"
    grep "\[FAIL\]" "$LOG_FILE"
fi

echo ""
echo "Full log saved to: $LOG_FILE"
echo ""

# If running on Pi, show system info
if [ -f /proc/device-tree/model ]; then
    echo "System Information:"
    echo "-------------------"
    cat /proc/device-tree/model
    echo ""
    echo "CPU: $(lscpu | grep "Model name" | cut -d: -f2 | xargs)"
    echo "Memory: $(free -h | grep "^Mem:" | awk '{print $2}')"
    echo "Temperature: $(vcgencmd measure_temp 2>/dev/null || echo "N/A")"
fi

exit $TEST_RESULT