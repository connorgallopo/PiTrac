#ifndef NEON_OPTIMIZATIONS_H
#define NEON_OPTIMIZATIONS_H

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include <vector>

namespace PiTrac {

class NEONCarryCalculator {
public:
#ifdef __ARM_NEON
    // Process 4 shots simultaneously using NEON SIMD
    static void calculateBatch4(
        const float* ball_speeds_mps,      // Array of 4 speeds
        const float* launch_angles_deg,    // Array of 4 angles
        const float* backspin_rpms,       // Array of 4 backspins
        const float* sidespin_rpms,       // Array of 4 sidespins
        float* carry_distances_meters     // Output: Array of 4 carries
    );

    // Process multiple shots in batches of 4
    static void calculateBatchN(
        const float* ball_speeds_mps,
        const float* launch_angles_deg,
        const float* backspin_rpms,
        const float* sidespin_rpms,
        float* carry_distances_meters,
        int count
    );

    // Optimized trilinear interpolation using NEON
    static float trilinearInterpolateNEON(
        const float* table_data,
        float speed_frac,
        float angle_frac,
        float spin_frac,
        int speed_idx,
        int angle_idx,
        int spin_idx,
        int speed_bins,
        int angle_bins,
        int spin_bins
    );

    // Fast math functions using NEON
    static float32x4_t sin_neon(float32x4_t x);
    static float32x4_t cos_neon(float32x4_t x);
    static float32x4_t exp_neon(float32x4_t x);
    static float32x4_t sqrt_neon(float32x4_t x);

    // Helper functions for angle conversions
    static float32x4_t deg_to_rad_neon(float32x4_t degrees);
    static float32x4_t rad_to_deg_neon(float32x4_t radians);

private:
    // NEON constants
    static const float32x4_t GRAVITY_VEC;
    static const float32x4_t DEG_TO_RAD;
    static const float32x4_t RAD_TO_DEG;
    static const float32x4_t DRAG_FACTOR;
    static const float32x4_t SPIN_FACTOR;

    // Taylor series approximations for trigonometric functions
    static float32x4_t sin_taylor_neon(float32x4_t x);
    static float32x4_t cos_taylor_neon(float32x4_t x);

#else
    // Fallback implementations for non-ARM platforms
    static void calculateBatch4(
        const float* ball_speeds_mps,
        const float* launch_angles_deg,
        const float* backspin_rpms,
        const float* sidespin_rpms,
        float* carry_distances_meters
    );

    static void calculateBatchN(
        const float* ball_speeds_mps,
        const float* launch_angles_deg,
        const float* backspin_rpms,
        const float* sidespin_rpms,
        float* carry_distances_meters,
        int count
    );
#endif // __ARM_NEON
};

// Utility class for NEON memory alignment
class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t size);
    ~AlignedBuffer();

    float* data() { return data_; }
    const float* data() const { return data_; }
    size_t size() const { return size_; }

    // Check if pointer is 16-byte aligned for NEON
    static bool is_aligned(const void* ptr);

    // Align size to 16-byte boundary
    static size_t align_size(size_t size);

private:
    float* data_;
    size_t size_;
    void* allocated_ptr_;
};

// Performance testing utilities
class NEONBenchmark {
public:
    struct Results {
        double neon_time_ms;
        double scalar_time_ms;
        double speedup_factor;
        bool neon_available;
    };

    // Benchmark NEON vs scalar implementation
    static Results benchmark(int num_iterations = 10000);

    // Check NEON availability at runtime
    static bool is_neon_available();
};

} // namespace PiTrac

#endif // NEON_OPTIMIZATIONS_H