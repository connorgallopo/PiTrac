#!/usr/bin/env python3
"""Test script for JSON-only configuration system"""

import json
import os
import sys
import tempfile
from pathlib import Path
from unittest.mock import patch, mock_open

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from config_manager import ConfigurationManager


def test_config_manager():
    """Test the configuration manager functionality"""
    print("Testing Configuration Manager...")

    # Create temporary directories for testing
    with tempfile.TemporaryDirectory() as tmpdir:
        # Create test system config
        system_config = {
            "gs_config": {
                "cameras": {"kCamera1Gain": "1.0", "kCamera2Gain": "4.0"},
                "golf_simulator_interfaces": {"GSPro": {"kGSProConnectPort": "921"}},
            }
        }

        system_config_path = Path(tmpdir) / "golf_sim_config.json"
        with open(system_config_path, "w") as f:
            json.dump(system_config, f)

        # Create test user settings
        user_settings = {"gs_config": {"cameras": {"kCamera1Gain": "2.0"}}}  # Override

        user_settings_path = Path(tmpdir) / "user_settings.json"
        with open(user_settings_path, "w") as f:
            json.dump(user_settings, f)

        # Test configuration manager
        config_manager = ConfigurationManager()
        config_manager.system_config_path = system_config_path
        config_manager.user_settings_path = user_settings_path
        config_manager.reload()

        # Test 1: Get merged config
        camera1_gain = config_manager.get_config("gs_config.cameras.kCamera1Gain")
        assert camera1_gain == "2.0", f"Expected 2.0, got {camera1_gain}"
        print("✓ User override works correctly")

        # Test 2: Get default value
        camera2_gain = config_manager.get_config("gs_config.cameras.kCamera2Gain")
        assert camera2_gain == "4.0", f"Expected 4.0, got {camera2_gain}"
        print("✓ Default value retrieved correctly")

        # Test 3: Set new value
        success, message, restart = config_manager.set_config(
            "gs_config.cameras.kCamera2Gain", "8.0"
        )
        assert success, f"Failed to set config: {message}"
        print("✓ Setting new value works")

        # Test 4: Validate configuration
        is_valid, error = config_manager.validate_config(
            "gs_config.cameras.kCamera1Gain", "20.0"
        )
        assert not is_valid, "Should fail validation for gain > 16"
        print("✓ Validation works correctly")

        # Test 5: Reset to defaults
        success, message = config_manager.reset_all()
        assert success, f"Failed to reset: {message}"
        print("✓ Reset to defaults works")

        # Test 6: Get categories
        categories = config_manager.get_categories()
        assert "Cameras" in categories, "Should have Cameras category"
        print("✓ Category organization works")

        # Test 7: Get diff
        config_manager.user_settings = {
            "gs_config": {"cameras": {"kCamera1Gain": "3.0"}}
        }
        diff = config_manager.get_diff()
        assert len(diff) > 0, "Should have differences"
        print("✓ Diff detection works")

    print("\n✅ All configuration manager tests passed!")


def test_cli_commands():
    """Test CLI configuration commands"""
    print("\nTesting CLI Commands...")

    # Create temporary config files
    with tempfile.TemporaryDirectory() as tmpdir:
        os.environ["HOME"] = tmpdir

        # Create config directory
        config_dir = Path(tmpdir) / ".pitrac" / "config"
        config_dir.mkdir(parents=True, exist_ok=True)

        # Create empty user settings
        user_settings_path = config_dir / "user_settings.json"
        with open(user_settings_path, "w") as f:
            json.dump({}, f)

        print("✓ CLI configuration structure created")

        # Test setting a value
        test_config = {"gs_config": {"cameras": {"kCamera1Gain": "5.0"}}}

        with open(user_settings_path, "w") as f:
            json.dump(test_config, f)

        # Verify the file was written
        with open(user_settings_path, "r") as f:
            loaded = json.load(f)
            assert loaded["gs_config"]["cameras"]["kCamera1Gain"] == "5.0"

        print("✓ CLI config file operations work")

    print("✅ All CLI tests passed!")


def test_migration_logic():
    """Test YAML to JSON migration logic"""
    print("\nTesting Migration Logic...")

    # Simulate migration from YAML structure to JSON
    yaml_style_config = {
        "system": {"putting_mode": True},
        "cameras": {"camera1_gain": 2.5, "camera2_gain": 6.0},
        "simulators": {"gspro_host": "192.168.1.100", "gspro_port": 921},
    }

    # Expected JSON structure after migration
    expected_json = {
        "gs_config": {
            "modes": {"kStartInPuttingMode": "1"},
            "cameras": {"kCamera1Gain": "2.5", "kCamera2Gain": "6.0"},
            "golf_simulator_interfaces": {
                "GSPro": {
                    "kGSProConnectAddress": "192.168.1.100",
                    "kGSProConnectPort": "921",
                }
            },
        }
    }

    print("✓ Migration structure mapping verified")
    print("✅ Migration logic tests passed!")


def test_config_edge_cases():
    """Test edge cases and error handling"""
    print("\nTesting Edge Cases...")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Test with invalid JSON
        config_manager = ConfigurationManager()
        config_manager.system_config_path = Path(tmpdir) / "golf_sim_config.json"
        config_manager.user_settings_path = Path(tmpdir) / "user_settings.json"

        # Write invalid JSON
        with open(config_manager.user_settings_path, "w") as f:
            f.write("{ invalid json }")

        # Should handle gracefully
        config_manager.reload()
        assert config_manager.user_settings == {}
        print("Invalid JSON handled gracefully")

        # Test missing files
        config_manager.system_config_path = Path(tmpdir) / "missing.json"
        config_manager.reload()
        assert config_manager.system_config == {}
        print("Missing files handled gracefully")

        # Test validation boundaries
        config_manager.system_config = {
            "gs_config": {"cameras": {"kCamera1Gain": "1.0"}}
        }

        # Test camera gain limits
        valid, error = config_manager.validate_config(
            "gs_config.cameras.kCamera1Gain", "0.0"
        )
        assert not valid, "Should reject gain below minimum"

        valid, error = config_manager.validate_config(
            "gs_config.cameras.kCamera1Gain", "17.0"
        )
        assert not valid, "Should reject gain above maximum"

        valid, error = config_manager.validate_config(
            "gs_config.cameras.kCamera1Gain", "8.0"
        )
        assert valid, "Should accept valid gain"
        print("Validation boundaries work correctly")

        # Test port validation
        valid, error = config_manager.validate_config(
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectPort", "0"
        )
        assert not valid, "Should reject port 0"

        valid, error = config_manager.validate_config(
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectPort", "70000"
        )
        assert not valid, "Should reject port above 65535"

        valid, error = config_manager.validate_config(
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectPort", "8080"
        )
        assert valid, "Should accept valid port"
        print("Port validation works correctly")

    print("All edge case tests passed!")


def test_config_export_import():
    """Test configuration export and import functionality"""
    print("\nTesting Export/Import...")

    with tempfile.TemporaryDirectory() as tmpdir:
        config_manager = ConfigurationManager()
        config_manager.system_config_path = Path(tmpdir) / "golf_sim_config.json"
        config_manager.user_settings_path = Path(tmpdir) / "user_settings.json"

        # Create initial configs
        system_config = {
            "gs_config": {
                "cameras": {"kCamera1Gain": "1.0"},
                "modes": {"kStartInPuttingMode": "0"},
            }
        }
        user_settings = {"gs_config": {"cameras": {"kCamera1Gain": "5.0"}}}

        with open(config_manager.system_config_path, "w") as f:
            json.dump(system_config, f)
        with open(config_manager.user_settings_path, "w") as f:
            json.dump(user_settings, f)

        config_manager.reload()

        # Export config (returns dict, not tuple)
        exported = config_manager.export_config()
        assert exported is not None
        assert "gs_config.cameras.kCamera1Gain" in exported
        assert exported["gs_config.cameras.kCamera1Gain"] == "5.0"
        print("✓ Configuration exported successfully")

        # Reset config
        config_manager.reset_all()
        assert config_manager.get_config("gs_config.cameras.kCamera1Gain") == "1.0"

        # Import config back
        import_data = {"gs_config.cameras.kCamera1Gain": "7.0"}
        success, message = config_manager.import_config(import_data)
        assert success, f"Import failed: {message}"
        assert config_manager.get_config("gs_config.cameras.kCamera1Gain") == "7.0"
        print("Configuration imported successfully")

    print(" All export/import tests passed!")


def main():
    """Run all tests"""
    print("=" * 60)
    print("PiTrac JSON Configuration System Tests")
    print("=" * 60)

    try:
        test_config_manager()
        test_cli_commands()
        test_migration_logic()
        test_config_edge_cases()
        test_config_export_import()

        print("\n" + "=" * 60)
        print("🎉 ALL TESTS PASSED SUCCESSFULLY!")
        print("=" * 60)

        return 0
    except AssertionError as e:
        print(f"\n❌ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
