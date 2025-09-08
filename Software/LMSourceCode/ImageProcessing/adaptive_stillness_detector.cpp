/*****************************************************************//**
 * \file   adaptive_stillness_detector.cpp
 * \brief  Implementation of adaptive ball stillness detection
 * 
 * \author PiTrac
 * \date   December 2024
 *********************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024-2025, Verdant Consultants, LLC.
 */

#include "adaptive_stillness_detector.h"
#include "gs_config.h"
#include "logging_tools.h"
#include "gs_format_lib.h"
#include <cmath>
#include <numeric>

namespace golf_sim {

// Static member initialization with defaults
// These are overridden by JSON configuration via Configure()
std::string AdaptiveStillnessDetector::kStabilizationMethod = "adaptive";
int AdaptiveStillnessDetector::kAdaptiveMinFrames = 6;              // ~100ms at 60fps
int AdaptiveStillnessDetector::kAdaptiveMaxFrames = 30;             // ~500ms of history
double AdaptiveStillnessDetector::kAdaptivePositionThreshold = 0.5; // pixels
double AdaptiveStillnessDetector::kAdaptiveRadiusThreshold = 0.02;  // 2% variation
double AdaptiveStillnessDetector::kAdaptiveMotionThreshold = 0.1;   // motion energy units
double AdaptiveStillnessDetector::kAdaptiveMotionDecayFactor = 0.8; // exponential decay
bool AdaptiveStillnessDetector::kAdaptiveDebugLogging = false;
int AdaptiveStillnessDetector::kAdaptiveCheckIntervalMs = 16;       // ~60fps
bool AdaptiveStillnessDetector::kAdaptiveUseFastMode = false;

bool AdaptiveStillnessDetector::Configure() {
    GS_LOG_TRACE_MSG(trace, "AdaptiveStillnessDetector::Configure");
    
    // Load configuration from JSON file
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kStabilizationMethod", 
                                     kStabilizationMethod);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveMinFrames", 
                                     kAdaptiveMinFrames);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveMaxFrames", 
                                     kAdaptiveMaxFrames);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptivePositionThreshold", 
                                     kAdaptivePositionThreshold);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveRadiusThreshold", 
                                     kAdaptiveRadiusThreshold);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveMotionThreshold", 
                                     kAdaptiveMotionThreshold);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveMotionDecayFactor",
                                     kAdaptiveMotionDecayFactor);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveDebugLogging",
                                     kAdaptiveDebugLogging);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveCheckIntervalMs",
                                     kAdaptiveCheckIntervalMs);
    GolfSimConfiguration::SetConstant("gs_config.ball_stabilization.kAdaptiveUseFastMode",
                                     kAdaptiveUseFastMode);
    
    GS_LOG_MSG(info, "AdaptiveStillnessDetector configured:");
    GS_LOG_MSG(info, "  Method: " + kStabilizationMethod);
    GS_LOG_MSG(info, "  MinFrames: " + std::to_string(kAdaptiveMinFrames));
    GS_LOG_MSG(info, "  MaxFrames: " + std::to_string(kAdaptiveMaxFrames));
    GS_LOG_MSG(info, "  PositionThreshold: " + std::to_string(kAdaptivePositionThreshold) + " pixels");
    GS_LOG_MSG(info, "  RadiusThreshold: " + std::to_string(kAdaptiveRadiusThreshold * 100) + "%");
    GS_LOG_MSG(info, "  MotionThreshold: " + std::to_string(kAdaptiveMotionThreshold));
    GS_LOG_MSG(info, "  MotionDecayFactor: " + std::to_string(kAdaptiveMotionDecayFactor));
    GS_LOG_MSG(info, "  CheckIntervalMs: " + std::to_string(kAdaptiveCheckIntervalMs));
    GS_LOG_MSG(info, "  FastMode: " + std::string(kAdaptiveUseFastMode ? "enabled" : "disabled"));
    
    return true;
}

AdaptiveStillnessDetector::AdaptiveStillnessDetector() 
    : position_history_(kAdaptiveMaxFrames),
      radius_history_(kAdaptiveMaxFrames),
      start_time_{},
      detection_time_ms_(-1),
      last_position_variance_(999.0),
      last_motion_energy_(999.0),
      last_radius_variance_(999.0) {
    GS_LOG_TRACE_MSG(trace, "AdaptiveStillnessDetector created with buffer size: " + 
                    std::to_string(kAdaptiveMaxFrames));
}

void AdaptiveStillnessDetector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    position_history_.clear();
    radius_history_.clear();
    start_time_ = std::chrono::steady_clock::time_point{};
    detection_time_ms_ = -1;
    last_position_variance_ = 999.0;
    last_motion_energy_ = 999.0;
    last_radius_variance_ = 999.0;
    
    GS_LOG_TRACE_MSG(trace, "AdaptiveStillnessDetector reset");
}

bool AdaptiveStillnessDetector::isBallStill(const GolfBall& ball) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Track timing
    if (position_history_.empty()) {
        start_time_ = std::chrono::steady_clock::now();
        detection_time_ms_ = -1;
    }
    
    // Add current measurement
    position_history_.push_back(cv::Point2f(static_cast<float>(ball.x()), 
                                           static_cast<float>(ball.y())));
    radius_history_.push_back(ball.measured_radius_pixels_);
    
    // Need minimum samples
    if (position_history_.size() < static_cast<size_t>(kAdaptiveMinFrames)) {
        if (kAdaptiveDebugLogging) {
            GS_LOG_TRACE_MSG(trace, "Adaptive: Need more frames (" + 
                            std::to_string(position_history_.size()) + "/" + 
                            std::to_string(kAdaptiveMinFrames) + ")");
        }
        return false;
    }
    
    // Calculate metrics (skip some in fast mode)
    double pos_var = calculatePositionVariance();
    double motion = kAdaptiveUseFastMode ? 0.0 : calculateMotionEnergy();
    double rad_var = kAdaptiveUseFastMode ? 0.0 : calculateRadiusVariance();
    
    // Store for debugging
    last_position_variance_ = pos_var;
    last_motion_energy_ = motion;
    last_radius_variance_ = rad_var;
    
    // Check thresholds
    bool position_stable = (pos_var < kAdaptivePositionThreshold);
    bool motion_stable = kAdaptiveUseFastMode || (motion < kAdaptiveMotionThreshold);
    bool radius_stable = kAdaptiveUseFastMode || (rad_var < kAdaptiveRadiusThreshold);
    
    bool is_still = position_stable && motion_stable && radius_stable;
    
    if (is_still && detection_time_ms_ < 0) {
        // First time detecting stillness
        auto now = std::chrono::steady_clock::now();
        detection_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();
        
        GS_LOG_MSG(info, "Ball stabilized in " + std::to_string(detection_time_ms_) + 
                   "ms (" + std::to_string(position_history_.size()) + " frames)");
        GS_LOG_MSG(info, "  Position variance: " + std::to_string(pos_var) + 
                   " pixels (threshold: " + std::to_string(kAdaptivePositionThreshold) + ")");
        if (!kAdaptiveUseFastMode) {
            GS_LOG_MSG(info, "  Motion energy: " + std::to_string(motion) + 
                       " (threshold: " + std::to_string(kAdaptiveMotionThreshold) + ")");
            GS_LOG_MSG(info, "  Radius variance: " + std::to_string(rad_var * 100) + 
                       "% (threshold: " + std::to_string(kAdaptiveRadiusThreshold * 100) + "%)");
        }
    }
    
    if (kAdaptiveDebugLogging && !is_still) {
        GS_LOG_TRACE_MSG(trace, "Not still - PosVar: " + std::to_string(pos_var) + 
                        " Motion: " + std::to_string(motion) + 
                        " RadVar: " + std::to_string(rad_var));
    }
    
    return is_still;
}

double AdaptiveStillnessDetector::calculatePositionVariance() {
    int sample_size = getSampleSize();
    if (sample_size < 2) return 999.0;
    
    // Calculate centroid of recent positions
    cv::Point2f centroid(0, 0);
    auto start_it = position_history_.end() - sample_size;
    for (auto it = start_it; it != position_history_.end(); ++it) {
        centroid += *it;
    }
    centroid *= (1.0f / sample_size);
    
    // Calculate variance from centroid
    double variance_sum = 0;
    for (auto it = start_it; it != position_history_.end(); ++it) {
        cv::Point2f diff = *it - centroid;
        variance_sum += (diff.x * diff.x + diff.y * diff.y);
    }
    
    // Return standard deviation (square root of variance)
    return std::sqrt(variance_sum / sample_size);
}

double AdaptiveStillnessDetector::calculateMotionEnergy() {
    if (position_history_.size() < 2) return 999.0;
    
    double total_motion = 0;
    double weight = 1.0;
    int samples_processed = 0;
    int max_samples = std::min(static_cast<int>(position_history_.size()) - 1, 
                               kAdaptiveMinFrames);
    
    // Calculate frame-to-frame motion with exponential decay for older frames
    for (size_t i = position_history_.size() - 1; i > 0 && samples_processed < max_samples; --i) {
        cv::Point2f delta = position_history_[i] - position_history_[i-1];
        double frame_motion = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        
        total_motion += frame_motion * weight;
        weight *= kAdaptiveMotionDecayFactor;
        samples_processed++;
    }
    
    return total_motion;
}

double AdaptiveStillnessDetector::calculateRadiusVariance() {
    int sample_size = getSampleSize();
    if (sample_size < 2) return 999.0;
    
    // Calculate mean radius
    float sum = 0;
    auto start_it = radius_history_.end() - sample_size;
    for (auto it = start_it; it != radius_history_.end(); ++it) {
        sum += *it;
    }
    float mean_radius = sum / sample_size;
    
    // Avoid division by zero
    if (mean_radius < 1.0f) return 999.0;
    
    // Calculate variance
    float variance_sum = 0;
    for (auto it = start_it; it != radius_history_.end(); ++it) {
        float diff = *it - mean_radius;
        variance_sum += diff * diff;
    }
    
    float stddev = std::sqrt(variance_sum / sample_size);
    
    // Return coefficient of variation (normalized by mean)
    return stddev / mean_radius;
}

int AdaptiveStillnessDetector::getSampleSize() const {
    return std::min(static_cast<int>(position_history_.size()), kAdaptiveMinFrames);
}

std::string AdaptiveStillnessDetector::getStatusString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string status = "AdaptiveStillness[";
    status += "frames:" + std::to_string(position_history_.size());
    status += ", pos_var:" + std::to_string(last_position_variance_);
    status += ", motion:" + std::to_string(last_motion_energy_);
    status += ", rad_var:" + std::to_string(last_radius_variance_ * 100) + "%";
    if (detection_time_ms_ > 0) {
        status += ", detected:" + std::to_string(detection_time_ms_) + "ms";
    }
    status += "]";
    
    return status;
}

} // namespace golf_sim