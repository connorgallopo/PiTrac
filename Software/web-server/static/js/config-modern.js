// Modern Configuration Manager - Bridges new UI with existing backend
/* global ws */

// Configuration state
let currentConfig = {};
let defaultConfig = {};
let userSettings = {};
// ws is already declared in base.html, don't redeclare
const modifiedSettings = new Set();

// Tab state
let currentTab = 'general';

// Tab switching function - make it immediately available for inline onclick
function switchTab(tabName) {
    currentTab = tabName;

    // Hide all content
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.add('hidden');
    });

    // Reset all tabs
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('border-purple-500', 'text-white');
        btn.classList.add('border-transparent', 'text-gray-400');
    });

    // Show selected content
    document.getElementById(`content-${tabName}`).classList.remove('hidden');

    // Highlight selected tab
    const selectedTab = document.getElementById(`tab-${tabName}`);
    selectedTab.classList.remove('border-transparent', 'text-gray-400');
    selectedTab.classList.add('border-purple-500', 'text-white');
}

// Export immediately for inline onclick handlers
window.switchTab = switchTab;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initConfigWebSocket();
    loadConfiguration();
    initializeUIEventHandlers();
});

// Initialize config-specific WebSocket handlers
function initConfigWebSocket() {
    // Use the existing WebSocket from base.html
    if (!ws) {
        // If WebSocket hasn't been initialized yet, wait a bit
        setTimeout(initConfigWebSocket, 100);
        return;
    }

    // Store the original onmessage handler
    const originalOnMessage = ws.onmessage;

    // Override with config-specific handler
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);

        if (data.type === 'config_update') {
            showToast(`Configuration updated: ${data.key}`, 'success');
            if (data.requires_restart) {
                showToast('Restart required for changes to take effect', 'warning');
            }
        } else if (data.type === 'config_reset') {
            showToast('Configuration reset to defaults', 'success');
            loadConfiguration();
        }

        // Call the original handler if it exists
        if (originalOnMessage && typeof originalOnMessage === 'function') {
            originalOnMessage(event);
        }
    };
}

// Load configuration from server
async function loadConfiguration() {
    try {
        modifiedSettings.clear();

        // Load all configuration data
        const [configRes, defaultsRes, userRes] = await Promise.all([
            fetch('/api/config'),
            fetch('/api/config/defaults'),
            fetch('/api/config/user')
        ]);

        const configData = await configRes.json();
        const defaultsData = await defaultsRes.json();
        const userData = await userRes.json();

        currentConfig = configData.data || {};
        defaultConfig = defaultsData.data || {};
        userSettings = userData.data || {};

        // Map configuration to UI elements
        mapConfigToUI();

        showToast('Configuration loaded', 'success');
    } catch (error) {
        console.error('Failed to load configuration:', error);
        showToast('Failed to load configuration', 'error');
    }
}

// Map configuration values to the new UI
function mapConfigToUI() {
    // General Settings
    if (document.getElementById('distance-units')) {
        const distanceUnit = getNestedValue(currentConfig, 'units.distance') || 'yards';
        document.getElementById('distance-units').value = distanceUnit;
    }

    if (document.getElementById('speed-units')) {
        const speedUnit = getNestedValue(currentConfig, 'units.speed') || 'mph';
        document.getElementById('speed-units').value = speedUnit;
    }

    // Display preferences
    if (document.getElementById('show-grid')) {
        const showGrid = getNestedValue(currentConfig, 'display.show_grid');
        document.getElementById('show-grid').checked = showGrid !== false;
    }

    if (document.getElementById('auto-record')) {
        const autoRecord = getNestedValue(currentConfig, 'recording.auto_record');
        document.getElementById('auto-record').checked = autoRecord !== false;
    }

    if (document.getElementById('sound-alerts')) {
        const soundAlerts = getNestedValue(currentConfig, 'alerts.sound_enabled');
        document.getElementById('sound-alerts').checked = soundAlerts === true;
    }

    // Session settings
    if (document.getElementById('save-interval')) {
        const saveInterval = getNestedValue(currentConfig, 'session.save_interval') || 5;
        document.getElementById('save-interval').value = saveInterval;
        document.getElementById('save-interval-value').textContent = saveInterval + ' min';
    }

    if (document.getElementById('session-timeout')) {
        const sessionTimeout = getNestedValue(currentConfig, 'session.timeout') || 60;
        document.getElementById('session-timeout').value = sessionTimeout;
        document.getElementById('session-timeout-value').textContent = sessionTimeout + ' min';
    }

    // Camera settings
    mapCameraSettings();

    // Analysis settings
    mapAnalysisSettings();

    // Network settings
    mapNetworkSettings();

    // Advanced settings
    mapAdvancedSettings();
}

// Map camera settings
function mapCameraSettings() {
    // Camera 1 settings
    const cam1Resolution = getNestedValue(currentConfig, 'cameras.slot1.resolution') || '1920x1080';
    const cam1FrameRate = getNestedValue(currentConfig, 'cameras.slot1.framerate') || 60;
    const cam1Exposure = getNestedValue(currentConfig, 'cameras.slot1.exposure') || 0;
    const cam1Gain = getNestedValue(currentConfig, 'cameras.slot1.gain') || 50;

    // Camera 2 settings
    const cam2Resolution = getNestedValue(currentConfig, 'cameras.slot2.resolution') || '1920x1080';
    const cam2FrameRate = getNestedValue(currentConfig, 'cameras.slot2.framerate') || 60;
    const cam2Exposure = getNestedValue(currentConfig, 'cameras.slot2.exposure') || 0;
    const cam2Gain = getNestedValue(currentConfig, 'cameras.slot2.gain') || 50;

    // Apply to UI if elements exist
    const cam1ResolutionEl = document.querySelector('#content-camera select:nth-of-type(1)');
    if (cam1ResolutionEl) cam1ResolutionEl.value = cam1Resolution;

    const cam1FrameRateEl = document.querySelector('#content-camera .glass:nth-child(1) select:nth-of-type(2)');
    if (cam1FrameRateEl) cam1FrameRateEl.value = String(cam1FrameRate);

    if (document.getElementById('cam1-exposure')) {
        document.getElementById('cam1-exposure').value = cam1Exposure;
        document.getElementById('cam1-exposure-value').textContent = cam1Exposure;
    }

    if (document.getElementById('cam1-gain')) {
        document.getElementById('cam1-gain').value = cam1Gain;
        document.getElementById('cam1-gain-value').textContent = cam1Gain;
    }

    // Similar for Camera 2
    const cam2ResolutionEl = document.querySelector('#content-camera .glass:nth-child(2) select:nth-of-type(1)');
    if (cam2ResolutionEl) cam2ResolutionEl.value = cam2Resolution;

    const cam2FrameRateEl = document.querySelector('#content-camera .glass:nth-child(2) select:nth-of-type(2)');
    if (cam2FrameRateEl) cam2FrameRateEl.value = String(cam2FrameRate);

    if (document.getElementById('cam2-exposure')) {
        document.getElementById('cam2-exposure').value = cam2Exposure;
        document.getElementById('cam2-exposure-value').textContent = cam2Exposure;
    }

    if (document.getElementById('cam2-gain')) {
        document.getElementById('cam2-gain').value = cam2Gain;
        document.getElementById('cam2-gain-value').textContent = cam2Gain;
    }
}

// Map analysis settings
function mapAnalysisSettings() {
    const detectionSensitivity = getNestedValue(currentConfig, 'analysis.detection_sensitivity') || 7;
    const minBallSize = getNestedValue(currentConfig, 'analysis.min_ball_size') || 10;
    const maxBallSize = getNestedValue(currentConfig, 'analysis.max_ball_size') || 100;
    const smoothingFactor = getNestedValue(currentConfig, 'analysis.smoothing_factor') || 5;
    const predictionModel = getNestedValue(currentConfig, 'analysis.prediction_model') || 'physics';
    const windCompensation = getNestedValue(currentConfig, 'analysis.wind_compensation') !== false;
    const altitudeCorrection = getNestedValue(currentConfig, 'analysis.altitude_correction') !== false;

    if (document.getElementById('detection-sensitivity')) {
        document.getElementById('detection-sensitivity').value = detectionSensitivity;
    }

    const minBallSizeInput = document.querySelector('#content-analysis input[type="number"]:nth-of-type(1)');
    if (minBallSizeInput) minBallSizeInput.value = minBallSize;

    const maxBallSizeInput = document.querySelector('#content-analysis input[type="number"]:nth-of-type(2)');
    if (maxBallSizeInput) maxBallSizeInput.value = maxBallSize;

    if (document.getElementById('smoothing')) {
        document.getElementById('smoothing').value = smoothingFactor;
        document.getElementById('smoothing-value').textContent = smoothingFactor;
    }

    const predictionModelSelect = document.querySelector('#content-analysis select');
    if (predictionModelSelect) predictionModelSelect.value = predictionModel;

    const windCompCheckbox = document.querySelector('#content-analysis input[type="checkbox"]:nth-of-type(1)');
    if (windCompCheckbox) windCompCheckbox.checked = windCompensation;

    const altitudeCheckbox = document.querySelector('#content-analysis input[type="checkbox"]:nth-of-type(2)');
    if (altitudeCheckbox) altitudeCheckbox.checked = altitudeCorrection;
}

// Map network settings
function mapNetworkSettings() {
    const wsPort = getNestedValue(currentConfig, 'network.websocket_port') || 8081;
    const maxConnections = getNestedValue(currentConfig, 'network.max_connections') || 10;
    const wsEnabled = getNestedValue(currentConfig, 'network.websocket_enabled') !== false;
    const apiEnabled = getNestedValue(currentConfig, 'network.api_enabled') === true;
    const apiKey = getNestedValue(currentConfig, 'network.api_key') || '';

    const wsPortInput = document.querySelector('#content-network input[type="number"]:nth-of-type(1)');
    if (wsPortInput) wsPortInput.value = wsPort;

    const maxConnInput = document.querySelector('#content-network input[type="number"]:nth-of-type(2)');
    if (maxConnInput) maxConnInput.value = maxConnections;

    const wsEnabledCheckbox = document.querySelector('#content-network input[type="checkbox"]:nth-of-type(1)');
    if (wsEnabledCheckbox) wsEnabledCheckbox.checked = wsEnabled;

    const apiEnabledCheckbox = document.querySelector('#content-network input[type="checkbox"]:nth-of-type(2)');
    if (apiEnabledCheckbox) apiEnabledCheckbox.checked = apiEnabled;

    if (document.getElementById('api-key') && apiKey) {
        document.getElementById('api-key').value = apiKey;
    }
}

// Map advanced settings
function mapAdvancedSettings() {
    const logLevel = getNestedValue(currentConfig, 'system.log_level') || 'info';
    const maxLogSize = getNestedValue(currentConfig, 'system.max_log_size') || 100;
    const debugMode = getNestedValue(currentConfig, 'system.debug_mode') === true;
    const telemetry = getNestedValue(currentConfig, 'system.telemetry_enabled') === true;

    const logLevelSelect = document.querySelector('#content-advanced select');
    if (logLevelSelect) logLevelSelect.value = logLevel;

    const maxLogSizeInput = document.querySelector('#content-advanced input[type="number"]');
    if (maxLogSizeInput) maxLogSizeInput.value = maxLogSize;

    const debugCheckbox = document.querySelector('#content-advanced input[type="checkbox"]:nth-of-type(1)');
    if (debugCheckbox) debugCheckbox.checked = debugMode;

    const telemetryCheckbox = document.querySelector('#content-advanced input[type="checkbox"]:nth-of-type(2)');
    if (telemetryCheckbox) telemetryCheckbox.checked = telemetry;
}

// Initialize UI event handlers
function initializeUIEventHandlers() {
    // Tab switching is already handled by inline onclick

    // General settings handlers
    document.getElementById('distance-units')?.addEventListener('change', (e) => {
        handleSettingChange('units.distance', e.target.value);
    });

    document.getElementById('speed-units')?.addEventListener('change', (e) => {
        handleSettingChange('units.speed', e.target.value);
    });

    document.getElementById('show-grid')?.addEventListener('change', (e) => {
        handleSettingChange('display.show_grid', e.target.checked);
    });

    document.getElementById('auto-record')?.addEventListener('change', (e) => {
        handleSettingChange('recording.auto_record', e.target.checked);
    });

    document.getElementById('sound-alerts')?.addEventListener('change', (e) => {
        handleSettingChange('alerts.sound_enabled', e.target.checked);
    });

    document.getElementById('save-interval')?.addEventListener('input', (e) => {
        document.getElementById('save-interval-value').textContent = e.target.value + ' min';
        handleSettingChange('session.save_interval', parseInt(e.target.value));
    });

    document.getElementById('session-timeout')?.addEventListener('input', (e) => {
        document.getElementById('session-timeout-value').textContent = e.target.value + ' min';
        handleSettingChange('session.timeout', parseInt(e.target.value));
    });

    // Camera settings handlers
    initializeCameraHandlers();

    // Analysis settings handlers
    initializeAnalysisHandlers();

    // Network settings handlers
    initializeNetworkHandlers();

    // Advanced settings handlers
    initializeAdvancedHandlers();
}

// Initialize camera event handlers
function initializeCameraHandlers() {
    // Camera 1 handlers
    document.getElementById('cam1-exposure')?.addEventListener('input', (e) => {
        document.getElementById('cam1-exposure-value').textContent = e.target.value;
        handleSettingChange('cameras.slot1.exposure', parseInt(e.target.value));
    });

    document.getElementById('cam1-gain')?.addEventListener('input', (e) => {
        document.getElementById('cam1-gain-value').textContent = e.target.value;
        handleSettingChange('cameras.slot1.gain', parseInt(e.target.value));
    });

    // Camera 2 handlers
    document.getElementById('cam2-exposure')?.addEventListener('input', (e) => {
        document.getElementById('cam2-exposure-value').textContent = e.target.value;
        handleSettingChange('cameras.slot2.exposure', parseInt(e.target.value));
    });

    document.getElementById('cam2-gain')?.addEventListener('input', (e) => {
        document.getElementById('cam2-gain-value').textContent = e.target.value;
        handleSettingChange('cameras.slot2.gain', parseInt(e.target.value));
    });

    // Test buttons
    document.querySelectorAll('#content-camera button').forEach((btn, index) => {
        if (btn.textContent.includes('Test Camera')) {
            btn.addEventListener('click', () => testCamera(index + 1));
        }
    });
}

// Initialize analysis event handlers
function initializeAnalysisHandlers() {
    document.getElementById('detection-sensitivity')?.addEventListener('input', (e) => {
        handleSettingChange('analysis.detection_sensitivity', parseInt(e.target.value));
    });

    document.getElementById('smoothing')?.addEventListener('input', (e) => {
        document.getElementById('smoothing-value').textContent = e.target.value;
        handleSettingChange('analysis.smoothing_factor', parseInt(e.target.value));
    });

    // Min/Max ball size inputs
    const ballSizeInputs = document.querySelectorAll('#content-analysis input[type="number"]');
    ballSizeInputs[0]?.addEventListener('change', (e) => {
        handleSettingChange('analysis.min_ball_size', parseInt(e.target.value));
    });
    ballSizeInputs[1]?.addEventListener('change', (e) => {
        handleSettingChange('analysis.max_ball_size', parseInt(e.target.value));
    });

    // Prediction model
    document.querySelector('#content-analysis select')?.addEventListener('change', (e) => {
        handleSettingChange('analysis.prediction_model', e.target.value);
    });

    // Checkboxes
    const analysisCheckboxes = document.querySelectorAll('#content-analysis input[type="checkbox"]');
    analysisCheckboxes[0]?.addEventListener('change', (e) => {
        handleSettingChange('analysis.wind_compensation', e.target.checked);
    });
    analysisCheckboxes[1]?.addEventListener('change', (e) => {
        handleSettingChange('analysis.altitude_correction', e.target.checked);
    });
}

// Initialize network event handlers
function initializeNetworkHandlers() {
    const networkInputs = document.querySelectorAll('#content-network input[type="number"]');
    networkInputs[0]?.addEventListener('change', (e) => {
        handleSettingChange('network.websocket_port', parseInt(e.target.value));
    });
    networkInputs[1]?.addEventListener('change', (e) => {
        handleSettingChange('network.max_connections', parseInt(e.target.value));
    });

    const networkCheckboxes = document.querySelectorAll('#content-network input[type="checkbox"]');
    networkCheckboxes[0]?.addEventListener('change', (e) => {
        handleSettingChange('network.websocket_enabled', e.target.checked);
    });
    networkCheckboxes[1]?.addEventListener('change', (e) => {
        handleSettingChange('network.api_enabled', e.target.checked);
    });
}

// Initialize advanced event handlers
function initializeAdvancedHandlers() {
    document.querySelector('#content-advanced select')?.addEventListener('change', (e) => {
        handleSettingChange('system.log_level', e.target.value);
    });

    document.querySelector('#content-advanced input[type="number"]')?.addEventListener('change', (e) => {
        handleSettingChange('system.max_log_size', parseInt(e.target.value));
    });

    const advancedCheckboxes = document.querySelectorAll('#content-advanced input[type="checkbox"]');
    advancedCheckboxes[0]?.addEventListener('change', (e) => {
        handleSettingChange('system.debug_mode', e.target.checked);
    });
    advancedCheckboxes[1]?.addEventListener('change', (e) => {
        handleSettingChange('system.telemetry_enabled', e.target.checked);
    });
}

// Handle setting change
function handleSettingChange(key, value) {
    const originalValue = getNestedValue(currentConfig, key);

    // Update current config
    setNestedValue(currentConfig, key, value);

    // Track modification
    if (value !== originalValue) {
        modifiedSettings.add(key);
    } else {
        modifiedSettings.delete(key);
    }

    // Update UI state
    updateSaveButtonState();
}

// Save settings
async function saveSettings() {
    if (modifiedSettings.size === 0) {
        showToast('No changes to save', 'warning');
        return;
    }

    showToast('Saving changes...', 'info');

    const errors = [];
    const requiresRestart = [];

    for (const key of modifiedSettings) {
        const value = getNestedValue(currentConfig, key);

        try {
            const response = await fetch(`/api/config/${key}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ value })
            });

            const result = await response.json();

            if (result.error) {
                errors.push(`${key}: ${result.error}`);
            } else if (result.requires_restart) {
                requiresRestart.push(key);
            }
        } catch (error) {
            errors.push(`${key}: ${error.message}`);
        }
    }

    if (errors.length > 0) {
        showToast(`Errors: ${errors.join(', ')}`, 'error');
    } else {
        modifiedSettings.clear();
        updateSaveButtonState();

        if (requiresRestart.length > 0) {
            showToast('Settings saved. Restart required for some changes.', 'warning');
        } else {
            showToast('Settings saved successfully', 'success');
        }
    }
}

// Discard changes
function discardChanges() {
    if (modifiedSettings.size === 0) {
        showToast('No changes to discard', 'info');
        return;
    }

    if (confirm('Are you sure you want to discard all changes?')) {
        loadConfiguration();
        modifiedSettings.clear();
        updateSaveButtonState();
    }
}

// Reset settings
function resetSettings() {
    if (confirm('Are you sure you want to reset all settings to defaults?')) {
        resetToDefaults();
    }
}

// Clear data
function clearData() {
    if (confirm('Are you sure you want to clear all shot data? This cannot be undone.')) {
        clearShotData();
    }
}

// Factory reset
function factoryReset() {
    if (confirm('WARNING: This will reset everything to factory defaults and clear all data. Are you sure?')) {
        if (confirm('This action cannot be undone. Please confirm again.')) {
            performFactoryReset();
        }
    }
}

// Reset to defaults
async function resetToDefaults() {
    try {
        const response = await fetch('/api/config/reset', {
            method: 'POST'
        });

        const result = await response.json();

        if (result.success) {
            showToast('All settings reset to defaults', 'success');
            loadConfiguration();
        } else {
            showToast(`Failed to reset: ${result.message}`, 'error');
        }
    } catch (error) {
        console.error('Failed to reset:', error);
        showToast('Failed to reset configuration', 'error');
    }
}

// Clear shot data
async function clearShotData() {
    try {
        const response = await fetch('/api/shots/clear', {
            method: 'DELETE'
        });

        const result = await response.json();

        if (result.success) {
            showToast('Shot data cleared', 'success');
        } else {
            showToast(`Failed to clear data: ${result.message}`, 'error');
        }
    } catch (error) {
        console.error('Failed to clear data:', error);
        showToast('Failed to clear shot data', 'error');
    }
}

// Perform factory reset
async function performFactoryReset() {
    try {
        const response = await fetch('/api/factory-reset', {
            method: 'POST'
        });

        const result = await response.json();

        if (result.success) {
            showToast('Factory reset initiated. Please wait...', 'success');
            setTimeout(() => {
                window.location.reload();
            }, 3000);
        } else {
            showToast(`Factory reset failed: ${result.message}`, 'error');
        }
    } catch (error) {
        console.error('Factory reset failed:', error);
        showToast('Factory reset failed', 'error');
    }
}

// Test camera
async function testCamera(cameraNumber) {
    try {
        showToast(`Testing Camera ${cameraNumber}...`, 'info');

        const response = await fetch(`/api/camera/${cameraNumber}/test`, {
            method: 'POST'
        });

        const result = await response.json();

        if (result.success) {
            showToast(`Camera ${cameraNumber} test successful`, 'success');
        } else {
            showToast(`Camera ${cameraNumber} test failed: ${result.message}`, 'error');
        }
    } catch (error) {
        console.error(`Camera ${cameraNumber} test failed:`, error);
        showToast(`Camera ${cameraNumber} test failed`, 'error');
    }
}

// Generate API key
function generateApiKey() {
    const newKey = 'pk_' + Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);
    document.getElementById('api-key').value = newKey;
    document.getElementById('api-key').type = 'text';

    // Hide after 3 seconds
    setTimeout(() => {
        document.getElementById('api-key').type = 'password';
    }, 3000);

    handleSettingChange('network.api_key', newKey);
}

// Update save button state
function updateSaveButtonState() {
    const saveBtn = document.querySelector('button[onclick="saveSettings()"]');
    if (saveBtn) {
        if (modifiedSettings.size > 0) {
            saveBtn.classList.remove('opacity-50', 'cursor-not-allowed');
            saveBtn.classList.add('hover:scale-105');
            saveBtn.disabled = false;
        } else {
            saveBtn.classList.add('opacity-50', 'cursor-not-allowed');
            saveBtn.classList.remove('hover:scale-105');
            saveBtn.disabled = true;
        }
    }
}

// Show toast notification (reusing existing toast element)
function showToast(message, type = 'success') {
    const toast = document.getElementById('status-toast');
    if (!toast) return;

    // Update toast content
    const iconSvg = type === 'success'
        ? '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7"></path>'
        : type === 'error'
        ? '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>'
        : '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path>';

    const bgColor = type === 'success' ? 'bg-green-500' : type === 'error' ? 'bg-red-500' : 'bg-yellow-500';
    const borderColor = type === 'success' ? 'border-green-500/20' : type === 'error' ? 'border-red-500/20' : 'border-yellow-500/20';
    const bgOpacity = type === 'success' ? 'bg-green-500/10' : type === 'error' ? 'bg-red-500/10' : 'bg-yellow-500/10';

    toast.className = `fixed bottom-8 right-8 glass rounded-xl p-4 border ${borderColor} ${bgOpacity}`;
    toast.innerHTML = `
        <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-full ${bgColor} flex items-center justify-center">
                <svg class="w-6 h-6 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    ${iconSvg}
                </svg>
            </div>
            <div>
                <p class="font-medium text-white">${message}</p>
            </div>
        </div>
    `;

    toast.classList.remove('hidden');

    // Auto-hide after 3 seconds
    setTimeout(() => {
        toast.classList.add('hidden');
    }, 3000);
}

// Utility functions
function getNestedValue(obj, path) {
    return path.split('.').reduce((current, key) => current?.[key], obj);
}

function setNestedValue(obj, path, value) {
    const parts = path.split('.');
    let current = obj;
    for (let i = 0; i < parts.length - 1; i++) {
        const part = parts[i];
        if (!(part in current) || typeof current[part] !== 'object') {
            current[part] = {};
        }
        current = current[part];
    }
    current[parts[parts.length - 1]] = value;
}

// Tab switching is already defined and exported at the top of the file

// Export functions for inline onclick handlers
window.saveSettings = saveSettings;
window.discardChanges = discardChanges;
window.resetSettings = resetSettings;
window.clearData = clearData;
window.factoryReset = factoryReset;
window.generateApiKey = generateApiKey;