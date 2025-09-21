# PiTrac Modern UI - Setup & Usage Guide

## Quick Start - How to Run the Modern UI

### Option 1: Use the Modern Templates Directly (Recommended)

1. **Update the server to use modern templates**:
   Edit `server.py` and change line 65 from:
   ```python
   "dashboard.html"
   ```
   to:
   ```python
   "dashboard_modern.html"
   ```

2. **Start the PiTrac Web Server**:
   ```bash
   cd Software/web-server
   python server.py
   # OR
   python main.py
   # OR
   uvicorn server:app --host 0.0.0.0 --port 8080
   ```

3. **Access the Modern UI**:
   Open your browser and navigate to:
   ```
   http://localhost:8080
   ```

### Option 2: Replace Existing Templates (Permanent Switch)

If you want to make the modern UI the default:

```bash
cd Software/web-server/templates

# Backup original templates
mv base.html base_original.html
mv dashboard.html dashboard_original.html

# Use modern templates as default
cp base_modern.html base.html
cp dashboard_modern.html dashboard.html
```

Then start the server normally:
```bash
cd ..
python server.py
```

### Option 3: Run with Docker (if available)

```bash
cd Software/web-server
docker build -t pitrac-web .
docker run -p 8080:8080 pitrac-web
```

## Features of the Modern UI

### CDN-Based (No Build Required!)
The modern UI uses CDN-hosted libraries, so you don't need to:
- Install Node.js dependencies
- Run any build process
- Compile CSS

Everything works immediately out of the box!

### What You'll See

1. **Professional Dashboard**: 
   - Real-time golf ball metrics with beautiful cards
   - Progress bars and status indicators
   - Shot image gallery with modal viewer
   - Responsive layout for all devices

2. **Modern Navigation**:
   - Collapsible sidebar menu
   - Quick access to all PiTrac features
   - Status indicators for WebSocket, ActiveMQ, and Cameras

3. **Theme Support**:
   - Switch between Light, Dark, and Corporate themes
   - Theme preference is saved in browser

4. **Control Panel**:
   - Start/Stop/Restart PiTrac buttons in header
   - Easy access to calibration and settings

## System Requirements

- Python 3.7+ (for running the server)
- Modern web browser (Chrome, Firefox, Safari, Edge)
- Active internet connection (for CDN resources)

## Troubleshooting

### If the UI doesn't load properly:
1. Check your internet connection (CDN resources need to load)
2. Clear browser cache
3. Try a different browser

### If the server won't start:
1. Make sure you're in the correct directory: `Software/web-server`
2. Check Python is installed: `python --version`
3. Install requirements if needed: `pip install -r requirements.txt`

### To verify the modern templates exist:
```bash
ls templates/*modern*
# Should show:
# templates/base_modern.html
# templates/dashboard_modern.html
```

## Development Mode (Optional)

If you want to customize the UI further with local Tailwind CSS:

1. **Install dependencies** (requires Node.js):
   ```bash
   npm install
   ```

2. **Build Tailwind CSS** (optional for customization):
   ```bash
   npm run build-css
   ```

But remember: **This is NOT required for normal use!** The CDN version works immediately.

## File Structure

```
Software/web-server/
├── templates/
│   ├── base_modern.html      # Modern base template
│   ├── dashboard_modern.html # Modern dashboard
│   ├── base.html             # Original base template
│   └── dashboard.html        # Original dashboard
├── server.py                 # Main server file
├── package.json             # Node.js dependencies (optional)
└── tailwind.config.js       # Tailwind configuration (optional)
```

## Key Technologies Used

- **FlyonUI**: Modern UI components
- **Tailwind CSS**: Utility-first CSS framework
- **Tabler Icons**: Beautiful icon set
- **Responsive Design**: Works on desktop, tablet, and mobile

## Support

For issues or questions about the modern UI:
1. Check this README first
2. Visit the PiTrac GitHub: https://github.com/jamespilgrim/PiTrac
3. Join the Discord: https://discord.gg/gMQcBBQYHT

Enjoy your modernized PiTrac Launch Monitor interface! ⛳
