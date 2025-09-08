/*****************************************************************//**
 * \file   adaptive_stillness_detector.h
 * \brief  Adaptive ball stillness detection using statistical analysis
 *         to replace fixed timer-based approach for faster ready-to-hit detection
 * 
 * \author PiTrac
 * \date   December 2024
 *********************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024-2025, Verdant Consultants, LLC.
 */

#pragma once

#include <boost/circular_buffer.hpp>
#include <opencv2/core.hpp>
#include <chrono>
#include <mutex>
#include "golf_ball.h"

namespace golf_sim {

/**
 * @brief Detects when a golf ball has stabilized (stopped moving) using statistical analysis
 * 
 * This class replaces the fixed 1-second timer with an adaptive approach that can detect
 * stillness in as little as 50-150ms (3-10 frames at 60fps). It analyzes position variance,
 * motion energy decay, and radius stability to determine when the ball is ready for hit.
 * 
 * Performance benefits:
 * - 5-20x faster than fixed timer (50-150ms vs 1000ms)
 * - Reduces expensive CheckForBall() calls
 * - More reliable - actually detects stillness rather than assuming it
 */
class AdaptiveStillnessDetector {
public:
    // Configuration parameters (loaded from JSON via GolfSimConfiguration)
    static std::string kStabilizationMethod;        // "adaptive" or "fixed"
    static int kAdaptiveMinFrames;                  // Minimum frames before decision (default: 6)
    static int kAdaptiveMaxFrames;                  // Maximum history size (default: 30)
    static double kAdaptivePositionThreshold;       // Position variance threshold in pixels (default: 0.5)
    static double kAdaptiveRadiusThreshold;         // Radius coefficient of variation (default: 0.02)
    static double kAdaptiveMotionThreshold;         // Motion energy threshold (default: 0.1)
    static double kAdaptiveMotionDecayFactor;       // Exponential decay for older frames (default: 0.8)
    static bool kAdaptiveDebugLogging;              // Enable detailed debug output (default: false)
    
    // Performance tuning for Raspberry Pi 5
    static int kAdaptiveCheckIntervalMs;            // Milliseconds between checks (default: 16 ~60fps)
    static bool kAdaptiveUseFastMode;               // Skip some calculations for speed (default: false)
    
    /**
     * @brief Constructor initializes circular buffers
     */
    AdaptiveStillnessDetector();
    
    /**
     * @brief Destructor
     */
    ~AdaptiveStillnessDetector() = default;
    
    /**
     * @brief Main detection function - determines if ball is still
     * 
     * @param ball Current ball detection with position and radius
     * @return true if ball is stable and ready for hit, false if still moving
     */
    bool isBallStill(const GolfBall& ball);
    
    /**
     * @brief Reset detector for new ball placement
     * Clears all history buffers
     */
    void reset();
    
    /**
     * @brief Get number of frames analyzed so far
     * @return Frame count in current detection cycle
     */
    int getFrameCount() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(position_history_.size()); 
    }
    
    /**
     * @brief Get last calculated position variance for debugging
     * @return Position variance in pixels
     */
    double getLastPositionVariance() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return last_position_variance_; 
    }
    
    /**
     * @brief Get last calculated motion energy for debugging
     * @return Motion energy value
     */
    double getLastMotionEnergy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_motion_energy_;
    }
    
    /**
     * @brief Get last calculated radius variance for debugging
     * @return Radius coefficient of variation
     */
    double getLastRadiusVariance() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_radius_variance_;
    }
    
    /**
     * @brief Get detection time in milliseconds
     * @return Time from first frame to detection (or -1 if not detected)
     */
    int getDetectionTimeMs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (detection_time_ms_ > 0) {
            return detection_time_ms_;
        }
        if (start_time_ != std::chrono::steady_clock::time_point{}) {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
        }
        return -1;
    }
    
    /**
     * @brief Configure detector from JSON configuration file
     * @return true if configuration successful
     */
    static bool Configure();
    
    /**
     * @brief Get human-readable status string for debugging
     * @return Status string with current metrics
     */
    std::string getStatusString() const;

private:
    // Thread safety
    mutable std::mutex mutex_;
    
    // Position and radius history buffers
    boost::circular_buffer<cv::Point2f> position_history_;
    boost::circular_buffer<float> radius_history_;
    
    // Timing tracking
    std::chrono::steady_clock::time_point start_time_;
    int detection_time_ms_;
    
    // Last calculated metrics for debugging
    double last_position_variance_;
    double last_motion_energy_;
    double last_radius_variance_;
    
    /**
     * @brief Calculate variance of recent ball positions
     * 
     * Uses the last kAdaptiveMinFrames positions to calculate spatial variance.
     * Lower variance indicates the ball has stopped moving.
     * 
     * @return Position variance in pixels (< kAdaptivePositionThreshold when still)
     */
    double calculatePositionVariance();
    
    /**
     * @brief Calculate motion energy with exponential decay
     * 
     * Measures frame-to-frame movement with recent frames weighted more heavily.
     * This helps detect when motion is decaying to zero.
     * 
     * @return Motion energy value (< kAdaptiveMotionThreshold when still)
     */
    double calculateMotionEnergy();
    
    /**
     * @brief Calculate radius stability
     * 
     * Measures variation in detected ball radius. Stable radius indicates
     * the ball isn't moving toward/away from camera.
     * 
     * @return Radius coefficient of variation (< kAdaptiveRadiusThreshold when still)
     */
    double calculateRadiusVariance();
    
    /**
     * @brief Helper to get sample size for calculations
     * @return Number of samples to use (min of available and kAdaptiveMinFrames)
     */
    int getSampleSize() const;
};

} // namespace golf_sim