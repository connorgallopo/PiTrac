#include "NEONOptimizations.h"
#include <cmath>
#include <cstring>
#include <chrono>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace PiTrac {

#ifdef __ARM_NEON

// Static NEON constants
const float32x4_t NEONCarryCalculator::GRAVITY_VEC = vdupq_n_f32(9.81f);
const float32x4_t NEONCarryCalculator::DEG_TO_RAD = vdupq_n_f32(0.0174533f);
const float32x4_t NEONCarryCalculator::RAD_TO_DEG = vdupq_n_f32(57.2958f);
const float32x4_t NEONCarryCalculator::DRAG_FACTOR = vdupq_n_f32(0.65f);
const float32x4_t NEONCarryCalculator::SPIN_FACTOR = vdupq_n_f32(0.00005f);

void NEONCarryCalculator::calculateBatch4(
    const float* ball_speeds_mps,
    const float* launch_angles_deg,
    const float* backspin_rpms,
    const float* sidespin_rpms,
    float* carry_distances_meters) {

    // Load 4 values at once
    float32x4_t speeds = vld1q_f32(ball_speeds_mps);
    float32x4_t angles = vld1q_f32(launch_angles_deg);
    float32x4_t backspins = vld1q_f32(backspin_rpms);
    float32x4_t sidespins = vld1q_f32(sidespin_rpms);

    // Convert angles to radians
    float32x4_t angles_rad = vmulq_f32(angles, DEG_TO_RAD);

    // Calculate velocity components
    float32x4_t vx = vmulq_f32(speeds, cos_neon(angles_rad));
    float32x4_t vy = vmulq_f32(speeds, sin_neon(angles_rad));

    // Calculate time of flight (simplified)
    float32x4_t two = vdupq_n_f32(2.0f);
    float32x4_t t_flight = vdivq_f32(vmulq_f32(two, vy), GRAVITY_VEC);

    // Apply drag factor
    float32x4_t vx_with_drag = vmulq_f32(vx, DRAG_FACTOR);

    // Apply spin effect
    float32x4_t spin_mult = vmlaq_f32(vdupq_n_f32(1.0f), backspins, SPIN_FACTOR);
    t_flight = vmulq_f32(t_flight, spin_mult);

    // Calculate carry distance
    float32x4_t carry = vmulq_f32(vx_with_drag, t_flight);

    // Apply sidespin reduction
    float32x4_t sidespin_abs = vabsq_f32(sidespins);
    float32x4_t sidespin_factor = vmlsq_f32(vdupq_n_f32(1.0f),
                                           sidespin_abs,
                                           vdupq_n_f32(0.00002f));
    carry = vmulq_f32(carry, sidespin_factor);

    // Store results
    vst1q_f32(carry_distances_meters, carry);
}

void NEONCarryCalculator::calculateBatchN(
    const float* ball_speeds_mps,
    const float* launch_angles_deg,
    const float* backspin_rpms,
    const float* sidespin_rpms,
    float* carry_distances_meters,
    int count) {

    int i = 0;

    // Process in batches of 4
    for (; i <= count - 4; i += 4) {
        calculateBatch4(
            ball_speeds_mps + i,
            launch_angles_deg + i,
            backspin_rpms + i,
            sidespin_rpms + i,
            carry_distances_meters + i
        );
    }

    // Process remaining elements
    for (; i < count; i++) {
        float theta = launch_angles_deg[i] * 0.0174533f;
        float vx = ball_speeds_mps[i] * std::cos(theta);
        float vy = ball_speeds_mps[i] * std::sin(theta);
        float t_flight = 2.0f * vy / 9.81f;

        // Apply corrections
        float carry = vx * t_flight * 0.65f;
        carry *= (1.0f + backspin_rpms[i] * 0.00005f);
        carry *= (1.0f - std::abs(sidespin_rpms[i]) * 0.00002f);

        carry_distances_meters[i] = carry;
    }
}

float32x4_t NEONCarryCalculator::sin_neon(float32x4_t x) {
    // Fast sine approximation using Taylor series
    // sin(x) ≈ x - x³/3! + x⁵/5! - x⁷/7!

    float32x4_t x2 = vmulq_f32(x, x);
    float32x4_t x3 = vmulq_f32(x2, x);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);

    float32x4_t result = x;
    result = vmlsq_f32(result, x3, vdupq_n_f32(1.0f / 6.0f));
    result = vmlaq_f32(result, x5, vdupq_n_f32(1.0f / 120.0f));
    result = vmlsq_f32(result, x7, vdupq_n_f32(1.0f / 5040.0f));

    return result;
}

float32x4_t NEONCarryCalculator::cos_neon(float32x4_t x) {
    // Fast cosine approximation using Taylor series
    // cos(x) ≈ 1 - x²/2! + x⁴/4! - x⁶/6!

    float32x4_t x2 = vmulq_f32(x, x);
    float32x4_t x4 = vmulq_f32(x2, x2);
    float32x4_t x6 = vmulq_f32(x4, x2);

    float32x4_t result = vdupq_n_f32(1.0f);
    result = vmlsq_f32(result, x2, vdupq_n_f32(0.5f));
    result = vmlaq_f32(result, x4, vdupq_n_f32(1.0f / 24.0f));
    result = vmlsq_f32(result, x6, vdupq_n_f32(1.0f / 720.0f));

    return result;
}

float32x4_t NEONCarryCalculator::exp_neon(float32x4_t x) {
    // Fast exponential approximation
    // Using the fact that e^x = 2^(x * log2(e))

    const float32x4_t log2_e = vdupq_n_f32(1.442695f);
    float32x4_t y = vmulq_f32(x, log2_e);

    // Extract integer and fractional parts
    int32x4_t i = vcvtq_s32_f32(y);
    float32x4_t f = vsubq_f32(y, vcvtq_f32_s32(i));

    // Polynomial approximation of 2^f for f in [0,1]
    float32x4_t p = vdupq_n_f32(1.0f);
    p = vmlaq_f32(p, f, vdupq_n_f32(0.693147f));
    p = vmlaq_f32(p, vmulq_f32(f, f), vdupq_n_f32(0.240227f));

    // Scale by 2^i
    // This is a simplified version - proper implementation would use bit manipulation
    return p;
}

float32x4_t NEONCarryCalculator::sqrt_neon(float32x4_t x) {
    // Use ARM NEON's built-in square root approximation
    float32x4_t result = vrsqrteq_f32(x);  // 1/sqrt(x) approximation

    // Newton-Raphson refinement for better accuracy
    float32x4_t half_x = vmulq_f32(x, vdupq_n_f32(0.5f));
    float32x4_t three_halfs = vdupq_n_f32(1.5f);

    result = vmulq_f32(result, vmlsq_f32(three_halfs,
                                         vmulq_f32(half_x, result),
                                         result));

    // Convert from 1/sqrt(x) to sqrt(x)
    return vmulq_f32(x, result);
}

#else // Non-ARM fallback implementations

void NEONCarryCalculator::calculateBatch4(
    const float* ball_speeds_mps,
    const float* launch_angles_deg,
    const float* backspin_rpms,
    const float* sidespin_rpms,
    float* carry_distances_meters) {

    for (int i = 0; i < 4; i++) {
        float theta = launch_angles_deg[i] * 0.0174533f;
        float vx = ball_speeds_mps[i] * std::cos(theta);
        float vy = ball_speeds_mps[i] * std::sin(theta);
        float t_flight = 2.0f * vy / 9.81f;

        float carry = vx * t_flight * 0.65f;
        carry *= (1.0f + backspin_rpms[i] * 0.00005f);
        carry *= (1.0f - std::abs(sidespin_rpms[i]) * 0.00002f);

        carry_distances_meters[i] = carry;
    }
}

void NEONCarryCalculator::calculateBatchN(
    const float* ball_speeds_mps,
    const float* launch_angles_deg,
    const float* backspin_rpms,
    const float* sidespin_rpms,
    float* carry_distances_meters,
    int count) {

    for (int i = 0; i < count; i++) {
        float theta = launch_angles_deg[i] * 0.0174533f;
        float vx = ball_speeds_mps[i] * std::cos(theta);
        float vy = ball_speeds_mps[i] * std::sin(theta);
        float t_flight = 2.0f * vy / 9.81f;

        float carry = vx * t_flight * 0.65f;
        carry *= (1.0f + backspin_rpms[i] * 0.00005f);
        carry *= (1.0f - std::abs(sidespin_rpms[i]) * 0.00002f);

        carry_distances_meters[i] = carry;
    }
}

#endif // __ARM_NEON

// AlignedBuffer implementation

AlignedBuffer::AlignedBuffer(size_t size) : size_(size) {
    size_t aligned_size = align_size(size * sizeof(float));

#ifdef _WIN32
    allocated_ptr_ = _aligned_malloc(aligned_size, 16);
#else
    if (posix_memalign(&allocated_ptr_, 16, aligned_size) != 0) {
        allocated_ptr_ = nullptr;
    }
#endif

    data_ = static_cast<float*>(allocated_ptr_);

    if (data_) {
        std::memset(data_, 0, aligned_size);
    }
}

AlignedBuffer::~AlignedBuffer() {
    if (allocated_ptr_) {
#ifdef _WIN32
        _aligned_free(allocated_ptr_);
#else
        free(allocated_ptr_);
#endif
    }
}

bool AlignedBuffer::is_aligned(const void* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & 15) == 0;
}

size_t AlignedBuffer::align_size(size_t size) {
    return (size + 15) & ~15;
}

// Benchmark implementation

NEONBenchmark::Results NEONBenchmark::benchmark(int num_iterations) {
    Results results;
    results.neon_available = is_neon_available();

    const int batch_size = 1000;
    AlignedBuffer speeds(batch_size);
    AlignedBuffer angles(batch_size);
    AlignedBuffer backspins(batch_size);
    AlignedBuffer sidespins(batch_size);
    AlignedBuffer carries(batch_size);

    // Initialize test data
    for (int i = 0; i < batch_size; i++) {
        speeds.data()[i] = 40.0f + (i % 40);
        angles.data()[i] = 10.0f + (i % 20);
        backspins.data()[i] = 2000.0f + (i % 3000);
        sidespins.data()[i] = -500.0f + (i % 1000);
    }

    // Benchmark NEON implementation
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; iter++) {
        NEONCarryCalculator::calculateBatchN(
            speeds.data(), angles.data(),
            backspins.data(), sidespins.data(),
            carries.data(), batch_size
        );
    }
    auto end = std::chrono::high_resolution_clock::now();
    results.neon_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Benchmark scalar implementation
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; iter++) {
        for (int i = 0; i < batch_size; i++) {
            float theta = angles.data()[i] * 0.0174533f;
            float vx = speeds.data()[i] * std::cos(theta);
            float vy = speeds.data()[i] * std::sin(theta);
            float t_flight = 2.0f * vy / 9.81f;

            float carry = vx * t_flight * 0.65f;
            carry *= (1.0f + backspins.data()[i] * 0.00005f);
            carry *= (1.0f - std::abs(sidespins.data()[i]) * 0.00002f);

            carries.data()[i] = carry;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    results.scalar_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    results.speedup_factor = results.scalar_time_ms / results.neon_time_ms;

    return results;
}

bool NEONBenchmark::is_neon_available() {
#ifdef __ARM_NEON
    return true;
#else
    return false;
#endif
}

} // namespace PiTrac