#include "LookupTable.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace PiTrac {

LookupTable3D::LookupTable3D()
    : is_compressed_(false)
    , is_valid_(false)
    , entries_filled_(0) {

    // Initialize table with zeros
    table_.resize(SPEED_BINS);
    for (int i = 0; i < SPEED_BINS; ++i) {
        table_[i].resize(ANGLE_BINS);
        for (int j = 0; j < ANGLE_BINS; ++j) {
            table_[i][j].resize(SPIN_BINS);
            for (int k = 0; k < SPIN_BINS; ++k) {
                table_[i][j][k] = {0.0f, 0};
            }
        }
    }
}

float LookupTable3D::lookup(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm) const {

    // Boundary check
    if (!isInBounds(ball_speed_mps, launch_angle_deg, backspin_rpm)) {
        // Fall back to nearest neighbor for out-of-bounds
        return lookupNearest(ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm);
    }

    // Get base indices
    int speed_idx = getSpeedIndex(ball_speed_mps);
    int angle_idx = getAngleIndex(launch_angle_deg);
    int spin_idx = getSpinIndex(backspin_rpm);

    // Get fractional positions for interpolation
    float speed_frac = getSpeedFraction(ball_speed_mps, speed_idx);
    float angle_frac = getAngleFraction(launch_angle_deg, angle_idx);
    float spin_frac = getSpinFraction(backspin_rpm, spin_idx);

    // Perform trilinear interpolation
    float base_carry = trilinearInterpolate(
        speed_frac, angle_frac, spin_frac,
        speed_idx, angle_idx, spin_idx
    );

    // Apply sidespin correction
    float sidespin_factor = computeSideSpinFactor(sidespin_rpm);
    return base_carry * sidespin_factor;
}

float LookupTable3D::lookupNearest(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm) const {

    // Get nearest indices
    int speed_idx = std::round((ball_speed_mps - MIN_SPEED_MPS) /
                              ((MAX_SPEED_MPS - MIN_SPEED_MPS) / (SPEED_BINS - 1)));
    int angle_idx = std::round((launch_angle_deg - MIN_ANGLE_DEG) /
                              ((MAX_ANGLE_DEG - MIN_ANGLE_DEG) / (ANGLE_BINS - 1)));
    int spin_idx = std::round((backspin_rpm - MIN_SPIN_RPM) /
                             ((MAX_SPIN_RPM - MIN_SPIN_RPM) / (SPIN_BINS - 1)));

    // Clamp to valid range
    clampIndices(speed_idx, angle_idx, spin_idx);

    // Get value
    float base_carry = table_[speed_idx][angle_idx][spin_idx].carry_meters;

    // Apply sidespin correction
    float sidespin_factor = computeSideSpinFactor(sidespin_rpm);
    return base_carry * sidespin_factor;
}

void LookupTable3D::setEntry(
    int speed_idx,
    int angle_idx,
    int spin_idx,
    float carry_meters,
    uint8_t confidence) {

    if (speed_idx >= 0 && speed_idx < SPEED_BINS &&
        angle_idx >= 0 && angle_idx < ANGLE_BINS &&
        spin_idx >= 0 && spin_idx < SPIN_BINS) {

        if (table_[speed_idx][angle_idx][spin_idx].confidence == 0) {
            entries_filled_++;
        }

        table_[speed_idx][angle_idx][spin_idx] = {carry_meters, confidence};

        if (entries_filled_ > (SPEED_BINS * ANGLE_BINS * SPIN_BINS * 0.8)) {
            is_valid_ = true;
        }
    }
}

LookupTable3D::TableEntry LookupTable3D::getEntry(
    int speed_idx,
    int angle_idx,
    int spin_idx) const {

    if (speed_idx >= 0 && speed_idx < SPEED_BINS &&
        angle_idx >= 0 && angle_idx < ANGLE_BINS &&
        spin_idx >= 0 && spin_idx < SPIN_BINS) {
        return table_[speed_idx][angle_idx][spin_idx];
    }
    return {0.0f, 0};
}

bool LookupTable3D::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read header
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "PLT3", 4) != 0) {
        return false;
    }

    // Read dimensions
    int speed_bins, angle_bins, spin_bins;
    file.read(reinterpret_cast<char*>(&speed_bins), sizeof(int));
    file.read(reinterpret_cast<char*>(&angle_bins), sizeof(int));
    file.read(reinterpret_cast<char*>(&spin_bins), sizeof(int));

    if (speed_bins != SPEED_BINS || angle_bins != ANGLE_BINS || spin_bins != SPIN_BINS) {
        return false;
    }

    // Read data
    entries_filled_ = 0;
    for (int i = 0; i < SPEED_BINS; ++i) {
        for (int j = 0; j < ANGLE_BINS; ++j) {
            for (int k = 0; k < SPIN_BINS; ++k) {
                file.read(reinterpret_cast<char*>(&table_[i][j][k].carry_meters), sizeof(float));
                file.read(reinterpret_cast<char*>(&table_[i][j][k].confidence), sizeof(uint8_t));
                if (table_[i][j][k].confidence > 0) {
                    entries_filled_++;
                }
            }
        }
    }

    file.close();
    is_valid_ = (entries_filled_ > (SPEED_BINS * ANGLE_BINS * SPIN_BINS * 0.8));
    return true;
}

bool LookupTable3D::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write header
    file.write("PLT3", 4);

    // Write dimensions
    file.write(reinterpret_cast<const char*>(&SPEED_BINS), sizeof(int));
    file.write(reinterpret_cast<const char*>(&ANGLE_BINS), sizeof(int));
    file.write(reinterpret_cast<const char*>(&SPIN_BINS), sizeof(int));

    // Write data
    for (int i = 0; i < SPEED_BINS; ++i) {
        for (int j = 0; j < ANGLE_BINS; ++j) {
            for (int k = 0; k < SPIN_BINS; ++k) {
                file.write(reinterpret_cast<const char*>(&table_[i][j][k].carry_meters), sizeof(float));
                file.write(reinterpret_cast<const char*>(&table_[i][j][k].confidence), sizeof(uint8_t));
            }
        }
    }

    file.close();
    return true;
}

size_t LookupTable3D::getMemoryUsage() const {
    return SPEED_BINS * ANGLE_BINS * SPIN_BINS * sizeof(TableEntry);
}

bool LookupTable3D::isValid() const {
    return is_valid_;
}

float LookupTable3D::getCoverage() const {
    int total_entries = SPEED_BINS * ANGLE_BINS * SPIN_BINS;
    return static_cast<float>(entries_filled_) / static_cast<float>(total_entries) * 100.0f;
}

int LookupTable3D::getSpeedIndex(float speed_mps) const {
    float normalized = (speed_mps - MIN_SPEED_MPS) / (MAX_SPEED_MPS - MIN_SPEED_MPS);
    int idx = static_cast<int>(normalized * (SPEED_BINS - 1));
    return std::max(0, std::min(SPEED_BINS - 1, idx));
}

int LookupTable3D::getAngleIndex(float angle_deg) const {
    float normalized = (angle_deg - MIN_ANGLE_DEG) / (MAX_ANGLE_DEG - MIN_ANGLE_DEG);
    int idx = static_cast<int>(normalized * (ANGLE_BINS - 1));
    return std::max(0, std::min(ANGLE_BINS - 1, idx));
}

int LookupTable3D::getSpinIndex(float spin_rpm) const {
    float normalized = (spin_rpm - MIN_SPIN_RPM) / (MAX_SPIN_RPM - MIN_SPIN_RPM);
    int idx = static_cast<int>(normalized * (SPIN_BINS - 1));
    return std::max(0, std::min(SPIN_BINS - 1, idx));
}

float LookupTable3D::getSpeedFraction(float speed_mps, int index) const {
    float bin_size = (MAX_SPEED_MPS - MIN_SPEED_MPS) / (SPEED_BINS - 1);
    float bin_start = MIN_SPEED_MPS + index * bin_size;
    return (speed_mps - bin_start) / bin_size;
}

float LookupTable3D::getAngleFraction(float angle_deg, int index) const {
    float bin_size = (MAX_ANGLE_DEG - MIN_ANGLE_DEG) / (ANGLE_BINS - 1);
    float bin_start = MIN_ANGLE_DEG + index * bin_size;
    return (angle_deg - bin_start) / bin_size;
}

float LookupTable3D::getSpinFraction(float spin_rpm, int index) const {
    float bin_size = (MAX_SPIN_RPM - MIN_SPIN_RPM) / (SPIN_BINS - 1);
    float bin_start = MIN_SPIN_RPM + index * bin_size;
    return (spin_rpm - bin_start) / bin_size;
}

float LookupTable3D::trilinearInterpolate(
    float speed_frac,
    float angle_frac,
    float spin_frac,
    int speed_idx,
    int angle_idx,
    int spin_idx) const {

    // Ensure we have room for interpolation
    int s0 = speed_idx;
    int s1 = std::min(speed_idx + 1, SPEED_BINS - 1);
    int a0 = angle_idx;
    int a1 = std::min(angle_idx + 1, ANGLE_BINS - 1);
    int sp0 = spin_idx;
    int sp1 = std::min(spin_idx + 1, SPIN_BINS - 1);

    // Get the 8 corner values
    float c000 = table_[s0][a0][sp0].carry_meters;
    float c001 = table_[s0][a0][sp1].carry_meters;
    float c010 = table_[s0][a1][sp0].carry_meters;
    float c011 = table_[s0][a1][sp1].carry_meters;
    float c100 = table_[s1][a0][sp0].carry_meters;
    float c101 = table_[s1][a0][sp1].carry_meters;
    float c110 = table_[s1][a1][sp0].carry_meters;
    float c111 = table_[s1][a1][sp1].carry_meters;

    // Trilinear interpolation
    float c00 = c000 * (1 - spin_frac) + c001 * spin_frac;
    float c01 = c010 * (1 - spin_frac) + c011 * spin_frac;
    float c10 = c100 * (1 - spin_frac) + c101 * spin_frac;
    float c11 = c110 * (1 - spin_frac) + c111 * spin_frac;

    float c0 = c00 * (1 - angle_frac) + c01 * angle_frac;
    float c1 = c10 * (1 - angle_frac) + c11 * angle_frac;

    return c0 * (1 - speed_frac) + c1 * speed_frac;
}

void LookupTable3D::clampIndices(int& speed_idx, int& angle_idx, int& spin_idx) const {
    speed_idx = std::max(0, std::min(SPEED_BINS - 1, speed_idx));
    angle_idx = std::max(0, std::min(ANGLE_BINS - 1, angle_idx));
    spin_idx = std::max(0, std::min(SPIN_BINS - 1, spin_idx));
}

bool LookupTable3D::isInBounds(float speed_mps, float angle_deg, float spin_rpm) const {
    return speed_mps >= MIN_SPEED_MPS && speed_mps <= MAX_SPEED_MPS &&
           angle_deg >= MIN_ANGLE_DEG && angle_deg <= MAX_ANGLE_DEG &&
           spin_rpm >= MIN_SPIN_RPM && spin_rpm <= MAX_SPIN_RPM;
}

float LookupTable3D::computeSideSpinFactor(float sidespin_rpm) const {
    // Sidespin reduces carry distance
    // Approximation: 1% reduction per 500 RPM of sidespin
    float sidespin_abs = std::abs(sidespin_rpm);
    float reduction_factor = 1.0f - (sidespin_abs / 500.0f) * 0.01f;
    return std::max(0.85f, reduction_factor); // Cap at 15% reduction
}

void LookupTable3D::printStatistics() const {
    std::cout << "LookupTable3D Statistics:" << std::endl;
    std::cout << "  Dimensions: " << SPEED_BINS << " x " << ANGLE_BINS << " x " << SPIN_BINS << std::endl;
    std::cout << "  Memory usage: " << getMemoryUsage() / 1024 << " KB" << std::endl;
    std::cout << "  Entries filled: " << entries_filled_ << " / " << (SPEED_BINS * ANGLE_BINS * SPIN_BINS) << std::endl;
    std::cout << "  Coverage: " << getCoverage() << "%" << std::endl;
    std::cout << "  Valid: " << (is_valid_ ? "Yes" : "No") << std::endl;
}

} // namespace PiTrac