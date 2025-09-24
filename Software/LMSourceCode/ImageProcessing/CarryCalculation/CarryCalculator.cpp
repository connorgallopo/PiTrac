#include "CarryCalculator.h"
#include "LookupTable.h"
#include "PhysicsFormulas.h"
#include "NEONOptimizations.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>

namespace PiTrac {

// Environmental corrections implementation
class EnvironmentalCorrections {
public:
    float apply(float base_carry, const CarryCalculator::EnvironmentalConditions& env, float ball_speed_mps) {
        float adjusted = base_carry;

        // Temperature adjustment (warmer = less dense = more carry)
        float temp_factor = 1.0f + (env.temperature_celsius - 20.0f) * 0.0005f;
        adjusted *= temp_factor;

        // Altitude adjustment (higher = less dense = more carry)
        float altitude_factor = 1.0f + (env.altitude_meters / 1000.0f) * 0.02f;
        adjusted *= altitude_factor;

        // Humidity adjustment (more humid = less dense = more carry)
        float humidity_factor = 1.0f + (env.humidity_percent - 50.0f) * 0.0001f;
        adjusted *= humidity_factor;

        // Wind adjustment
        if (env.wind_speed_mps > 0) {
            // Headwind/tailwind component
            float wind_effect = env.wind_speed_mps * std::cos(env.wind_direction_deg * M_PI / 180.0f);
            adjusted += wind_effect * 2.0f; // 2 meters per m/s of wind
        }

        return adjusted;
    }
};

CarryCalculator::CarryCalculator(AccuracyMode mode)
    : accuracy_mode_(mode) {

    lookup_table_ = std::make_unique<LookupTable3D>();
    physics_engine_ = std::make_unique<PhysicsEngine>();
    env_corrections_ = std::make_unique<EnvironmentalCorrections>();

    // Try to load pre-computed lookup table
    std::string table_path = "/usr/share/pitrac/carry_table.plt3";
    if (!lookup_table_->loadFromFile(table_path)) {
        // Try local path
        table_path = "carry_table.plt3";
        if (!lookup_table_->loadFromFile(table_path)) {
            std::cout << "Warning: No pre-computed carry table found. "
                     << "Using physics calculations (slower)." << std::endl;
        }
    }

    last_metrics_ = {};
}

CarryCalculator::~CarryCalculator() = default;

float CarryCalculator::calculateCarry(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm,
    const EnvironmentalConditions* env) {

    auto start = std::chrono::high_resolution_clock::now();

    // Validate inputs
    if (!validateInputs(ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm)) {
        last_metrics_.estimated_error_yards = 999.0f;
        return 0.0f;
    }

    float carry_meters = 0.0f;
    bool used_lookup = false;
    bool applied_corrections = false;

    switch (accuracy_mode_) {
        case AccuracyMode::FAST:
            // Use simplified physics for fastest calculation
            carry_meters = SimpleBallistics::calculateCarryFast(
                ball_speed_mps, launch_angle_deg, backspin_rpm
            );
            last_metrics_.estimated_error_yards = 3.0f;
            break;

        case AccuracyMode::BALANCED:
            // Try lookup table first
            if (lookup_table_->isValid()) {
                carry_meters = calculateWithLookupTable(
                    ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm
                );
                used_lookup = true;
                last_metrics_.estimated_error_yards = 1.5f;
            } else {
                // Fall back to simplified physics
                carry_meters = physics_engine_->calculateCarrySimplified(
                    ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm
                );
                last_metrics_.estimated_error_yards = 2.0f;
            }

            // Apply environmental corrections if provided
            if (env) {
                carry_meters = applyEnvironmentalCorrections(carry_meters, *env, ball_speed_mps);
                applied_corrections = true;
            }
            break;

        case AccuracyMode::ACCURATE:
            // Use full physics simulation
            if (env) {
                float air_density = PhysicsEngine::calculateAirDensity(
                    env->temperature_celsius, env->altitude_meters, env->humidity_percent
                );
                carry_meters = physics_engine_->calculateCarryRK4(
                    ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm, air_density
                );
                applied_corrections = true;
            } else {
                carry_meters = physics_engine_->calculateCarryRK4(
                    ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm
                );
            }
            last_metrics_.estimated_error_yards = 0.8f;
            break;
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Update performance metrics
    last_metrics_.calculation_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    last_metrics_.used_lookup_table = used_lookup;
    last_metrics_.applied_corrections = applied_corrections;
    last_metrics_.interpolation_points_used = used_lookup ? 8 : 0;

    return carry_meters;
}

void CarryCalculator::calculateMultipleCarries(
    const float* ball_speeds_mps,
    const float* launch_angles_deg,
    const float* backspin_rpms,
    const float* sidespin_rpms,
    float* carry_distances_meters,
    int count) {

    // Use NEON optimization for batch processing
    NEONCarryCalculator::calculateBatchN(
        ball_speeds_mps,
        launch_angles_deg,
        backspin_rpms,
        sidespin_rpms,
        carry_distances_meters,
        count
    );
}

void CarryCalculator::setAccuracyMode(AccuracyMode mode) {
    accuracy_mode_ = mode;
}

bool CarryCalculator::loadLookupTable(const std::string& filepath) {
    return lookup_table_->loadFromFile(filepath);
}

bool CarryCalculator::saveLookupTable(const std::string& filepath) const {
    return lookup_table_->saveToFile(filepath);
}

void CarryCalculator::generateLookupTable(bool high_precision) {
    std::cout << "Generating lookup table..." << std::endl;

    int total_entries = LookupTable3D::SPEED_BINS *
                       LookupTable3D::ANGLE_BINS *
                       LookupTable3D::SPIN_BINS;
    int processed = 0;

    for (int s = 0; s < LookupTable3D::SPEED_BINS; ++s) {
        float speed = LookupTable3D::MIN_SPEED_MPS +
                     (LookupTable3D::MAX_SPEED_MPS - LookupTable3D::MIN_SPEED_MPS) *
                     s / (LookupTable3D::SPEED_BINS - 1);

        for (int a = 0; a < LookupTable3D::ANGLE_BINS; ++a) {
            float angle = LookupTable3D::MIN_ANGLE_DEG +
                         (LookupTable3D::MAX_ANGLE_DEG - LookupTable3D::MIN_ANGLE_DEG) *
                         a / (LookupTable3D::ANGLE_BINS - 1);

            for (int sp = 0; sp < LookupTable3D::SPIN_BINS; ++sp) {
                float spin = LookupTable3D::MIN_SPIN_RPM +
                           (LookupTable3D::MAX_SPIN_RPM - LookupTable3D::MIN_SPIN_RPM) *
                           sp / (LookupTable3D::SPIN_BINS - 1);

                float carry;
                if (high_precision) {
                    // Use RK4 for high precision
                    carry = physics_engine_->calculateCarryRK4(speed, angle, spin, 0);
                } else {
                    // Use simplified physics for speed
                    carry = physics_engine_->calculateCarrySimplified(speed, angle, spin, 0);
                }

                lookup_table_->setEntry(s, a, sp, carry, 255);

                processed++;
                if (processed % 100 == 0) {
                    float progress = (float)processed / total_entries * 100.0f;
                    std::cout << "\rProgress: " << progress << "%";
                    std::cout.flush();
                }
            }
        }
    }

    std::cout << "\nLookup table generation complete!" << std::endl;
}

size_t CarryCalculator::getMemoryUsage() const {
    size_t total = 0;
    if (lookup_table_) {
        total += lookup_table_->getMemoryUsage();
    }
    total += sizeof(*this);
    total += sizeof(PhysicsEngine);
    total += sizeof(EnvironmentalCorrections);
    return total;
}

bool CarryCalculator::validateInputs(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm) {

    // Check reasonable ranges
    if (ball_speed_mps < 10.0f || ball_speed_mps > 100.0f) {
        return false;
    }
    if (launch_angle_deg < -15.0f || launch_angle_deg > 60.0f) {
        return false;
    }
    if (backspin_rpm < -1000.0f || backspin_rpm > 15000.0f) {
        return false;
    }
    if (std::abs(sidespin_rpm) > 10000.0f) {
        return false;
    }

    // Check for NaN or infinity
    if (!std::isfinite(ball_speed_mps) || !std::isfinite(launch_angle_deg) ||
        !std::isfinite(backspin_rpm) || !std::isfinite(sidespin_rpm)) {
        return false;
    }

    return true;
}

float CarryCalculator::calculateWithLookupTable(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm) {

    return lookup_table_->lookup(ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm);
}

float CarryCalculator::calculateWithPhysics(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm,
    const EnvironmentalConditions* env) {

    if (env) {
        float air_density = PhysicsEngine::calculateAirDensity(
            env->temperature_celsius, env->altitude_meters, env->humidity_percent
        );
        return physics_engine_->calculateCarryRK4(
            ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm, air_density
        );
    } else {
        return physics_engine_->calculateCarrySimplified(
            ball_speed_mps, launch_angle_deg, backspin_rpm, sidespin_rpm
        );
    }
}

float CarryCalculator::applyEnvironmentalCorrections(
    float base_carry,
    const EnvironmentalConditions& env,
    float ball_speed_mps) {

    return env_corrections_->apply(base_carry, env, ball_speed_mps);
}

// Static helper for air density calculation
float PhysicsEngine::calculateAirDensity(
    float temperature_celsius,
    float altitude_meters,
    float humidity_percent) {

    // Base air density at sea level, 15°C
    float base_density = 1.225f;

    // Temperature correction
    float temp_kelvin = temperature_celsius + 273.15f;
    float temp_factor = 288.15f / temp_kelvin;

    // Altitude correction (barometric formula)
    float altitude_factor = std::exp(-altitude_meters / 8400.0f);

    // Humidity correction (simplified)
    float humidity_factor = 1.0f - (humidity_percent / 100.0f) * 0.01f;

    return base_density * temp_factor * altitude_factor * humidity_factor;
}

} // namespace PiTrac