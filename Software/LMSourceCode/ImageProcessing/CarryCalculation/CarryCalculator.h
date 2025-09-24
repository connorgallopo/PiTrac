#ifndef CARRY_CALCULATOR_H
#define CARRY_CALCULATOR_H

#include <string>
#include <memory>
#include <chrono>

namespace PiTrac {

class LookupTable3D;
class PhysicsEngine;
class EnvironmentalCorrections;

class CarryCalculator {
public:
    enum class AccuracyMode {
        FAST,      // <0.5ms, ±3 yards (lookup only)
        BALANCED,  // <2ms, ±1.5 yards (lookup + corrections)
        ACCURATE   // <5ms, ±0.8 yards (physics simulation)
    };

    struct EnvironmentalConditions {
        float temperature_celsius = 20.0f;
        float humidity_percent = 50.0f;
        float altitude_meters = 0.0f;
        float air_pressure_hpa = 1013.25f;
        float wind_speed_mps = 0.0f;
        float wind_direction_deg = 0.0f;
    };

    struct PerformanceMetrics {
        std::chrono::microseconds calculation_time;
        float estimated_error_yards;
        bool used_lookup_table;
        bool applied_corrections;
        int interpolation_points_used;
    };

    explicit CarryCalculator(AccuracyMode mode = AccuracyMode::BALANCED);
    ~CarryCalculator();

    // Main calculation method
    float calculateCarry(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm,
        const EnvironmentalConditions* env = nullptr
    );

    // Batch processing for multiple shots (NEON optimized)
    void calculateMultipleCarries(
        const float* ball_speeds_mps,
        const float* launch_angles_deg,
        const float* backspin_rpms,
        const float* sidespin_rpms,
        float* carry_distances_meters,
        int count
    );

    // Configuration
    void setAccuracyMode(AccuracyMode mode);
    AccuracyMode getAccuracyMode() const { return accuracy_mode_; }

    // Performance monitoring
    PerformanceMetrics getLastPerformanceMetrics() const { return last_metrics_; }

    // Table management
    bool loadLookupTable(const std::string& filepath);
    bool saveLookupTable(const std::string& filepath) const;
    void generateLookupTable(bool high_precision = false);

    // Get memory usage in bytes
    size_t getMemoryUsage() const;

    // Validate input parameters
    static bool validateInputs(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm
    );

private:
    AccuracyMode accuracy_mode_;
    std::unique_ptr<LookupTable3D> lookup_table_;
    std::unique_ptr<PhysicsEngine> physics_engine_;
    std::unique_ptr<EnvironmentalCorrections> env_corrections_;
    PerformanceMetrics last_metrics_;

    // Helper methods
    float calculateWithLookupTable(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm
    );

    float calculateWithPhysics(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm,
        const EnvironmentalConditions* env
    );

    float applyEnvironmentalCorrections(
        float base_carry,
        const EnvironmentalConditions& env,
        float ball_speed_mps
    );

    // Convert units
    static float metersToYards(float meters) { return meters * 1.09361f; }
    static float yardsToMeters(float yards) { return yards * 0.9144f; }
    static float mpsToMph(float mps) { return mps * 2.23694f; }
    static float mphToMps(float mph) { return mph * 0.44704f; }
    static float degToRad(float deg) { return deg * 0.0174533f; }
    static float radToDeg(float rad) { return rad * 57.2958f; }
};

} // namespace PiTrac

#endif // CARRY_CALCULATOR_H