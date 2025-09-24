#ifndef PHYSICS_FORMULAS_H
#define PHYSICS_FORMULAS_H

#include <vector>
#include <array>

namespace PiTrac {

struct TrajectoryPoint {
    float x;      // Horizontal distance (meters)
    float y;      // Height (meters)
    float z;      // Side deviation (meters)
    float vx;     // Velocity x component (m/s)
    float vy;     // Velocity y component (m/s)
    float vz;     // Velocity z component (m/s)
    float time;   // Time (seconds)
};

class PhysicsEngine {
public:
    // Physical constants
    static constexpr float GRAVITY = 9.81f;           // m/s²
    static constexpr float AIR_DENSITY = 1.225f;      // kg/m³ at sea level
    static constexpr float BALL_MASS = 0.04593f;      // kg
    static constexpr float BALL_RADIUS = 0.021335f;   // meters
    static constexpr float BALL_AREA = 0.001432f;     // m² cross-sectional area
    static constexpr float DRAG_COEFFICIENT = 0.2f;   // Golf ball Cd

    PhysicsEngine();
    ~PhysicsEngine() = default;

    // Calculate carry using simplified physics
    float calculateCarrySimplified(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm
    );

    // Calculate carry using Runge-Kutta 4th order integration
    float calculateCarryRK4(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm,
        float air_density = AIR_DENSITY,
        float time_step = 0.001f
    );

    // Calculate full trajectory
    std::vector<TrajectoryPoint> calculateTrajectory(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm,
        float air_density = AIR_DENSITY,
        float time_step = 0.001f,
        float max_time = 10.0f
    );

    // Environmental adjustments
    float adjustForAltitude(float carry_meters, float altitude_meters);
    float adjustForTemperature(float carry_meters, float temperature_celsius);
    float adjustForHumidity(float carry_meters, float humidity_percent);
    float adjustForWind(float carry_meters, float wind_speed_mps, float wind_angle_deg);

    // Helper calculations
    static float calculateAirDensity(float temperature_celsius, float altitude_meters, float humidity_percent);
    static float calculateDragForce(float velocity, float air_density);
    static float calculateMagnusForce(float velocity, float spin_rpm);
    static float calculateSpinDecay(float initial_spin_rpm, float time);

private:
    // RK4 integration step
    struct State {
        float x, y, z;     // Position
        float vx, vy, vz;  // Velocity
    };

    struct Derivative {
        float dx, dy, dz;     // Position derivatives
        float dvx, dvy, dvz;  // Velocity derivatives
    };

    // RK4 helper functions
    Derivative evaluate(
        const State& initial,
        float dt,
        const Derivative& d,
        float backspin_rpm,
        float sidespin_rpm,
        float air_density
    );

    State integrate(
        State state,
        float dt,
        float backspin_rpm,
        float sidespin_rpm,
        float air_density
    );

    // Force calculations
    std::array<float, 3> calculateDragVector(
        float vx, float vy, float vz,
        float air_density
    );

    std::array<float, 3> calculateMagnusVector(
        float vx, float vy, float vz,
        float backspin_rpm,
        float sidespin_rpm
    );

    // Fast approximations
    float fastSqrt(float x);
    float fastExp(float x);
    float fastSin(float x);
    float fastCos(float x);

    // Optimization: pre-computed values
    void precomputeConstants();

    // Cache for frequently used values
    float cached_drag_factor_;
    float cached_magnus_factor_;
    bool constants_computed_;
};

// Simplified ballistics for very fast calculation
class SimpleBallistics {
public:
    // Ultra-fast approximation (<0.1ms)
    static float calculateCarryFast(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm
    );

    // Analytical solution with drag (no spin)
    static float calculateCarryWithDrag(
        float ball_speed_mps,
        float launch_angle_deg,
        float drag_coefficient = 0.2f
    );

    // Empirical formula based on PGA Tour data
    static float calculateCarryEmpirical(
        float ball_speed_mph,
        float launch_angle_deg,
        float backspin_rpm
    );
};

} // namespace PiTrac

#endif // PHYSICS_FORMULAS_H