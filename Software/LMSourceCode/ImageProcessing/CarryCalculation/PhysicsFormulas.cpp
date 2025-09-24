#include "PhysicsFormulas.h"
#include <cmath>
#include <algorithm>

namespace PiTrac {

PhysicsEngine::PhysicsEngine()
    : constants_computed_(false) {
    precomputeConstants();
}

void PhysicsEngine::precomputeConstants() {
    cached_drag_factor_ = 0.5f * DRAG_COEFFICIENT * BALL_AREA / BALL_MASS;
    cached_magnus_factor_ = (8.0f / 3.0f) * M_PI * std::pow(BALL_RADIUS, 3) / BALL_MASS;
    constants_computed_ = true;
}

float PhysicsEngine::calculateCarrySimplified(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm) {

    float theta = launch_angle_deg * M_PI / 180.0f;
    float v0_x = ball_speed_mps * std::cos(theta);
    float v0_y = ball_speed_mps * std::sin(theta);

    // Time of flight (simplified - ignoring drag on vertical component)
    float t_flight = 2.0f * v0_y / GRAVITY;

    // Average horizontal velocity with drag
    float drag_deceleration = cached_drag_factor_ * AIR_DENSITY * v0_x;
    float avg_vx = v0_x * (1.0f - std::exp(-drag_deceleration * t_flight)) / (drag_deceleration * t_flight);

    // Magnus lift effect (simplified)
    float lift_multiplier = 1.0f + (backspin_rpm / 3000.0f) * 0.12f;
    t_flight *= lift_multiplier;

    // Sidespin reduction
    float sidespin_factor = 1.0f - (std::abs(sidespin_rpm) / 1000.0f) * 0.02f;

    return avg_vx * t_flight * sidespin_factor;
}

float PhysicsEngine::calculateCarryRK4(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm,
    float sidespin_rpm,
    float air_density,
    float time_step) {

    float theta = launch_angle_deg * M_PI / 180.0f;

    State state;
    state.x = 0;
    state.y = 0;
    state.z = 0;
    state.vx = ball_speed_mps * std::cos(theta);
    state.vy = ball_speed_mps * std::sin(theta);
    state.vz = 0;

    float time = 0;
    float max_time = 10.0f;

    while (state.y >= 0 && time < max_time) {
        state = integrate(state, time_step, backspin_rpm, sidespin_rpm, air_density);
        time += time_step;

        // Early termination if ball is descending and near ground
        if (state.vy < 0 && state.y < 0.1f) {
            break;
        }
    }

    return state.x;
}

PhysicsEngine::State PhysicsEngine::integrate(
    State state,
    float dt,
    float backspin_rpm,
    float sidespin_rpm,
    float air_density) {

    Derivative k1 = evaluate(state, 0, Derivative{}, backspin_rpm, sidespin_rpm, air_density);
    Derivative k2 = evaluate(state, dt * 0.5f, k1, backspin_rpm, sidespin_rpm, air_density);
    Derivative k3 = evaluate(state, dt * 0.5f, k2, backspin_rpm, sidespin_rpm, air_density);
    Derivative k4 = evaluate(state, dt, k3, backspin_rpm, sidespin_rpm, air_density);

    const float sixth = 1.0f / 6.0f;

    state.x += sixth * dt * (k1.dx + 2 * k2.dx + 2 * k3.dx + k4.dx);
    state.y += sixth * dt * (k1.dy + 2 * k2.dy + 2 * k3.dy + k4.dy);
    state.z += sixth * dt * (k1.dz + 2 * k2.dz + 2 * k3.dz + k4.dz);
    state.vx += sixth * dt * (k1.dvx + 2 * k2.dvx + 2 * k3.dvx + k4.dvx);
    state.vy += sixth * dt * (k1.dvy + 2 * k2.dvy + 2 * k3.dvy + k4.dvy);
    state.vz += sixth * dt * (k1.dvz + 2 * k2.dvz + 2 * k3.dvz + k4.dvz);

    return state;
}

PhysicsEngine::Derivative PhysicsEngine::evaluate(
    const State& initial,
    float dt,
    const Derivative& d,
    float backspin_rpm,
    float sidespin_rpm,
    float air_density) {

    State state;
    state.x = initial.x + d.dx * dt;
    state.y = initial.y + d.dy * dt;
    state.z = initial.z + d.dz * dt;
    state.vx = initial.vx + d.dvx * dt;
    state.vy = initial.vy + d.dvy * dt;
    state.vz = initial.vz + d.dvz * dt;

    Derivative output;
    output.dx = state.vx;
    output.dy = state.vy;
    output.dz = state.vz;

    // Calculate forces
    auto drag = calculateDragVector(state.vx, state.vy, state.vz, air_density);
    auto magnus = calculateMagnusVector(state.vx, state.vy, state.vz, backspin_rpm, sidespin_rpm);

    output.dvx = -drag[0] + magnus[0];
    output.dvy = -drag[1] + magnus[1] - GRAVITY;
    output.dvz = -drag[2] + magnus[2];

    return output;
}

std::array<float, 3> PhysicsEngine::calculateDragVector(
    float vx, float vy, float vz,
    float air_density) {

    float v = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (v < 0.001f) return {0, 0, 0};

    float drag_magnitude = cached_drag_factor_ * air_density * v * v;

    return {
        drag_magnitude * vx / v,
        drag_magnitude * vy / v,
        drag_magnitude * vz / v
    };
}

std::array<float, 3> PhysicsEngine::calculateMagnusVector(
    float vx, float vy, float vz,
    float backspin_rpm,
    float sidespin_rpm) {

    float v = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (v < 0.001f) return {0, 0, 0};

    // Convert RPM to rad/s
    float backspin_rads = backspin_rpm * 2 * M_PI / 60.0f;
    float sidespin_rads = sidespin_rpm * 2 * M_PI / 60.0f;

    // Magnus force perpendicular to velocity and spin axis
    float magnus_coefficient = cached_magnus_factor_ * AIR_DENSITY;

    // Simplified Magnus calculation
    float lift_force = magnus_coefficient * backspin_rads * v;
    float side_force = magnus_coefficient * sidespin_rads * v;

    return {
        -side_force * vz / v,
        lift_force * vx / v,
        side_force * vx / v
    };
}

float PhysicsEngine::adjustForAltitude(float carry_meters, float altitude_meters) {
    // Air density decreases with altitude
    // Approximately 12% less dense per 1000m
    float density_factor = std::exp(-altitude_meters / 8400.0f);
    float adjustment = 1.0f + (1.0f - density_factor) * 0.05f; // 5% more carry per density reduction
    return carry_meters * adjustment;
}

float PhysicsEngine::adjustForTemperature(float carry_meters, float temperature_celsius) {
    // Warmer air is less dense
    // Approximately 0.5% more carry per 10°C above 20°C
    float temp_diff = temperature_celsius - 20.0f;
    float adjustment = 1.0f + (temp_diff / 10.0f) * 0.005f;
    return carry_meters * adjustment;
}

float PhysicsEngine::adjustForHumidity(float carry_meters, float humidity_percent) {
    // Humid air is slightly less dense
    // Maximum effect about 1% at 100% humidity vs 0%
    float adjustment = 1.0f + (humidity_percent / 100.0f) * 0.01f;
    return carry_meters * adjustment;
}

float PhysicsEngine::adjustForWind(float carry_meters, float wind_speed_mps, float wind_angle_deg) {
    // Headwind/tailwind adjustment
    float wind_component = wind_speed_mps * std::cos(wind_angle_deg * M_PI / 180.0f);
    // Approximately 2 yards per mph of wind
    float adjustment_yards = wind_component * 2.0f * 2.237f; // Convert m/s to mph then to yards
    return carry_meters + adjustment_yards * 0.9144f; // Convert back to meters
}

// SimpleBallistics implementations

float SimpleBallistics::calculateCarryFast(
    float ball_speed_mps,
    float launch_angle_deg,
    float backspin_rpm) {

    // Ultra-simple formula based on empirical data
    // Carry ≈ v² * sin(2θ) / g * spin_factor * drag_factor

    float theta = launch_angle_deg * M_PI / 180.0f;
    float range_vacuum = (ball_speed_mps * ball_speed_mps * std::sin(2 * theta)) / 9.81f;

    // Empirical corrections
    float spin_factor = 1.0f + (backspin_rpm / 3000.0f) * 0.15f;
    float drag_factor = 0.65f; // Accounts for average drag loss

    return range_vacuum * spin_factor * drag_factor;
}

float SimpleBallistics::calculateCarryWithDrag(
    float ball_speed_mps,
    float launch_angle_deg,
    float drag_coefficient) {

    float theta = launch_angle_deg * M_PI / 180.0f;
    float v0 = ball_speed_mps;
    float g = 9.81f;

    // Analytical solution for projectile with quadratic drag
    // This is an approximation using average velocity
    float vx = v0 * std::cos(theta);
    float vy = v0 * std::sin(theta);

    float k = drag_coefficient * 1.225f * 0.001432f / (2 * 0.04593f);
    float t_flight = 2 * vy / (g + k * vy);
    float x_max = (vx / k) * (1 - std::exp(-k * t_flight));

    return x_max;
}

float SimpleBallistics::calculateCarryEmpirical(
    float ball_speed_mph,
    float launch_angle_deg,
    float backspin_rpm) {

    // PGA Tour empirical formula
    // Based on TrackMan data fitting

    float optimal_launch = 13.5f; // Optimal launch angle for max carry
    float launch_factor = 1.0f - std::pow((launch_angle_deg - optimal_launch) / 20.0f, 2);
    launch_factor = std::max(0.5f, launch_factor);

    float spin_ratio = backspin_rpm / (ball_speed_mph * 100);
    float optimal_spin_ratio = 2.5f;
    float spin_factor = 1.0f - 0.1f * std::abs(spin_ratio - optimal_spin_ratio);
    spin_factor = std::max(0.8f, spin_factor);

    // Base carry in yards
    float carry_yards = ball_speed_mph * 2.0f * launch_factor * spin_factor;

    // Convert to meters
    return carry_yards * 0.9144f;
}

} // namespace PiTrac