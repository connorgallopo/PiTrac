"""Configuration Manager for PiTrac Web Server

Handles reading and writing JSON configuration files with a two-tier system:
1. System defaults: /etc/pitrac/golf_sim_config.json (read-only)
2. User overrides: ~/.pitrac/config/user_settings.json (read-write, sparse)
"""

import json
import logging
import shutil
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger(__name__)


class ConfigurationManager:
    """Manages PiTrac configuration with JSON-based system"""
    
    def __init__(self):
        # Configuration file paths
        self.system_config_path = Path("/etc/pitrac/golf_sim_config.json")
        self.user_settings_path = Path.home() / ".pitrac" / "config" / "user_settings.json"
        self.backup_dir = Path.home() / ".pitrac" / "backups"
        
        # Cache for loaded configurations
        self.system_config: Dict[str, Any] = {}
        self.user_settings: Dict[str, Any] = {}
        self.merged_config: Dict[str, Any] = {}
        
        # Track which parameters require restart
        self.restart_required_params = {
            "gs_config.cameras.kCamera1Gain",
            "gs_config.cameras.kCamera2Gain", 
            "gs_config.ipc_interface.kWebActiveMQHostAddress",
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectPort",
            "gs_config.golf_simulator_interfaces.E6.kE6ConnectPort",
        }
        
        # Load configurations
        self.reload()
    
    def reload(self) -> None:
        """Reload all configuration files"""
        self.system_config = self._load_json(self.system_config_path)
        self.user_settings = self._load_json(self.user_settings_path)
        self.merged_config = self._merge_configs()
        logger.info(f"Loaded configuration: {len(self.user_settings)} user overrides")
    
    def _load_json(self, path: Path) -> Dict[str, Any]:
        """Load JSON file safely"""
        if not path.exists():
            return {}
        
        try:
            with open(path, 'r') as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            logger.error(f"Failed to load {path}: {e}")
            return {}
    
    def _save_json(self, path: Path, data: Dict[str, Any]) -> bool:
        """Save JSON file with proper formatting"""
        try:
            # Ensure directory exists
            path.parent.mkdir(parents=True, exist_ok=True)
            
            # Write to temporary file first
            temp_path = path.with_suffix('.tmp')
            with open(temp_path, 'w') as f:
                json.dump(data, f, indent=2, sort_keys=True)
            
            # Atomic rename
            temp_path.replace(path)
            logger.info(f"Saved configuration to {path}")
            return True
            
        except (IOError, OSError) as e:
            logger.error(f"Failed to save {path}: {e}")
            return False
    
    def _merge_configs(self) -> Dict[str, Any]:
        """Merge system defaults with user overrides"""
        # Deep merge: user settings override system defaults
        def deep_merge(base: Dict, override: Dict) -> Dict:
            """Recursively merge override into base"""
            result = base.copy()
            for key, value in override.items():
                if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                    result[key] = deep_merge(result[key], value)
                else:
                    result[key] = value
            return result
        
        return deep_merge(self.system_config, self.user_settings)
    
    def get_config(self, key: Optional[str] = None) -> Any:
        """Get configuration value or entire config
        
        Args:
            key: Dot-notation path (e.g., 'gs_config.cameras.kCamera1Gain')
                 If None, returns entire merged config
        
        Returns:
            Configuration value or None if not found
        """
        if key is None:
            return self.merged_config
        
        # Navigate nested dictionary using dot notation
        value = self.merged_config
        for part in key.split('.'):
            if isinstance(value, dict) and part in value:
                value = value[part]
            else:
                return None
        
        return value
    
    def get_default(self, key: Optional[str] = None) -> Any:
        """Get default system configuration value"""
        if key is None:
            return self.system_config
        
        value = self.system_config
        for part in key.split('.'):
            if isinstance(value, dict) and part in value:
                value = value[part]
            else:
                return None
        
        return value
    
    def get_user_settings(self) -> Dict[str, Any]:
        """Get only user overrides"""
        return self.user_settings.copy()
    
    def set_config(self, key: str, value: Any) -> Tuple[bool, str, bool]:
        """Set configuration value
        
        Args:
            key: Dot-notation path
            value: New value
        
        Returns:
            Tuple of (success, message, requires_restart)
        """
        # Check if value is same as default
        default_value = self.get_default(key)
        
        if value == default_value:
            # Remove from user settings if it's the default
            if self._delete_from_dict(self.user_settings, key):
                self._save_json(self.user_settings_path, self.user_settings)
                self.reload()
                return True, f"Reset {key} to default value", key in self.restart_required_params
            return True, "Value already at default", False
        
        # Set in user settings
        if self._set_in_dict(self.user_settings, key, value):
            # Backup before saving
            self._backup_config()
            
            if self._save_json(self.user_settings_path, self.user_settings):
                self.reload()
                requires_restart = key in self.restart_required_params
                return True, f"Set {key} = {value}", requires_restart
            
            return False, "Failed to save configuration", False
        
        return False, "Failed to set value", False
    
    def _set_in_dict(self, d: Dict[str, Any], key: str, value: Any) -> bool:
        """Set value in nested dictionary using dot notation"""
        parts = key.split('.')
        current = d
        
        # Navigate/create nested structure
        for part in parts[:-1]:
            if part not in current:
                current[part] = {}
            elif not isinstance(current[part], dict):
                return False  # Path blocked by non-dict value
            current = current[part]
        
        # Set the value
        current[parts[-1]] = value
        return True
    
    def _delete_from_dict(self, d: Dict[str, Any], key: str) -> bool:
        """Delete value from nested dictionary using dot notation"""
        parts = key.split('.')
        current = d
        
        # Navigate to parent
        for part in parts[:-1]:
            if isinstance(current, dict) and part in current:
                current = current[part]
            else:
                return False  # Key doesn't exist
        
        # Delete the key
        if isinstance(current, dict) and parts[-1] in current:
            del current[parts[-1]]
            
            # Clean up empty parent dicts
            self._cleanup_empty_dicts(d)
            return True
        
        return False
    
    def _cleanup_empty_dicts(self, d: Dict[str, Any]) -> None:
        """Remove empty nested dictionaries"""
        keys_to_delete = []
        
        for key, value in d.items():
            if isinstance(value, dict):
                self._cleanup_empty_dicts(value)
                if not value:  # Empty dict
                    keys_to_delete.append(key)
        
        for key in keys_to_delete:
            del d[key]
    
    def reset_all(self) -> Tuple[bool, str]:
        """Reset all user settings to defaults"""
        # Backup first
        self._backup_config()
        
        # Clear user settings
        self.user_settings = {}
        
        if self._save_json(self.user_settings_path, self.user_settings):
            self.reload()
            return True, "Reset all settings to defaults"
        
        return False, "Failed to reset configuration"
    
    def _backup_config(self) -> Optional[Path]:
        """Create backup of current user settings"""
        if not self.user_settings:
            return None
        
        try:
            self.backup_dir.mkdir(parents=True, exist_ok=True)
            
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_path = self.backup_dir / f"user_settings_{timestamp}.json"
            
            shutil.copy2(self.user_settings_path, backup_path)
            logger.info(f"Created backup: {backup_path}")
            return backup_path
            
        except (IOError, OSError) as e:
            logger.error(f"Failed to create backup: {e}")
            return None
    
    def get_diff(self) -> Dict[str, Any]:
        """Get differences between user settings and defaults
        
        Returns:
            Dictionary showing what's different from defaults
        """
        diff = {}
        
        def compare_nested(user: Dict, default: Dict, path: str = "") -> None:
            for key, value in user.items():
                current_path = f"{path}.{key}" if path else key
                
                if key not in default:
                    # New key not in defaults
                    diff[current_path] = {"user": value, "default": None}
                elif isinstance(value, dict) and isinstance(default.get(key), dict):
                    # Recursive comparison
                    compare_nested(value, default[key], current_path)
                elif value != default.get(key):
                    # Different value
                    diff[current_path] = {"user": value, "default": default[key]}
        
        compare_nested(self.user_settings, self.system_config)
        return diff
    
    def validate_config(self, key: str, value: Any) -> Tuple[bool, str]:
        """Validate configuration value
        
        Args:
            key: Configuration key
            value: Value to validate
        
        Returns:
            Tuple of (is_valid, error_message)
        """
        # Type validation based on key patterns
        if "Gain" in key:
            try:
                float_val = float(value)
                if not 0.5 <= float_val <= 16.0:
                    return False, "Gain must be between 0.5 and 16.0"
            except (TypeError, ValueError):
                return False, "Gain must be a number"
        
        elif "Port" in key:
            try:
                port = int(value)
                if not 1 <= port <= 65535:
                    return False, "Port must be between 1 and 65535"
            except (TypeError, ValueError):
                return False, "Port must be an integer"
        
        elif any(x in key for x in ["Address", "Host"]):
            if value and not isinstance(value, str):
                return False, "Address must be a string"
        
        return True, ""
    
    def get_categories(self) -> Dict[str, List[str]]:
        """Get configuration organized by categories
        
        Returns:
            Dictionary with category names and their parameters
        """
        # Define basic settings that should appear first
        basic_settings = [
            # System mode
            "gs_config.modes.kStartInPuttingMode",
            "gs_config.system.kGolferOrientation",  # right_handed/left_handed
            
            # Camera settings
            "gs_config.cameras.kCamera1Gain",
            "gs_config.cameras.kCamera2Gain",
            
            # Simulator interfaces
            "gs_config.golf_simulator_interfaces.E6.kE6ConnectAddress",
            "gs_config.golf_simulator_interfaces.E6.kE6ConnectPort",
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectAddress",
            "gs_config.golf_simulator_interfaces.GSPro.kGSProConnectPort",
            
            # Ball detection basics
            "gs_config.ball_identification.kBallDetectionMethod",  # legacy/experimental/experimental_sahi
            "gs_config.ball_identification.kUseCLAHEProcessing",
            "gs_config.ball_identification.kCLAHEClipLimit",
            "gs_config.ball_position.kExpectedBallRadiusPixelsAt40cm",
            
            # Strobing (LED timing) adjustments
            "gs_config.strobing.kStandardBallSpeedAdjustmentPercent",
            "gs_config.strobing.kPracticeBallSpeedAdjustmentPercent",
            "gs_config.strobing.kPuttingBallSpeedAdjustmentPercent",
            
            # Common storage settings
            "gs_config.system.kLogExposureImages",
            "gs_config.system.kLogSpinAnalysisImages",
            "gs_config.system.kLogWebServerImages",
            "gs_config.system.kUseUniqueFileNames",  # unique_diagnostic_files
            "gs_config.system.kImageDirectory",
            "gs_config.system.kWebShareDirectory",
            
            # Network
            "gs_config.ipc_interface.kWebActiveMQHostAddress",
            "gs_config.ipc_interface.kWebRefreshSeconds",
            
            # Spin calculation
            "gs_config.spin.kSkipSpinCalculation",
            "gs_config.spin.kWriteCSVFiles",
        ]
        
        categories = {
            "Basic": [],
            "Cameras": [],
            "Simulators": [],
            "Ball Detection": [],
            "Storage": [],
            "Network": [],
            "Logging": [],
            "Spin Analysis": [],
            "Calibration": [],
            "Advanced": []
        }
        
        def categorize_key(key: str) -> str:
            # Check if it's a basic setting first
            if key in basic_settings:
                return "Basic"
            elif "camera" in key.lower() or "Camera" in key:
                return "Cameras"
            elif any(x in key for x in ["GSPro", "E6", "golf_simulator"]):
                return "Simulators"
            elif "ball" in key.lower() or "hough" in key.lower():
                return "Ball Detection"
            elif any(x in key.lower() for x in ["log", "dir", "path", "file"]):
                return "Storage"
            elif any(x in key.lower() for x in ["network", "broker", "port", "address"]):
                return "Network"
            elif "logging" in key.lower():
                return "Logging"
            elif "spin" in key.lower():
                return "Spin Analysis"
            elif "calibrat" in key.lower():
                return "Calibration"
            else:
                return "Advanced"
        
        def extract_keys(d: Dict, prefix: str = "") -> None:
            for key, value in d.items():
                full_key = f"{prefix}.{key}" if prefix else key
                
                if isinstance(value, dict):
                    extract_keys(value, full_key)
                else:
                    category = categorize_key(full_key)
                    categories[category].append(full_key)
        
        extract_keys(self.merged_config)
        
        # Sort keys within each category
        for category in categories:
            categories[category].sort()
        
        # Remove empty categories
        return {k: v for k, v in categories.items() if v}
    
    def export_config(self) -> Dict[str, Any]:
        """Export current configuration for backup/sharing"""
        return {
            "version": "1.0",
            "exported_at": datetime.now().isoformat(),
            "user_settings": self.user_settings,
            "system_version": self.system_config.get("version", "unknown")
        }
    
    def import_config(self, config_data: Dict[str, Any]) -> Tuple[bool, str]:
        """Import configuration from exported data"""
        if "user_settings" not in config_data:
            return False, "Invalid configuration format"
        
        # Backup current settings
        self._backup_config()
        
        # Import new settings
        self.user_settings = config_data["user_settings"]
        
        if self._save_json(self.user_settings_path, self.user_settings):
            self.reload()
            return True, "Configuration imported successfully"
        
        return False, "Failed to import configuration"