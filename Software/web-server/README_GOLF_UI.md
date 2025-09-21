# PiTrac Professional Golf Launch Monitor UI

## Overview
A modern, professional-grade golf launch monitor interface built with Tailwind CSS, designed to rival commercial systems like Trackman and provide real-time shot data visualization.

## Features

### Core Metrics Display
- **Ball Speed** - Real-time mph measurement with tour average comparison
- **Launch Angle** - Degree measurement with optimal range indicators
- **Spin Rate** - RPM tracking with optimal zone visualization
- **Carry Distance** - Yard measurement with total distance projection

### Secondary Metrics
- Club Head Speed
- Smash Factor
- Attack Angle
- Club Path

### Visualization
- **Shot Trajectory Canvas** - SVG-based ball flight visualization
- **Real-time Updates** - Live data streaming via WebSocket
- **Shot History Timeline** - Visual timeline of recent shots
- **Session Statistics** - Averages, best shot, consistency metrics

### User Interface
- **Dark Mode Support** - Full dark/light theme toggle
- **Responsive Design** - Mobile, tablet, and desktop optimized
- **Professional Aesthetics** - Golf-specific color palette and gradients
- **High Contrast Mode** - For outdoor visibility

## Setup Instructions

### Prerequisites
- Node.js 21.6.1+ installed
- Python 3.8+ with FastAPI
- PiTrac hardware configured

### Installation

1. **Install Node Dependencies**
```bash
cd Software/web-server
npm install
```

2. **Build Tailwind CSS**
```bash
# Development build with watch mode
npm run build-css

# Production build (minified)
npm run build-css-prod
```

3. **Start the Web Server**
```bash
python main.py
# or
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

4. **Access the Dashboard**
- Golf Dashboard: `http://localhost:8000/golf`
- Classic Dashboard: `http://localhost:8000/`

## Tailwind Configuration

### Custom Theme Extensions

#### Golf-Specific Colors
```javascript
// tailwind.config.js
colors: {
  'golf-green': {
    50-900: // Full spectrum of golf course green
  },
  'fairway': {
    light, DEFAULT, dark // Fairway grass tones
  },
  'rough': {
    light, DEFAULT, dark // Rough area colors
  }
}
```

#### Custom Animations
- `fade-in` - Smooth element appearance
- `slide-up` - Upward sliding animation
- `pulse-slow` - Gentle pulsing for live indicators
- `gradient` - Animated gradient backgrounds
- `ballFlight` - Ball trajectory animation
- `traceShot` - Shot path tracing effect

### Component Classes

#### Metric Cards
```html
<div class="metric-card">
  <p class="metric-label">Ball Speed</p>
  <span class="metric-value">168.4</span>
  <span class="metric-unit">mph</span>
</div>
```

#### Shot Trajectory
```html
<div class="trajectory-canvas">
  <!-- SVG ball flight visualization -->
</div>
```

#### Club Selection
```html
<div class="club-selector">
  <button class="club-button club-button-active">Driver</button>
  <button class="club-button club-button-inactive">3 Wood</button>
</div>
```

#### Real-time Metrics
```html
<div class="realtime-metric">
  <p>Club Speed</p>
  <div class="realtime-metric-bar"></div>
</div>
```

## API Integration

### WebSocket Connection
```javascript
// Connect to real-time data stream
const ws = new WebSocket('ws://localhost:8000/ws');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  updateMetrics(data);
};
```

### REST Endpoints
- `GET /api/shot` - Current shot data
- `GET /api/history` - Shot history
- `POST /api/reset` - Reset shot data
- `GET /health` - System health check

## Development

### File Structure
```
Software/web-server/
├── src/
│   └── input.css          # Tailwind input with custom styles
├── static/
│   └── css/
│       └── tailwind.css   # Generated output CSS
├── templates/
│   └── golf_dashboard.html # Golf UI template
├── tailwind.config.js     # Tailwind configuration
├── postcss.config.js      # PostCSS configuration
└── package.json           # Node dependencies
```

### Custom CSS Components

The `src/input.css` file includes custom component classes:

```css
@layer components {
  .metric-card { /* Card styling */ }
  .launch-data-grid { /* Grid layout */ }
  .shot-timeline { /* Timeline styling */ }
  .gradient-fairway { /* Golf gradients */ }
}
```

### Build Process

1. **Tailwind CSS Processing**
   - Input: `src/input.css`
   - Config: `tailwind.config.js`
   - PostCSS: `postcss.config.js`
   - Output: `static/css/tailwind.css`

2. **Watch Mode for Development**
```bash
npm run build-css
# Watches for changes and rebuilds automatically
```

3. **Production Build**
```bash
npm run build-css-prod
# Minifies and optimizes for production
```

## Customization

### Adding New Metrics
1. Add metric card in HTML template
2. Update WebSocket handler for new data
3. Style with Tailwind utility classes

### Modifying Color Scheme
Edit `tailwind.config.js`:
```javascript
extend: {
  colors: {
    'your-color': {
      // Add custom color palette
    }
  }
}
```

### Creating New Components
Add to `src/input.css`:
```css
@layer components {
  .your-component {
    @apply /* Tailwind utilities */;
  }
}
```

## Performance Optimization

### Production Build
- CSS minification enabled
- Unused styles purged
- Optimized for file size

### Caching Strategy
- Static assets served with cache headers
- CSS versioning for cache busting

### Real-time Updates
- WebSocket for live data
- Debounced metric updates
- Efficient DOM manipulation

## Browser Support
- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+
- Mobile browsers (responsive design)

## Troubleshooting

### CSS Not Building
```bash
# Clean and rebuild
rm -rf node_modules
npm install
npm run build-css
```

### Styles Not Applying
- Check Tailwind content paths in config
- Verify HTML classes match Tailwind utilities
- Ensure CSS file is linked correctly

### Dark Mode Issues
- Check localStorage for theme preference
- Verify dark: variants in Tailwind classes

## Future Enhancements
- 3D ball flight visualization
- Video replay integration
- Multi-session comparison
- Export to CSV/PDF reports
- Mobile app companion
- Cloud data sync
- AI-powered swing analysis

## License
MIT License - See LICENSE file for details

## Support
For issues or questions about the UI, please open an issue on GitHub.
