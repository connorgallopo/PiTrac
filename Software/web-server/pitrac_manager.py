"""
PiTrac Process Manager - Manages the lifecycle of the pitrac_lm process
"""

import asyncio
import logging
import os
import signal
import subprocess
from pathlib import Path
from typing import Optional, Dict, Any
import yaml
import json

logger = logging.getLogger(__name__)


class PiTracProcessManager:
    """Manages the PiTrac launch monitor process"""

    def __init__(self):
        self.process: Optional[subprocess.Popen] = None
        self.pitrac_binary = "/usr/lib/pitrac/pitrac_lm"
        self.config_file = "/etc/pitrac/golf_sim_config.json"
        self.pitrac_config_file = "/etc/pitrac/pitrac.yaml"
        self.log_file = Path.home() / ".pitrac" / "logs" / "pitrac.log"
        self.pid_file = Path.home() / ".pitrac" / "run" / "pitrac.pid"

        # Ensure directories exist
        self.log_file.parent.mkdir(parents=True, exist_ok=True)
        self.pid_file.parent.mkdir(parents=True, exist_ok=True)

    def _load_pitrac_config(self) -> Dict[str, Any]:
        """Load PiTrac configuration from YAML file"""
        config = {}
        if Path(self.pitrac_config_file).exists():
            try:
                with open(self.pitrac_config_file, "r") as f:
                    config = yaml.safe_load(f) or {}
                logger.info(f"Loaded PiTrac config from {self.pitrac_config_file}")
            except Exception as e:
                logger.error(f"Failed to load PiTrac config: {e}")
        return config

    def _build_command(self) -> list:
        """Build the command to run pitrac_lm with proper arguments"""
        cmd = [self.pitrac_binary]

        config = self._load_pitrac_config()

        system_config = config.get("system", {})

        if system_config.get("mode") == "dual":
            camera_role = system_config.get("camera_role", "camera1")
            system_mode = camera_role
        else:
            system_mode = "camera1"
        cmd.append(f"--system_mode={system_mode}")

        logging_config = config.get("logging", {})
        log_level = logging_config.get("level", "info")
        if log_level == "trace":
            cmd.append("--trace")
        elif log_level == "debug":
            cmd.append("--debug")
        elif log_level == "info":
            cmd.append("--info")

        network_config = config.get("network", {})
        msg_broker = network_config.get("broker_address")
        if not msg_broker:
            msg_broker = "tcp://localhost:61616"
        cmd.append(f"--msg_broker_address={msg_broker}")

        storage_config = config.get("storage", {})
        base_image_dir = storage_config.get("image_dir")
        if not base_image_dir:
            base_image_dir = str(Path.home() / "LM_Shares" / "Images")
        cmd.append(f"--base_image_logging_dir={base_image_dir}")

        web_share_dir = storage_config.get("web_share_dir")
        if not web_share_dir:
            web_share_dir = str(Path.home() / "LM_Shares" / "WebShare")
        cmd.append(f"--web_server_share_dir={web_share_dir}")

        simulators_config = config.get("simulators", {})
        e6_host = simulators_config.get("e6_host")
        if e6_host:
            cmd.append(f"--e6_host_address={e6_host}")

        gspro_host = simulators_config.get("gspro_host")
        if gspro_host:
            cmd.append(f"--gspro_host_address={gspro_host}")

        if Path(self.config_file).exists():
            cmd.append(f"--config={self.config_file}")

        logger.info(f"Built command: {' '.join(cmd)}")
        return cmd

    async def start(self) -> Dict[str, Any]:
        """Start the PiTrac process"""
        if self.is_running():
            return {
                "status": "already_running",
                "message": "PiTrac is already running",
                "pid": self.get_pid(),
            }

        try:
            # Ensure directories exist
            Path(self.log_file).parent.mkdir(parents=True, exist_ok=True)
            Path(self.pid_file).parent.mkdir(parents=True, exist_ok=True)

            # Build command
            cmd = self._build_command()

            # Set environment variables
            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = "/usr/lib/pitrac"
            home_dir = str(Path.home())
            env["PITRAC_BASE_IMAGE_LOGGING_DIR"] = f"{home_dir}/LM_Shares/Images/"
            env["PITRAC_WEBSERVER_SHARE_DIR"] = f"{home_dir}/LM_Shares/WebShare/"
            env["PITRAC_MSG_BROKER_FULL_ADDRESS"] = "tcp://localhost:61616"

            # Add camera configuration from YAML if present
            config = self._load_pitrac_config()
            cameras_config = config.get("cameras", {})

            # Camera slot 1
            slot1 = cameras_config.get("slot1", {})
            if "type" in slot1:
                env["PITRAC_SLOT1_CAMERA_TYPE"] = str(slot1["type"])
            if "lens" in slot1:
                env["PITRAC_SLOT1_LENS_TYPE"] = str(slot1["lens"])

            # Camera slot 2
            slot2 = cameras_config.get("slot2", {})
            if "type" in slot2:
                env["PITRAC_SLOT2_CAMERA_TYPE"] = str(slot2["type"])
            if "lens" in slot2:
                env["PITRAC_SLOT2_LENS_TYPE"] = str(slot2["lens"])

            # Create directories for images and web share
            Path(f"{home_dir}/LM_Shares/Images").mkdir(parents=True, exist_ok=True)
            Path(f"{home_dir}/LM_Shares/WebShare").mkdir(parents=True, exist_ok=True)

            # Open log file for output
            with open(self.log_file, "a") as log:
                # Start the process
                self.process = subprocess.Popen(
                    cmd,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    env=env,
                    preexec_fn=os.setsid,  # Create new process group for clean shutdown
                )

                with open(self.pid_file, "w") as f:
                    f.write(str(self.process.pid))

                await asyncio.sleep(2)

                if self.process.poll() is None:
                    logger.info(
                        f"PiTrac started successfully with PID {self.process.pid}"
                    )
                    return {
                        "status": "started",
                        "message": "PiTrac started successfully",
                        "pid": self.process.pid,
                    }
                else:
                    logger.error("PiTrac process exited immediately")
                    return {
                        "status": "failed",
                        "message": "PiTrac failed to start - check logs",
                        "log_file": str(self.log_file),
                    }

        except Exception as e:
            logger.error(f"Failed to start PiTrac: {e}")
            return {"status": "error", "message": f"Failed to start PiTrac: {str(e)}"}

    async def stop(self) -> Dict[str, Any]:
        """Stop the PiTrac process gracefully"""
        if not self.is_running():
            return {"status": "not_running", "message": "PiTrac is not running"}

        try:
            pid = self.get_pid()

            if pid:
                os.kill(pid, signal.SIGTERM)
                logger.info(f"Sent SIGTERM to PiTrac process {pid}")

                max_wait = 5
                for i in range(max_wait * 10):
                    await asyncio.sleep(0.1)
                    if not self.is_running():
                        break

                if self.is_running():
                    logger.warning("PiTrac didn't stop gracefully, forcing...")
                    os.kill(pid, signal.SIGKILL)
                    await asyncio.sleep(0.5)

                if self.pid_file.exists():
                    self.pid_file.unlink()

                self.process = None
                logger.info("PiTrac stopped successfully")
                return {"status": "stopped", "message": "PiTrac stopped successfully"}
            else:
                return {
                    "status": "error",
                    "message": "Could not find PiTrac process ID",
                }

        except Exception as e:
            logger.error(f"Failed to stop PiTrac: {e}")
            return {"status": "error", "message": f"Failed to stop PiTrac: {str(e)}"}

    def is_running(self) -> bool:
        """Check if PiTrac is currently running"""
        pid = self.get_pid()
        if pid:
            try:
                # Check if process exists
                os.kill(pid, 0)
                return True
            except ProcessLookupError:
                # Process doesn't exist, clean up PID file
                if self.pid_file.exists():
                    self.pid_file.unlink()
                return False
        return False

    def get_pid(self) -> Optional[int]:
        """Get the PID of the running PiTrac process"""
        # First check our tracked process
        if self.process and self.process.poll() is None:
            return self.process.pid

        # Check PID file
        if self.pid_file.exists():
            try:
                with open(self.pid_file, "r") as f:
                    return int(f.read().strip())
            except (ValueError, IOError):
                pass

        # Try to find process by name
        try:
            result = subprocess.run(
                ["pgrep", "-x", "pitrac_lm"], capture_output=True, text=True, timeout=1
            )
            if result.returncode == 0 and result.stdout:
                return int(result.stdout.strip().split("\n")[0])
        except Exception:
            pass

        return None

    def get_status(self) -> Dict[str, Any]:
        """Get detailed status of PiTrac process"""
        is_running = self.is_running()
        pid = self.get_pid() if is_running else None

        status = {
            "running": is_running,
            "pid": pid,
            "log_file": str(self.log_file),
            "config_file": self.config_file,
            "binary": self.pitrac_binary,
        }

        # Add recent log lines if available
        if self.log_file.exists():
            try:
                with open(self.log_file, "r") as f:
                    lines = f.readlines()
                    status["recent_logs"] = lines[-20:] if len(lines) > 20 else lines
            except Exception as e:
                status["log_error"] = str(e)

        return status

    async def restart(self) -> Dict[str, Any]:
        """Restart the PiTrac process"""
        logger.info("Restarting PiTrac...")

        # Stop if running
        if self.is_running():
            stop_result = await self.stop()
            if stop_result["status"] == "error":
                return stop_result

            # Wait a moment for cleanup
            await asyncio.sleep(1)

        # Start again
        return await self.start()
