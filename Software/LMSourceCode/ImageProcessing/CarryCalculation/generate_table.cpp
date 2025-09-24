// Standalone utility to generate carry distance lookup tables
#include "CarryCalculator.h"
#include <iostream>
#include <string>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "PiTrac Carry Distance Table Generator" << std::endl;
    std::cout << "======================================" << std::endl;

    bool high_precision = false;
    std::string output_file = "carry_table.plt3";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--high-precision" || arg == "-hp") {
            high_precision = true;
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                output_file = argv[++i];
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -hp, --high-precision  Use RK4 integration (slower, more accurate)" << std::endl;
            std::cout << "  -o, --output FILE      Output file path (default: carry_table.plt3)" << std::endl;
            std::cout << "  -h, --help            Show this help message" << std::endl;
            return 0;
        }
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Precision mode: " << (high_precision ? "HIGH (RK4)" : "NORMAL (Simplified)") << std::endl;
    std::cout << "  Output file: " << output_file << std::endl;
    std::cout << std::endl;

    // Create calculator
    PiTrac::CarryCalculator calculator(PiTrac::CarryCalculator::AccuracyMode::ACCURATE);

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Generate the table
    std::cout << "Generating lookup table..." << std::endl;
    calculator.generateLookupTable(high_precision);

    // Save the table
    std::cout << "Saving table to " << output_file << "..." << std::endl;
    if (!calculator.saveLookupTable(output_file)) {
        std::cerr << "Error: Failed to save lookup table!" << std::endl;
        return 1;
    }

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << std::endl;
    std::cout << "Table generation complete!" << std::endl;
    std::cout << "Time taken: " << duration.count() << " seconds" << std::endl;
    std::cout << "File size: " << (512 * 1024) / 1024 << " KB" << std::endl;

    // Test the generated table
    std::cout << std::endl;
    std::cout << "Testing generated table..." << std::endl;

    PiTrac::CarryCalculator test_calc(PiTrac::CarryCalculator::AccuracyMode::BALANCED);
    if (!test_calc.loadLookupTable(output_file)) {
        std::cerr << "Error: Failed to load generated table for testing!" << std::endl;
        return 1;
    }

    // Test cases
    struct TestCase {
        float speed_mps;
        float angle_deg;
        float backspin_rpm;
        const char* description;
    };

    TestCase test_cases[] = {
        {70.0f, 10.0f, 2500.0f, "Driver - Low launch"},
        {70.0f, 14.0f, 2200.0f, "Driver - Optimal"},
        {50.0f, 20.0f, 4000.0f, "7 Iron"},
        {35.0f, 30.0f, 6000.0f, "Wedge"},
        {25.0f, -2.0f, 1000.0f, "Punch shot"}
    };

    for (const auto& test : test_cases) {
        float carry = test_calc.calculateCarry(
            test.speed_mps, test.angle_deg, test.backspin_rpm, 0
        );

        std::cout << test.description << ": ";
        std::cout << carry << " meters (";
        std::cout << carry * 1.09361f << " yards)" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Success! Table ready for use." << std::endl;

    return 0;
}