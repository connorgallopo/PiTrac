// Modern Dashboard JavaScript - Real-time data updates with WebSocket
// ws is already declared in base.html, don't redeclare
let chartData = {
    trajectory: [],
    speed: [],
    spin: []
};

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initializeDashboard();
    initializeCharts();
});

// Initialize dashboard
function initializeDashboard() {
    // WebSocket is handled by base.html, but we define the handler
    window.handleWebSocketMessage = function(data) {
        updateDashboardMetrics(data);
    };

    // Load initial data
    loadLatestShot();
    loadSessionStats();
}

// Update dashboard metrics
function updateDashboardMetrics(data) {
    // Update ball speed
    if (data.ball_speed !== undefined) {
        updateMetricCard('ball-speed', data.ball_speed, 'mph', data.ball_speed_change);
    }

    // Update launch angle
    if (data.launch_angle !== undefined) {
        updateMetricCard('launch-angle', data.launch_angle, '°', null, checkOptimalRange(data.launch_angle, 12, 15));
    }

    // Update spin rate
    if (data.spin_rate !== undefined) {
        updateMetricCard('spin-rate', data.spin_rate, 'rpm', data.spin_change);
    }

    // Update carry distance
    if (data.carry_distance !== undefined) {
        updateMetricCard('carry-distance', data.carry_distance, 'yds', data.carry_change);
    }

    // Update detailed metrics
    if (data.club_speed) updateDetailMetric('club-speed', data.club_speed, 'mph');
    if (data.smash_factor) updateDetailMetric('smash-factor', data.smash_factor);
    if (data.apex_height) updateDetailMetric('apex-height', data.apex_height, 'yds');
    if (data.flight_time) updateDetailMetric('flight-time', data.flight_time, 's');

    // Update trajectory visualization
    if (data.trajectory) {
        updateTrajectory(data.trajectory);
    }

    // Update recent shots list
    if (data.shot_id) {
        addRecentShot(data);
    }

    // Update status indicators
    updateSystemStatus(data);
}

// Update metric card
function updateMetricCard(id, value, unit, change, status) {
    const card = document.querySelector(`[data-metric="${id}"]`);
    if (!card) return;

    // Update value
    const valueEl = card.querySelector('.metric-value');
    if (valueEl) {
        valueEl.innerHTML = `${value} <span class="text-lg text-gray-400">${unit}</span>`;
    }

    // Update change indicator
    const changeEl = card.querySelector('.metric-change');
    if (changeEl && change !== null && change !== undefined) {
        if (change > 0) {
            changeEl.className = 'text-xs text-green-400 bg-green-400/10 px-2 py-1 rounded-full';
            changeEl.textContent = `+${change}${unit === '%' ? '%' : ''}`;
        } else if (change < 0) {
            changeEl.className = 'text-xs text-red-400 bg-red-400/10 px-2 py-1 rounded-full';
            changeEl.textContent = `${change}${unit === '%' ? '%' : ''}`;
        } else {
            changeEl.className = 'text-xs text-gray-400 bg-gray-400/10 px-2 py-1 rounded-full';
            changeEl.textContent = '±0';
        }
    }

    // Update status
    if (status !== undefined) {
        const statusEl = card.querySelector('.metric-status');
        if (statusEl) {
            if (status === 'optimal') {
                statusEl.className = 'text-xs text-yellow-400 bg-yellow-400/10 px-2 py-1 rounded-full';
                statusEl.textContent = 'Optimal';
            } else if (status === 'good') {
                statusEl.className = 'text-xs text-green-400 bg-green-400/10 px-2 py-1 rounded-full';
                statusEl.textContent = '✓ In Range';
            } else {
                statusEl.className = 'text-xs text-red-400 bg-red-400/10 px-2 py-1 rounded-full';
                statusEl.textContent = 'Adjust';
            }
        }
    }

    // Add update animation
    card.classList.add('ring-2', 'ring-purple-500/50');
    setTimeout(() => {
        card.classList.remove('ring-2', 'ring-purple-500/50');
    }, 1000);
}

// Update detailed metric
function updateDetailMetric(id, value, unit = '') {
    const el = document.querySelector(`[data-detail="${id}"]`);
    if (el) {
        el.textContent = unit ? `${value} ${unit}` : value;
    }
}

// Check if value is in optimal range
function checkOptimalRange(value, min, max) {
    if (value >= min && value <= max) {
        return 'optimal';
    } else if (Math.abs(value - (min + max) / 2) < (max - min)) {
        return 'good';
    }
    return 'adjust';
}

// Update trajectory visualization
function updateTrajectory(trajectoryData) {
    const svg = document.querySelector('#trajectory-svg');
    if (!svg) return;

    // Clear existing path
    const existingPath = svg.querySelector('.trajectory-path');
    if (existingPath) existingPath.remove();

    // Create new path
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'trajectory-path');
    path.setAttribute('stroke', 'url(#gradient)');
    path.setAttribute('stroke-width', '3');
    path.setAttribute('fill', 'none');

    // Convert trajectory data to SVG path
    let pathData = 'M ';
    trajectoryData.forEach((point, index) => {
        const x = (point.distance / point.maxDistance) * svg.clientWidth;
        const y = svg.clientHeight - (point.height / point.maxHeight) * svg.clientHeight;

        if (index === 0) {
            pathData += `${x} ${y}`;
        } else {
            pathData += ` L ${x} ${y}`;
        }
    });

    path.setAttribute('d', pathData);
    svg.appendChild(path);

    // Animate the path
    const length = path.getTotalLength();
    path.style.strokeDasharray = length;
    path.style.strokeDashoffset = length;
    path.style.animation = 'draw 2s ease-out forwards';
}

// Add recent shot to list
function addRecentShot(shotData) {
    const list = document.querySelector('#recent-shots-list');
    if (!list) return;

    const shotItem = document.createElement('div');
    shotItem.className = 'glass rounded-xl p-4 border border-white/10 hover:border-purple-500/40 transition-all cursor-pointer';
    shotItem.innerHTML = `
        <div class="flex justify-between items-start">
            <div>
                <p class="text-sm font-medium text-white">Shot #${shotData.shot_id}</p>
                <p class="text-xs text-gray-400">${new Date(shotData.timestamp).toLocaleTimeString()}</p>
            </div>
            <div class="text-right">
                <p class="text-lg font-bold text-white">${shotData.carry_distance} yds</p>
                <p class="text-xs text-gray-400">${shotData.ball_speed} mph</p>
            </div>
        </div>
        <div class="mt-2 flex justify-between text-xs">
            <span class="text-gray-400">Launch: ${shotData.launch_angle}°</span>
            <span class="text-gray-400">Spin: ${shotData.spin_rate} rpm</span>
            <span class="text-gray-400">Side: ${shotData.side_angle}°</span>
        </div>
    `;

    // Add to top of list
    if (list.firstChild) {
        list.insertBefore(shotItem, list.firstChild);
    } else {
        list.appendChild(shotItem);
    }

    // Keep only last 10 shots
    while (list.children.length > 10) {
        list.removeChild(list.lastChild);
    }

    // Add click handler
    shotItem.addEventListener('click', () => {
        loadShotDetails(shotData.shot_id);
    });
}

// Load latest shot data
async function loadLatestShot() {
    try {
        const response = await fetch('/api/shots/latest');
        const data = await response.json();

        if (data.success && data.shot) {
            updateDashboardMetrics(data.shot);
        }
    } catch (error) {
        console.error('Failed to load latest shot:', error);
    }
}

// Load session statistics
async function loadSessionStats() {
    try {
        const response = await fetch('/api/session/stats');
        const data = await response.json();

        if (data.success && data.stats) {
            updateSessionStats(data.stats);
        }
    } catch (error) {
        console.error('Failed to load session stats:', error);
    }
}

// Update session statistics
function updateSessionStats(stats) {
    // Update averages
    if (stats.avg_ball_speed) {
        document.querySelector('[data-stat="avg-speed"]').textContent = `${stats.avg_ball_speed} mph`;
    }
    if (stats.avg_carry) {
        document.querySelector('[data-stat="avg-carry"]').textContent = `${stats.avg_carry} yds`;
    }
    if (stats.avg_launch_angle) {
        document.querySelector('[data-stat="avg-launch"]').textContent = `${stats.avg_launch_angle}°`;
    }

    // Update maximums
    if (stats.max_ball_speed) {
        document.querySelector('[data-stat="max-speed"]').textContent = `${stats.max_ball_speed} mph`;
    }
    if (stats.max_carry) {
        document.querySelector('[data-stat="max-carry"]').textContent = `${stats.max_carry} yds`;
    }

    // Update shot count
    if (stats.total_shots) {
        document.querySelector('[data-stat="shot-count"]').textContent = stats.total_shots;
    }
}

// Load shot details
async function loadShotDetails(shotId) {
    try {
        const response = await fetch(`/api/shots/${shotId}`);
        const data = await response.json();

        if (data.success && data.shot) {
            showShotDetails(data.shot);
        }
    } catch (error) {
        console.error('Failed to load shot details:', error);
    }
}

// Show shot details modal
function showShotDetails(shot) {
    // Create modal
    const modal = document.createElement('div');
    modal.className = 'fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50 backdrop-blur-sm';
    modal.innerHTML = `
        <div class="glass rounded-2xl p-6 max-w-2xl w-full border border-white/10">
            <div class="flex justify-between items-start mb-4">
                <h3 class="text-xl font-bold text-white">Shot #${shot.shot_id} Details</h3>
                <button onclick="this.closest('.fixed').remove()" class="p-2 hover:bg-white/10 rounded-lg transition-colors">
                    <svg class="w-6 h-6 text-gray-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
                    </svg>
                </button>
            </div>

            <div class="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
                <div>
                    <p class="text-xs text-gray-400 mb-1">Ball Speed</p>
                    <p class="text-lg font-bold text-white">${shot.ball_speed} mph</p>
                </div>
                <div>
                    <p class="text-xs text-gray-400 mb-1">Launch Angle</p>
                    <p class="text-lg font-bold text-white">${shot.launch_angle}°</p>
                </div>
                <div>
                    <p class="text-xs text-gray-400 mb-1">Spin Rate</p>
                    <p class="text-lg font-bold text-white">${shot.spin_rate} rpm</p>
                </div>
                <div>
                    <p class="text-xs text-gray-400 mb-1">Carry Distance</p>
                    <p class="text-lg font-bold text-white">${shot.carry_distance} yds</p>
                </div>
            </div>

            ${shot.images && shot.images.length > 0 ? `
                <div class="grid grid-cols-2 gap-4">
                    ${shot.images.map(img => `
                        <img src="/images/${img}" alt="Shot image" class="rounded-lg w-full h-auto">
                    `).join('')}
                </div>
            ` : ''}
        </div>
    `;

    document.body.appendChild(modal);

    // Close on background click
    modal.addEventListener('click', (e) => {
        if (e.target === modal) {
            modal.remove();
        }
    });
}

// Initialize charts
function initializeCharts() {
    // This would initialize any chart libraries if needed
    // For now, using SVG for trajectory visualization
}

// Update system status
function updateSystemStatus(data) {
    if (data.system_status) {
        const statusEl = document.querySelector('[data-status="system"]');
        if (statusEl) {
            if (data.system_status === 'running') {
                statusEl.className = 'flex items-center space-x-1';
                statusEl.innerHTML = `
                    <div class="w-2 h-2 bg-green-500 rounded-full pulse-glow"></div>
                    <span class="text-xs text-green-400">Online</span>
                `;
            } else if (data.system_status === 'error') {
                statusEl.className = 'flex items-center space-x-1';
                statusEl.innerHTML = `
                    <div class="w-2 h-2 bg-red-500 rounded-full"></div>
                    <span class="text-xs text-red-400">Error</span>
                `;
            } else {
                statusEl.className = 'flex items-center space-x-1';
                statusEl.innerHTML = `
                    <div class="w-2 h-2 bg-yellow-500 rounded-full"></div>
                    <span class="text-xs text-yellow-400">Waiting</span>
                `;
            }
        }
    }
}

// Export functions for external use
window.updateDashboardMetrics = updateDashboardMetrics;
window.loadShotDetails = loadShotDetails;