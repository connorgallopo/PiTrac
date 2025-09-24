#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include <vector>
#include <string>
#include <cstdint>

namespace PiTrac {

class LookupTable3D {
public:
    // Table dimensions - optimized for memory and cache performance
    static constexpr int SPEED_BINS = 32;    // 20-80 m/s range
    static constexpr int ANGLE_BINS = 24;    // -5 to 35 degrees
    static constexpr int SPIN_BINS = 16;     // 0-6000 RPM backspin

    // Parameter ranges
    static constexpr float MIN_SPEED_MPS = 20.0f;
    static constexpr float MAX_SPEED_MPS = 80.0f;
    static constexpr float MIN_ANGLE_DEG = -5.0f;
    static constexpr float MAX_ANGLE_DEG = 35.0f;
    static constexpr float MIN_SPIN_RPM = 0.0f;
    static constexpr float MAX_SPIN_RPM = 6000.0f;

    struct TableEntry {
        float carry_meters;
        uint8_t confidence;  // 0-255 confidence score
    };

    LookupTable3D();
    ~LookupTable3D() = default;

    // Main lookup with trilinear interpolation
    float lookup(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm
    ) const;

    // Fast lookup without interpolation
    float lookupNearest(
        float ball_speed_mps,
        float launch_angle_deg,
        float backspin_rpm,
        float sidespin_rpm
    ) const;

    // Set a single entry
    void setEntry(
        int speed_idx,
        int angle_idx,
        int spin_idx,
        float carry_meters,
        uint8_t confidence = 255
    );

    // Get a single entry
    TableEntry getEntry(
        int speed_idx,
        int angle_idx,
        int spin_idx
    ) const;

    // File I/O
    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;
    bool loadFromBinaryData(const uint8_t* data, size_t size);

    // Generate table using physics simulation
    void generateTable(bool high_precision = false);

    // Memory management
    size_t getMemoryUsage() const;
    void compressTable();  // Reduce precision to save memory
    void decompressTable(); // Restore full precision

    // Validation
    bool isValid() const;
    float getCoverage() const; // Percentage of table filled

    // Debug/Analysis
    void printStatistics() const;
    std::vector<float> getSlice(int dimension, int index) const;

private:
    // Main data storage - using float16 would save memory but needs special handling
    std::vector<std::vector<std::vector<TableEntry>>> table_;

    // Metadata
    bool is_compressed_;
    bool is_valid_;
    int entries_filled_;

    // Helper methods for index calculation
    int getSpeedIndex(float speed_mps) const;
    int getAngleIndex(float angle_deg) const;
    int getSpinIndex(float spin_rpm) const;

    // Get fractional position for interpolation
    float getSpeedFraction(float speed_mps, int index) const;
    float getAngleFraction(float angle_deg, int index) const;
    float getSpinFraction(float spin_rpm, int index) const;

    // Trilinear interpolation
    float trilinearInterpolate(
        float speed_frac,
        float angle_frac,
        float spin_frac,
        int speed_idx,
        int angle_idx,
        int spin_idx
    ) const;

    // Boundary handling
    void clampIndices(int& speed_idx, int& angle_idx, int& spin_idx) const;
    bool isInBounds(float speed_mps, float angle_deg, float spin_rpm) const;

    // Side spin adjustment factor
    float computeSideSpinFactor(float sidespin_rpm) const;
};

// Precomputed table loader for embedded data
class EmbeddedTableLoader {
public:
    static bool loadEmbeddedTable(LookupTable3D& table);
    static const uint8_t* getEmbeddedData();
    static size_t getEmbeddedDataSize();
};

} // namespace PiTrac

#endif // LOOKUP_TABLE_H