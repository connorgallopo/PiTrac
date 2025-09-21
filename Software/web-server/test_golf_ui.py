#!/usr/bin/env python3
"""Test script to verify golf UI setup is working correctly."""

import os
import sys
from pathlib import Path

# Colors for output
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"


def check_file_exists(filepath, description):
    """Check if a file exists."""
    if Path(filepath).exists():
        print(f"{GREEN}✓{RESET} {description}: Found")
        return True
    else:
        print(f"{RED}✗{RESET} {description}: Missing")
        return False


def main():
    print("\nPiTrac Golf UI Setup Verification")
    print("=" * 50)

    all_good = True

    # Check CSS file
    print("\n1. Checking CSS Build:")
    if check_file_exists("static/css/tailwind.css", "Tailwind CSS"):
        css_size = Path("static/css/tailwind.css").stat().st_size
        print(f"  {YELLOW}→{RESET} CSS file size: {css_size} bytes")
    else:
        all_good = False

    # Check template
    print("\n2. Checking HTML Template:")
    if not check_file_exists("templates/golf_dashboard.html", "Golf Dashboard HTML"):
        all_good = False

    # Check icons
    print("\n3. Checking Heroicons:")
    icons_dir = Path("static/icons")
    if icons_dir.exists():
        icon_count = len(list(icons_dir.glob("*.svg")))
        print(f"{GREEN}✓{RESET} Icons directory: Found ({icon_count} icons)")

        # Check specific icons used in template
        required_icons = [
            "bolt.svg",
            "moon.svg",
            "arrow-trending-up.svg",
            "arrow-up.svg",
            "arrow-path.svg",
            "map-pin.svg",
        ]
        for icon in required_icons:
            if not check_file_exists(f"static/icons/{icon}", f"  Icon {icon}"):
                all_good = False
    else:
        print(f"{RED}✗{RESET} Icons directory: Missing")
        all_good = False

    # Check server route
    print("\n4. Checking Server Configuration:")
    if check_file_exists("server.py", "Server file"):
        with open("server.py", "r") as f:
            content = f.read()
            if '@self.app.get("/golf"' in content:
                print(f"{GREEN}✓{RESET} Golf route: Configured")
            else:
                print(f"{RED}✗{RESET} Golf route: Not found")
                all_good = False
    else:
        all_good = False

    # Check dependencies
    print("\n5. Checking Node Dependencies:")
    if check_file_exists("package.json", "Package.json"):
        if check_file_exists("node_modules/tailwindcss/package.json", "  Tailwind installed"):
            pass
        else:
            print(f"  {YELLOW}→{RESET} Run: npm install")
            all_good = False
    else:
        all_good = False

    # Summary
    print("\n" + "=" * 50)
    if all_good:
        print(f"{GREEN}✓ All checks passed!{RESET}")
        print("\nTo run the server:")
        print("  python main.py")
        print("\nThen visit:")
        print("  http://localhost:8000/golf")
    else:
        print(f"{RED}✗ Some checks failed.{RESET}")
        print("\nTroubleshooting steps:")
        print("1. Run: npm install")
        print("2. Run: npm run build-css")
        print("3. Restart the server")
        print("4. Clear browser cache (Ctrl+Shift+R)")

    return 0 if all_good else 1


if __name__ == "__main__":
    # Change to web-server directory if needed
    if not Path("server.py").exists():
        os.chdir(Path(__file__).parent)

    sys.exit(main())
