// Connection configuration
const baseUrlInput = document.getElementById('baseUrlInput');
const connectBtn = document.getElementById('connectBtn');
const statusPill = document.getElementById('statusPill');
const statusText = document.getElementById('statusText');

// UI Elements
const ldrRaw = document.getElementById('ldrRaw');
const ldrPercent = document.getElementById('ldrPercent');
const ldrVoltage = document.getElementById('ldrVoltage');
const ambientModeLabel = document.getElementById('ambientModeLabel');

// Simulation components
const simSwitch = document.getElementById('simSwitch');
const simLabelText = document.getElementById('simLabelText');
const simSlider = document.getElementById('simSlider');
const simSliderVal = document.getElementById('simSliderVal');
const simSliderGroup = document.getElementById('simSliderGroup');

// Lamp SVG components
const lampBeam = document.getElementById('lampBeam');
const lampGlowCircle = document.getElementById('lampGlowCircle');
const lampBulb = document.getElementById('lampBulb');
const pullChain = document.getElementById('pullChain');
const pullChainCord = document.getElementById('pullChainCord');
const pullChainBell = document.getElementById('pullChainBell');

// History Chart components
const canvas = document.getElementById('historyChart');
const ctx = canvas.getContext('2d');

// State variables
let baseUrl = baseUrlInput.value.trim() || 'http://192.168.4.1';
let isConnected = false;
let simulationMode = false;
let pollInterval = null;
let lastValue = 2000;
const historyData = [];
const maxHistoryLength = 50;

// Set connection status styling
function updateStatusUI(state, message) {
	statusPill.className = 'status-pill';
	if (state === 'connected') {
		statusPill.classList.add('connected');
		statusText.textContent = 'Connected';
		isConnected = true;
	} else if (state === 'simulating') {
		statusPill.classList.add('simulating');
		statusText.textContent = 'Simulating';
		isConnected = false;
	} else {
		statusText.textContent = message || 'Offline';
		isConnected = false;
	}
}

// Compute metrics and update visual components
function updateLuminosityUI(value) {
	lastValue = value;
	
	// Update text elements
	ldrRaw.textContent = value;
	
	// Percentage (12-bit is 0 to 4095)
	const percentage = ((value / 4095) * 100).toFixed(1);
	ldrPercent.textContent = `${percentage}%`;

	// Voltage (ESP32 ADC uses 3.3V reference)
	const voltage = ((value / 4095) * 3.3).toFixed(2);
	ldrVoltage.textContent = `${voltage} V`;

	// Theme switching threshold
	const isDarkTheme = value < 1000;
	if (isDarkTheme) {
		document.documentElement.classList.add('dark-theme');
		ambientModeLabel.textContent = 'Night Mode (Dim)';
		ambientModeLabel.style.color = 'var(--accent)';
	} else {
		document.documentElement.classList.remove('dark-theme');
		ambientModeLabel.textContent = 'Day Mode (Bright)';
		ambientModeLabel.style.color = 'var(--status-ok)';
	}

	// Apply lamp visuals based on luminosity
	const ratio = value / 4095;
	
	// Light beam intensity and size
	lampBeam.style.opacity = ratio * 0.9;
	
	// Glow circle scale & opacity
	lampGlowCircle.style.opacity = ratio * 0.85;
	lampGlowCircle.style.transform = `scale(${0.6 + ratio * 0.6})`;
	lampGlowCircle.style.transformOrigin = '100px 110px';

	// Bulb core visual temperature color transition (Amber-yellow base at dim, bright white at high)
	if (value === 0) {
		lampBulb.style.fill = '#475569'; // lamp is off / black
	} else {
		// Transition bulb color from soft gold to bright white
		lampBulb.style.fill = `rgb(255, ${Math.round(200 + 55 * ratio)}, ${Math.round(100 + 155 * ratio)})`;
	}

	// Push to history data
	historyData.push(value);
	if (historyData.length > maxHistoryLength) {
		historyData.shift();
	}

	drawChart();
}

// Canvas drawing loop for rolling line chart
function drawChart() {
	const dpr = window.devicePixelRatio || 1;
	const rect = canvas.getBoundingClientRect();
	
	// Keep canvas size responsive and sharp on high DPI screens
	if (canvas.width !== rect.width * dpr || canvas.height !== rect.height * dpr) {
		canvas.width = rect.width * dpr;
		canvas.height = rect.height * dpr;
	}
	
	ctx.save();
	ctx.scale(dpr, dpr);
	ctx.clearRect(0, 0, rect.width, rect.height);

	const width = rect.width;
	const height = rect.height;
	const padding = { top: 15, right: 15, bottom: 20, left: 45 };
	const graphWidth = width - padding.left - padding.right;
	const graphHeight = height - padding.top - padding.bottom;

	// Get CSS variable colors
	const textStyle = getComputedStyle(document.documentElement);
	const mutedColor = textStyle.getPropertyValue('--text-muted').trim() || '#64748b';
	const gridColor = textStyle.getPropertyValue('--panel-border').trim() || 'rgba(148, 163, 184, 0.1)';
	const accentColor = textStyle.getPropertyValue('--accent').trim() || '#3b82f6';
	
	// Draw horizontal grid lines and labels
	const gridLevels = [0, 1000, 2000, 3000, 4095];
	ctx.strokeStyle = gridColor;
	ctx.lineWidth = 1;
	ctx.font = '10px "Fira Code", monospace';
	ctx.fillStyle = mutedColor;
	ctx.textAlign = 'right';
	ctx.textBaseline = 'middle';

	gridLevels.forEach(level => {
		const y = padding.top + graphHeight * (1 - level / 4095);
		
		// Draw line
		ctx.beginPath();
		ctx.moveTo(padding.left, y);
		ctx.lineTo(width - padding.right, y);
		ctx.stroke();

		// Draw text label
		ctx.fillText(level, padding.left - 8, y);
	});

	// Draw threshold warning line at 1000
	const thresholdY = padding.top + graphHeight * (1 - 1000 / 4095);
	ctx.strokeStyle = 'rgba(239, 68, 68, 0.35)';
	ctx.lineWidth = 1.5;
	ctx.setLineDash([4, 4]);
	ctx.beginPath();
	ctx.moveTo(padding.left, thresholdY);
	ctx.lineTo(width - padding.right, thresholdY);
	ctx.stroke();
	ctx.setLineDash([]); // Reset dash

	// Draw threshold label on the right
	ctx.fillStyle = 'rgba(239, 68, 68, 0.7)';
	ctx.font = '500 9px sans-serif';
	ctx.textAlign = 'right';
	ctx.fillText('Dark Mode Threshold (1000)', width - padding.right - 4, thresholdY - 8);

	if (historyData.length < 2) {
		ctx.restore();
		return;
	}

	// Plot points
	const points = historyData.map((val, idx) => {
		const x = padding.left + (idx / (maxHistoryLength - 1)) * graphWidth;
		const y = padding.top + graphHeight * (1 - val / 4095);
		return { x, y };
	});

	// Draw gradient area under curve
	const areaGrad = ctx.createLinearGradient(0, padding.top, 0, height - padding.bottom);
	areaGrad.addColorStop(0, accentColor + '30'); // Accent color with opacity
	areaGrad.addColorStop(1, accentColor + '00'); // Transparent

	ctx.beginPath();
	ctx.moveTo(points[0].x, height - padding.bottom);
	points.forEach(pt => ctx.lineTo(pt.x, pt.y));
	ctx.lineTo(points[points.length - 1].x, height - padding.bottom);
	ctx.closePath();
	ctx.fillStyle = areaGrad;
	ctx.fill();

	// Draw line curve
	ctx.strokeStyle = accentColor;
	ctx.lineWidth = 2.5;
	ctx.lineCap = 'round';
	ctx.lineJoin = 'round';
	ctx.beginPath();
	ctx.moveTo(points[0].x, points[0].y);
	for (let i = 1; i < points.length; i++) {
		ctx.lineTo(points[i].x, points[i].y);
	}
	ctx.stroke();

	// Draw current pulsing dot at the last point
	const lastPoint = points[points.length - 1];
	const pulseRadius = 5 + Math.sin(Date.now() / 150) * 2;
	ctx.fillStyle = accentColor;
	ctx.beginPath();
	ctx.arc(lastPoint.x, lastPoint.y, 4, 0, Math.PI * 2);
	ctx.fill();

	ctx.strokeStyle = accentColor + '60';
	ctx.lineWidth = 1.5;
	ctx.beginPath();
	ctx.arc(lastPoint.x, lastPoint.y, pulseRadius, 0, Math.PI * 2);
	ctx.stroke();

	ctx.restore();
}

// Perform network fetch request
async function fetchLuminosity() {
	if (simulationMode) {
		// Let user range slider feed values
		updateLuminosityUI(parseInt(simSlider.value, 10));
		updateStatusUI('simulating');
		return;
	}

	try {
		const controller = new AbortController();
		const timeoutId = setTimeout(() => controller.abort(), 400); // Stop request if longer than 400ms to stay in sync with 500ms intervals

		const response = await fetch(`${baseUrl}/`, {
			method: 'GET',
			cache: 'no-store',
			signal: controller.signal
		});
		clearTimeout(timeoutId);

		if (!response.ok) {
			throw new Error(`HTTP ${response.status}`);
		}

		const data = await response.json();
		if (data && typeof data.luminosity !== 'undefined') {
			const value = parseInt(data.luminosity, 10);
			if (!isNaN(value)) {
				updateLuminosityUI(value);
				updateStatusUI('connected');
			} else {
				throw new Error('Invalid numeric value');
			}
		} else {
			throw new Error('Response JSON missing "luminosity" key');
		}
	} catch (error) {
		updateStatusUI('offline', `Offline: ${error.message}`);
		// Automatically render using the last cached value to maintain curve consistency
		updateLuminosityUI(lastValue);
	}
}

// Pull Chain trigger function
function triggerPullChainAnimation() {
	// Trigger physical animation class
	pullChainCord.classList.remove('pull-chain-animation');
	pullChainBell.classList.remove('pull-chain-animation');
	
	// Force layout recalculation to retrigger keyframe animation
	void pullChainCord.offsetWidth; 
	
	pullChainCord.classList.add('pull-chain-animation');
	pullChainBell.classList.add('pull-chain-animation');
	
	// Toggle Simulation mode
	simSwitch.checked = !simSwitch.checked;
	simSwitch.dispatchEvent(new Event('change'));
}

// Event handlers setup
simSwitch.addEventListener('change', (e) => {
	simulationMode = e.target.checked;
	simLabelText.textContent = simulationMode ? 'ON' : 'OFF';
	
	if (simulationMode) {
		simSliderGroup.style.opacity = '1';
		simSliderGroup.style.pointerEvents = 'auto';
		updateStatusUI('simulating');
	} else {
		simSliderGroup.style.opacity = '0.5';
		simSliderGroup.style.pointerEvents = 'none';
		updateStatusUI(isConnected ? 'connected' : 'offline');
	}
});

simSlider.addEventListener('input', (e) => {
	const val = parseInt(e.target.value, 10);
	simSliderVal.textContent = val;
	if (simulationMode) {
		updateLuminosityUI(val);
	}
});

connectBtn.addEventListener('click', () => {
	baseUrl = baseUrlInput.value.trim() || 'http://192.168.4.1';
	// Strip trailing slash if entered
	if (baseUrl.endsWith('/')) {
		baseUrl = baseUrl.slice(0, -1);
	}
	fetchLuminosity();
});

pullChain.addEventListener('click', triggerPullChainAnimation);

// Adjust animation updates inside canvas
setInterval(() => {
	if (historyData.length > 0) {
		drawChart();
	}
}, 100);

// Initial start loops
updateLuminosityUI(2000); // Initial mockup load
fetchLuminosity();
pollInterval = setInterval(fetchLuminosity, 500);

// Redraw chart when viewport resizes
window.addEventListener('resize', drawChart);
