/*****************************************************************//**
 * \file   test_adaptive_stillness.cpp
 * \brief  Test suite for AdaptiveStillnessDetector with timing validation
 *         Can be run on Raspberry Pi with test images to validate performance
 * 
 * \author PiTrac
 * \date   December 2024
 *********************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024-2025, Verdant Consultants, LLC.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <fstream>
#include <iomanip>

#include <opencv2/opencv.hpp>
#include <boost/filesystem.hpp>
#include <boost/timer/timer.hpp>

#include "adaptive_stillness_detector.h"
#include "golf_ball.h"
#include "gs_config.h"
#include "logging_tools.h"
#include "gs_automated_testing.h"
#include "libcamera_interface.h"
#include "ball_image_proc.h"

namespace fs = boost::filesystem;
using namespace golf_sim;

/**
 * @brief Test harness for AdaptiveStillnessDetector
 * 
 * Provides comprehensive testing including:
 * - Unit tests for detector logic
 * - Performance benchmarking on Pi
 * - Real image testing with teed ball images
 * - Comparison with legacy timer method
 */
class AdaptiveStillnessTestSuite {
public:
    AdaptiveStillnessTestSuite() : 
        test_passed_(0), 
        test_failed_(0),
        verbose_(false) {}
    
    /**
     * @brief Run all tests in the suite
     * @param test_image_dir Directory containing test ball images
     * @param verbose Enable detailed output
     * @return true if all tests pass
     */
    bool runAllTests(const std::string& test_image_dir, bool verbose = false) {
        verbose_ = verbose;
        
        std::cout << "\n========================================\n";
        std::cout << "Adaptive Stillness Detection Test Suite\n";
        std::cout << "========================================\n\n";
        
        // Configure detector with test parameters
        configureForTesting();
        
        // Run test categories
        bool all_passed = true;
        
        all_passed &= runUnitTests();
        all_passed &= runPerformanceTests();
        all_passed &= runImageBasedTests(test_image_dir);
        all_passed &= runTimingValidationTests();
        all_passed &= runComparisonTests();
        
        // Print summary
        printTestSummary();
        
        return all_passed;
    }
    
private:
    int test_passed_;
    int test_failed_;
    bool verbose_;
    
    void configureForTesting() {
        // Set test configuration
        AdaptiveStillnessDetector::kStabilizationMethod = "adaptive";
        AdaptiveStillnessDetector::kAdaptiveMinFrames = 6;
        AdaptiveStillnessDetector::kAdaptiveMaxFrames = 30;
        AdaptiveStillnessDetector::kAdaptivePositionThreshold = 0.5;
        AdaptiveStillnessDetector::kAdaptiveRadiusThreshold = 0.02;
        AdaptiveStillnessDetector::kAdaptiveMotionThreshold = 0.1;
        AdaptiveStillnessDetector::kAdaptiveDebugLogging = verbose_;
        AdaptiveStillnessDetector::Configure();
    }
    
    /**
     * @brief Unit tests for detector logic
     */
    bool runUnitTests() {
        std::cout << "Running Unit Tests...\n";
        std::cout << "--------------------\n";
        
        bool all_passed = true;
        
        // Test 1: Detector initializes properly
        {
            AdaptiveStillnessDetector detector;
            bool passed = (detector.getFrameCount() == 0);
            reportTest("Detector initialization", passed);
            all_passed &= passed;
        }
        
        // Test 2: Reset clears state
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            detector.isBallStill(ball);
            detector.reset();
            bool passed = (detector.getFrameCount() == 0);
            reportTest("Reset functionality", passed);
            all_passed &= passed;
        }
        
        // Test 3: Requires minimum frames
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            // Add fewer than minimum frames
            for (int i = 0; i < AdaptiveStillnessDetector::kAdaptiveMinFrames - 1; i++) {
                bool still = detector.isBallStill(ball);
                if (still) {
                    reportTest("Minimum frames requirement", false);
                    all_passed = false;
                    break;
                }
            }
            if (detector.getFrameCount() == AdaptiveStillnessDetector::kAdaptiveMinFrames - 1) {
                reportTest("Minimum frames requirement", true);
            }
        }
        
        // Test 4: Detects perfectly still ball
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            bool detected = false;
            for (int i = 0; i < AdaptiveStillnessDetector::kAdaptiveMinFrames + 2; i++) {
                if (detector.isBallStill(ball)) {
                    detected = true;
                    break;
                }
            }
            reportTest("Still ball detection", detected);
            all_passed &= detected;
        }
        
        // Test 5: Detects moving ball
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.measured_radius_pixels_ = 50;
            
            bool still = false;
            for (int i = 0; i < AdaptiveStillnessDetector::kAdaptiveMinFrames + 5; i++) {
                // Move ball significantly each frame
                ball.center_ = cv::Point2d(100 + i * 5, 100);
                still = detector.isBallStill(ball);
            }
            reportTest("Moving ball rejection", !still);
            all_passed &= !still;
        }
        
        // Test 6: Detects settling ball
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.measured_radius_pixels_ = 50;
            
            // Simulate ball settling (decreasing motion)
            std::vector<double> motion_amounts = {10, 5, 2, 1, 0.3, 0.1, 0.05, 0.02, 0.01};
            double x = 100;
            
            bool detected = false;
            int detection_frame = -1;
            
            for (size_t i = 0; i < motion_amounts.size(); i++) {
                x += motion_amounts[i];
                ball.center_ = cv::Point2d(x, 100);
                
                if (detector.isBallStill(ball)) {
                    detected = true;
                    detection_frame = i;
                    break;
                }
            }
            
            if (verbose_ && detected) {
                std::cout << "  Settling detected at frame " << detection_frame << "\n";
            }
            
            reportTest("Settling ball detection", detected);
            all_passed &= detected;
        }
        
        std::cout << "\n";
        return all_passed;
    }
    
    /**
     * @brief Performance benchmarking tests
     */
    bool runPerformanceTests() {
        std::cout << "Running Performance Tests...\n";
        std::cout << "---------------------------\n";
        
        bool all_passed = true;
        
        // Test 1: Single detection call performance
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            // Warm up
            for (int i = 0; i < 10; i++) {
                detector.isBallStill(ball);
            }
            detector.reset();
            
            // Measure single call
            auto start = std::chrono::high_resolution_clock::now();
            detector.isBallStill(ball);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            bool passed = (duration_us < 1000); // Should be < 1ms
            if (verbose_) {
                std::cout << "  Single call time: " << duration_us << " microseconds\n";
            }
            reportTest("Single call performance (<1ms)", passed);
            all_passed &= passed;
        }
        
        // Test 2: Full detection sequence performance
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            bool detected = false;
            for (int i = 0; i < 20; i++) {
                if (detector.isBallStill(ball)) {
                    detected = true;
                    break;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            bool passed = detected && (duration_ms < 10); // Should detect in <10ms
            if (verbose_) {
                std::cout << "  Full detection time: " << duration_ms << " ms\n";
                std::cout << "  Detection at frame: " << detector.getFrameCount() << "\n";
            }
            reportTest("Full sequence performance (<10ms)", passed);
            all_passed &= passed;
        }
        
        // Test 3: Memory usage (buffer management)
        {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            // Fill buffer beyond max
            for (int i = 0; i < AdaptiveStillnessDetector::kAdaptiveMaxFrames * 2; i++) {
                detector.isBallStill(ball);
            }
            
            // Buffer should be capped at max frames
            bool passed = (detector.getFrameCount() <= AdaptiveStillnessDetector::kAdaptiveMaxFrames);
            reportTest("Buffer size management", passed);
            all_passed &= passed;
        }
        
        std::cout << "\n";
        return all_passed;
    }
    
    /**
     * @brief Test with real teed ball images
     */
    bool runImageBasedTests(const std::string& test_image_dir) {
        std::cout << "Running Image-Based Tests...\n";
        std::cout << "----------------------------\n";
        
        if (test_image_dir.empty() || !fs::exists(test_image_dir)) {
            std::cout << "  Skipping - no test image directory provided\n\n";
            return true;
        }
        
        bool all_passed = true;
        
        // Load test images
        std::vector<std::string> test_images;
        for (fs::directory_iterator it(test_image_dir); it != fs::directory_iterator(); ++it) {
            if (fs::is_regular_file(it->path())) {
                std::string ext = it->path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                    test_images.push_back(it->path().string());
                }
            }
        }
        
        if (test_images.empty()) {
            std::cout << "  No test images found in " << test_image_dir << "\n\n";
            return true;
        }
        
        // Prioritize specific teed ball test images
        std::vector<std::string> priority_images = {
            "log_ball_final_found_ball_img.png",
            "gs_log_img__log_ball_final_found_ball_img.png"
        };
        
        // Add priority images first if they exist
        std::vector<std::string> ordered_test_images;
        for (const auto& priority_name : priority_images) {
            auto it = std::find_if(test_images.begin(), test_images.end(),
                [&priority_name](const std::string& path) {
                    return fs::path(path).filename().string() == priority_name;
                });
            if (it != test_images.end()) {
                ordered_test_images.push_back(*it);
                test_images.erase(it);
            }
        }
        
        // Add remaining images
        ordered_test_images.insert(ordered_test_images.end(), test_images.begin(), test_images.end());
        
        std::cout << "  Found " << ordered_test_images.size() << " test images\n";
        if (!ordered_test_images.empty() && 
            fs::path(ordered_test_images[0]).filename().string().find("log_ball_final_found_ball_img") != std::string::npos) {
            std::cout << "  Using primary teed ball test image: " << fs::path(ordered_test_images[0]).filename() << "\n";
        }
        
        // Test each image
        for (const auto& image_path : ordered_test_images) {
            cv::Mat img = cv::imread(image_path);
            if (img.empty()) continue;
            
            // Detect ball in image
            GolfBall ball;
            bool ball_found = false;
            
            // Use the system's ball detection
            try {
                // Simplified detection for testing
                std::vector<GsCircle> circles;
                BallImageProc::BallSearchMode mode = BallImageProc::BallSearchMode::kFindPlacedBall;
                
                // This would use YOLO if configured
                if (BallImageProc::DetectBalls(img, mode, circles)) {
                    if (!circles.empty()) {
                        ball.center_ = cv::Point2d(circles[0].center_.x, circles[0].center_.y);
                        ball.measured_radius_pixels_ = circles[0].radius_;
                        ball_found = true;
                    }
                }
            } catch (...) {
                // Fallback to simple circle detection for testing
                cv::Mat gray;
                cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
                std::vector<cv::Vec3f> circles;
                cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, 100, 100, 30, 20, 100);
                
                if (!circles.empty()) {
                    ball.center_ = cv::Point2d(circles[0][0], circles[0][1]);
                    ball.measured_radius_pixels_ = circles[0][2];
                    ball_found = true;
                }
            }
            
            if (ball_found) {
                // Test stillness detection with this ball
                AdaptiveStillnessDetector detector;
                
                // Simulate multiple frames with same ball position
                bool detected = false;
                for (int i = 0; i < 10; i++) {
                    if (detector.isBallStill(ball)) {
                        detected = true;
                        break;
                    }
                }
                
                if (verbose_) {
                    std::cout << "  " << fs::path(image_path).filename() 
                              << ": Ball at (" << ball.center_.x << ", " << ball.center_.y 
                              << ") r=" << ball.measured_radius_pixels_ 
                              << " - " << (detected ? "STILL" : "NOT DETECTED") << "\n";
                }
                
                reportTest("Image: " + fs::path(image_path).filename().string(), detected);
                all_passed &= detected;
            }
        }
        
        std::cout << "\n";
        return all_passed;
    }
    
    /**
     * @brief Timing validation tests
     */
    bool runTimingValidationTests() {
        std::cout << "Running Timing Validation Tests...\n";
        std::cout << "----------------------------------\n";
        
        bool all_passed = true;
        
        // Test different motion patterns and measure detection time
        struct MotionPattern {
            std::string name;
            std::vector<cv::Point2d> positions;
            int expected_detection_frame;
            int tolerance_frames;
        };
        
        std::vector<MotionPattern> patterns = {
            {"Perfectly still", 
             std::vector<cv::Point2d>(20, cv::Point2d(100, 100)), 
             AdaptiveStillnessDetector::kAdaptiveMinFrames, 1},
            
            {"Quick settle", 
             {cv::Point2d(100, 100), cv::Point2d(102, 100), cv::Point2d(101, 100),
              cv::Point2d(100.5, 100), cv::Point2d(100.2, 100), cv::Point2d(100.1, 100),
              cv::Point2d(100, 100), cv::Point2d(100, 100), cv::Point2d(100, 100)},
             7, 2},
            
            {"Slow settle",
             {cv::Point2d(100, 100), cv::Point2d(105, 100), cv::Point2d(103, 100),
              cv::Point2d(102, 100), cv::Point2d(101.5, 100), cv::Point2d(101, 100),
              cv::Point2d(100.7, 100), cv::Point2d(100.5, 100), cv::Point2d(100.3, 100),
              cv::Point2d(100.2, 100), cv::Point2d(100.1, 100), cv::Point2d(100, 100)},
             10, 2}
        };
        
        for (const auto& pattern : patterns) {
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.measured_radius_pixels_ = 50;
            
            int detection_frame = -1;
            auto start_time = std::chrono::high_resolution_clock::now();
            
            for (size_t i = 0; i < pattern.positions.size(); i++) {
                ball.center_ = pattern.positions[i];
                
                if (detector.isBallStill(ball)) {
                    detection_frame = i + 1; // 1-based frame count
                    break;
                }
                
                // Simulate frame delay
                std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
            
            bool frame_match = (detection_frame >= 0) && 
                              (std::abs(detection_frame - pattern.expected_detection_frame) <= pattern.tolerance_frames);
            
            if (verbose_) {
                std::cout << "  Pattern '" << pattern.name << "':\n";
                std::cout << "    Expected frame: " << pattern.expected_detection_frame 
                         << " ± " << pattern.tolerance_frames << "\n";
                std::cout << "    Detected frame: " << detection_frame << "\n";
                std::cout << "    Time: " << duration_ms << " ms\n";
                std::cout << "    Detection time from detector: " << detector.getDetectionTimeMs() << " ms\n";
            }
            
            reportTest("Timing - " + pattern.name, frame_match);
            all_passed &= frame_match;
        }
        
        std::cout << "\n";
        return all_passed;
    }
    
    /**
     * @brief Compare with legacy timer method
     */
    bool runComparisonTests() {
        std::cout << "Running Comparison Tests (Adaptive vs Fixed)...\n";
        std::cout << "-----------------------------------------------\n";
        
        bool all_passed = true;
        
        // Simulate ball placement scenarios
        struct Scenario {
            std::string name;
            int settle_frames;  // Frames until ball is actually still
            double expected_speedup;  // Expected improvement factor
        };
        
        std::vector<Scenario> scenarios = {
            {"Fast placement", 3, 5.0},
            {"Normal placement", 6, 3.0},
            {"Slow placement", 10, 2.0}
        };
        
        const int fixed_timer_ms = 1000;  // Legacy 1-second timer
        const int frame_interval_ms = 16;  // ~60fps
        
        for (const auto& scenario : scenarios) {
            // Adaptive detection
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.measured_radius_pixels_ = 50;
            
            auto adaptive_start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 100; i++) {
                // Simulate motion then stillness
                if (i < scenario.settle_frames) {
                    ball.center_ = cv::Point2d(100 + (scenario.settle_frames - i), 100);
                } else {
                    ball.center_ = cv::Point2d(100, 100);
                }
                
                if (detector.isBallStill(ball)) {
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(frame_interval_ms));
            }
            
            auto adaptive_end = std::chrono::high_resolution_clock::now();
            auto adaptive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                adaptive_end - adaptive_start).count();
            
            // Fixed timer (simulated)
            auto fixed_ms = fixed_timer_ms;
            
            double speedup = (double)fixed_ms / (double)adaptive_ms;
            bool meets_expectation = speedup >= (scenario.expected_speedup * 0.7); // 70% of expected
            
            if (verbose_) {
                std::cout << "  Scenario '" << scenario.name << "':\n";
                std::cout << "    Adaptive: " << adaptive_ms << " ms\n";
                std::cout << "    Fixed: " << fixed_ms << " ms\n";
                std::cout << "    Speedup: " << std::fixed << std::setprecision(1) << speedup << "x\n";
                std::cout << "    Expected: >" << scenario.expected_speedup << "x\n";
            }
            
            reportTest("Comparison - " + scenario.name, meets_expectation);
            all_passed &= meets_expectation;
        }
        
        std::cout << "\n";
        return all_passed;
    }
    
    void reportTest(const std::string& test_name, bool passed) {
        if (passed) {
            std::cout << "  [PASS] " << test_name << "\n";
            test_passed_++;
        } else {
            std::cout << "  [FAIL] " << test_name << "\n";
            test_failed_++;
        }
    }
    
    void printTestSummary() {
        std::cout << "========================================\n";
        std::cout << "Test Summary:\n";
        std::cout << "  Passed: " << test_passed_ << "\n";
        std::cout << "  Failed: " << test_failed_ << "\n";
        std::cout << "  Total:  " << (test_passed_ + test_failed_) << "\n";
        
        if (test_failed_ == 0) {
            std::cout << "\nALL TESTS PASSED! ✓\n";
        } else {
            std::cout << "\nSOME TESTS FAILED ✗\n";
        }
        std::cout << "========================================\n";
    }
};

/**
 * @brief Main test runner
 */
int main(int argc, char* argv[]) {
    std::cout << "Adaptive Stillness Detection Test Program\n";
    std::cout << "==========================================\n";
    
    // Parse command line arguments
    std::string test_image_dir;
    bool verbose = false;
    bool benchmark_mode = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--images" && i + 1 < argc) {
            test_image_dir = argv[++i];
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--benchmark") {
            benchmark_mode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --images <dir>   Directory containing test ball images\n";
            std::cout << "  --verbose, -v    Enable verbose output\n";
            std::cout << "  --benchmark      Run extended performance benchmarks\n";
            std::cout << "  --help, -h       Show this help message\n";
            return 0;
        }
    }
    
    // Auto-detect test images if not specified
    if (test_image_dir.empty()) {
        // Look for standard test image locations
        std::vector<std::string> possible_dirs = {
            "./Images/",
            "../Images/",
            "../../Images/",
            "./TestImages/TeedBalls/",
            "../TestImages/TeedBalls/",
            "/home/PiTracUser/LM_Shares/Images/"
        };
        
        for (const auto& dir : possible_dirs) {
            if (fs::exists(dir)) {
                test_image_dir = dir;
                std::cout << "Auto-detected test image directory: " << test_image_dir << "\n";
                break;
            }
        }
        
        // Also look for specific teed ball test image
        if (test_image_dir.empty() || !fs::exists(test_image_dir)) {
            std::vector<std::string> test_files = {
                "./Images/log_ball_final_found_ball_img.png",
                "./Images/gs_log_img__log_ball_final_found_ball_img.png",
                "../Images/log_ball_final_found_ball_img.png",
                "../Images/gs_log_img__log_ball_final_found_ball_img.png"
            };
            
            for (const auto& file : test_files) {
                if (fs::exists(file)) {
                    test_image_dir = fs::path(file).parent_path().string();
                    std::cout << "Found teed ball test image in: " << test_image_dir << "\n";
                    break;
                }
            }
        }
    }
    
    // Initialize logging
    LoggingTools::SetLogLevel(verbose ? trace : info);
    
    // Load configuration
    if (!GolfSimConfiguration::Init()) {
        std::cout << "Warning: Could not load configuration file, using defaults\n";
    }
    
    // Run test suite
    AdaptiveStillnessTestSuite test_suite;
    bool all_passed = test_suite.runAllTests(test_image_dir, verbose);
    
    // Extended benchmarking if requested
    if (benchmark_mode) {
        std::cout << "\nRunning Extended Benchmarks...\n";
        std::cout << "==============================\n";
        
        // Benchmark different configurations
        struct BenchConfig {
            std::string name;
            int min_frames;
            double pos_threshold;
            double motion_threshold;
        };
        
        std::vector<BenchConfig> configs = {
            {"Ultra Fast", 3, 1.0, 0.2},
            {"Fast", 4, 0.75, 0.15},
            {"Balanced", 6, 0.5, 0.1},
            {"Accurate", 8, 0.25, 0.05},
            {"Ultra Accurate", 10, 0.1, 0.02}
        };
        
        for (const auto& config : configs) {
            AdaptiveStillnessDetector::kAdaptiveMinFrames = config.min_frames;
            AdaptiveStillnessDetector::kAdaptivePositionThreshold = config.pos_threshold;
            AdaptiveStillnessDetector::kAdaptiveMotionThreshold = config.motion_threshold;
            
            AdaptiveStillnessDetector detector;
            GolfBall ball;
            ball.center_ = cv::Point2d(100, 100);
            ball.measured_radius_pixels_ = 50;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            int frames = 0;
            for (frames = 0; frames < 100; frames++) {
                if (detector.isBallStill(ball)) {
                    break;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            std::cout << "  Config '" << config.name << "':\n";
            std::cout << "    Min frames: " << config.min_frames << "\n";
            std::cout << "    Position threshold: " << config.pos_threshold << " px\n";
            std::cout << "    Motion threshold: " << config.motion_threshold << "\n";
            std::cout << "    Detection at frame: " << frames << "\n";
            std::cout << "    Time: " << duration_ms << " ms\n\n";
        }
    }
    
    return all_passed ? 0 : 1;
}