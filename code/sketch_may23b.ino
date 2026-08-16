#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <EEPROM.h>

// Initialize the PCA9685 PWM Driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// PCA9685 Pulse length configuration (Standard 50Hz servos)
#define SERVOMIN 100
#define SERVOMAX 600

// 5 Servos: 0: Shoulder, 1: Elbow, 2: Base, 3: Gripper S1, 4: Gripper S2
#define NUM_SERVOS 5        
#define MAX_POSITIONS 50
#define EEPROM_SIZE 2048

// Access Point Credentials
const char* ssid = "RoboticArm_AP";
const char* password = "123456789";

WebServer server(80);

// ==========================================================
// --- ⚙️ SERVO LIMITS & CALIBRATION ⚙️ ---
// ==========================================================
// { Shoulder(0), Elbow(1), Base(2), Gripper(3), Gripper_S2(4) }
const int MIN_ANGLES[NUM_SERVOS] = { 60,    60,    10,   30,    0 }; 
const int MAX_ANGLES[NUM_SERVOS] = { 120,   130,   85,   90,  180 }; 
// ==========================================================

// Servo Control Arrays 
float currentAngle[NUM_SERVOS] = {89, 85, 25, 90, 100};
float targetAngle[NUM_SERVOS]  = {89, 85, 25, 90, 100};
float velocity[NUM_SERVOS]     = {0, 0, 0, 0, 0};

// --- NEW SPEED & SMOOTHING VARIABLES ---
float speedBase = 1.5f; 
float speedArm  = 1.5f;
float maxSpeedBase = 0.3f * speedBase;
float maxSpeedArm  = 0.3f * speedArm;

float smoothnessFactor = 50.0f; // Range: 1 to 100
float currentAccel = 0.02f;
float currentDecelDist = 15.0f;

// Integrated Gripper Configuration Parameters
int offset2 = 10;       
int openAngle = 90;     
int gripStrength = 70;  

// Physical Hardware Pins
int closeButton = 25;
int openButton  = 26;                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
const int ledRed = 13;   // Red LED Pin
const int ledGreen = 14; // Green LED Pin

bool lastCloseState = HIGH;
bool lastOpenState  = HIGH;

// EEPROM Data Structure
struct Position {
  float angles[NUM_SERVOS];
  uint16_t delayMs;
  bool valid;
};
Position savedPositions[MAX_POSITIONS];
int positionCount = 0;

// Automation Playback State Machines
enum PlaybackMode { MANUAL, AUTO_PLAY, PAUSED, STOPPED };
PlaybackMode currentMode = MANUAL;
int currentPlaybackIndex = 0;
unsigned long lastPositionTime = 0;
bool loopMode = false;
bool emergencyStop = false;
uint16_t defaultDelay = 1000;

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 25; 

// Anti-Jolt Safety Feature
bool servosActive = false; 

// Serial Monitor Timer
unsigned long lastSerialPrint = 0;

// Coordinate Vectors
const float homePosition[NUM_SERVOS] = {89, 85, 25, 90, 100};
const float customPosition[NUM_SERVOS] = {120, 94, 25, 90, 100}; 

// Function Prototypes
void setServoPulse(int channel, float angle);
void setGripperChannels(float pos);

// Global Wake-Up Function to prevent sequential snapping
void wakeUpServos() {
  if (!servosActive) {
    for (int i = 0; i < NUM_SERVOS; i++) {
      if (i < 3) {
        setServoPulse(i, currentAngle[i]);
      } else if (i == 3) {
        setGripperChannels(currentAngle[3]);
      }
    }
    servosActive = true;
  }
}

// Returns Re-engineered Professional UI Webpage Layout
String getWebPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<title>Robotic Arm Control Panel</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root {
  --bg-primary: #f8fafc;
  --bg-card: #ffffff;
  --text-main: #0f172a;
  --text-muted: #64748b;
  --border-color: #cbd5e1;
  --accent-color: #2563eb;
  --accent-hover: #1d4ed8;
  --success-color: #10b981;
  --danger-color: #ef4444;
  --warning-color: #f59e0b;
  --shadow: 0 4px 6px -1px rgba(0,0,0,0.05), 0 2px 4px -1px rgba(0,0,0,0.03);
}

[data-theme="dark"] {
  --bg-primary: #0f172a;
  --bg-card: #1e293b;
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --border-color: #334155;
  --accent-color: #3b82f6;
  --accent-hover: #2563eb;
  --shadow: 0 4px 6px -1px rgba(0,0,0,0.3);
}

* { margin: 0; padding: 0; box-sizing: border-box; transition: background-color 0.3s, border-color 0.3s, color 0.3s; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg-primary); color: var(--text-main); min-height: 100vh; padding: 20px; line-height: 1.5; overflow-x: hidden; }
.container { max-width: 1000px; margin: 0 auto; }

header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; padding-bottom: 15px; border-bottom: 1px solid var(--border-color); }
h1 { font-size: 1.5em; font-weight: 600; letter-spacing: -0.025em; }

.header-controls { display: flex; gap: 10px; }

/* Sidebar Menu Styles - FIXED CSS GLITCH */
.side-menu {
  position: fixed; top: 0; right: -350px; width: 100%; max-width: 320px; height: 100vh;
  background: var(--bg-card); box-shadow: -4px 0 15px rgba(0,0,0,0.1);
  transition: right 0.3s cubic-bezier(0.4, 0, 0.2, 1); z-index: 2000; padding: 20px;
  overflow-y: auto; border-left: 1px solid var(--border-color);
}
.side-menu.open { right: 0; }
.side-menu h2 { margin-bottom: 20px; display: flex; justify-content: space-between; align-items: center; }
.close-btn { background: none; border: none; font-size: 1.5em; color: var(--text-muted); cursor: pointer; }
.overlay {
  position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
  background: rgba(0,0,0,0.5); z-index: 1999; display: none;
  opacity: 0; transition: opacity 0.3s;
}
.overlay.active { display: block; opacity: 1; }

.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 20px; }
@media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }

.card { background-color: var(--bg-card); border: 1px solid var(--border-color); border-radius: 10px; padding: 20px; box-shadow: var(--shadow); }
.card h2 { font-size: 1.1em; font-weight: 600; margin-bottom: 15px; color: var(--text-main); }

.servo-control { margin-bottom: 20px; }
.servo-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.servo-title { font-size: 0.9em; font-weight: 500; color: var(--text-muted); }
.angle-display { background-color: var(--bg-primary); border: 1px solid var(--border-color); padding: 4px 10px; border-radius: 6px; font-size: 0.9em; font-weight: 600; min-width: 55px; text-align: center; }

.slider { width: 100%; height: 6px; border-radius: 3px; background: var(--border-color); outline: none; -webkit-appearance: none; margin: 8px 0; }
.slider::-webkit-slider-thumb { -webkit-appearance: none; width: 16px; height: 16px; border-radius: 50%; background: var(--accent-color); cursor: pointer; border: none; }
.slider::-moz-range-thumb { width: 16px; height: 16px; border-radius: 50%; background: var(--accent-color); cursor: pointer; border: none; }
.range-labels { display: flex; justify-content: space-between; font-size: 0.75em; color: var(--text-muted); }

.btn { padding: 10px 16px; border: 1px solid transparent; border-radius: 6px; font-size: 0.9em; font-weight: 500; cursor: pointer; transition: all 0.2s; text-align: center; }
.btn-primary { background-color: var(--accent-color); color: white; }
.btn-primary:hover { background-color: var(--accent-hover); }
.btn-danger { background-color: var(--danger-color); color: white; }
.btn-danger:hover { opacity: 0.9; }
.btn-warning { background-color: var(--warning-color); color: white; }
.btn-warning:hover { opacity: 0.9; }
.btn-outline { background-color: transparent; border-color: var(--border-color); color: var(--text-main); }
.btn-outline:hover { background-color: var(--bg-primary); }
.btn-block { width: 100%; margin: 6px 0; }
.btn-flex { flex: 1; }
.flex-row { display: flex; gap: 10px; margin: 10px 0; }

.settings-item { display: flex; flex-direction: column; gap: 4px; margin-bottom: 15px; }
.settings-item label { font-size: 0.8em; font-weight: 600; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; }

.toggle-container { display: flex; align-items: center; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid var(--border-color); }
.toggle-label { font-size: 0.85em; font-weight: 500; }
.switch { position: relative; display: inline-block; width: 36px; height: 20px; }
.switch input { opacity: 0; width: 0; height: 0; }
.toggle-slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: var(--border-color); transition: .3s; border-radius: 20px; }
.toggle-slider:before { position: absolute; content: ""; height: 14px; width: 14px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
input:checked + .toggle-slider { background-color: var(--success-color); }
input:checked + .toggle-slider:before { transform: translateX(16px); }

.position-list { max-height: 250px; overflow-y: auto; background: var(--bg-primary); border: 1px solid var(--border-color); border-radius: 6px; padding: 10px; }
.position-item { background: var(--bg-card); border: 1px solid var(--border-color); padding: 10px; margin-bottom: 8px; border-radius: 4px; display: flex; justify-content: space-between; align-items: center; }
.position-info strong { font-size: 0.85em; color: var(--text-main); }
.position-angles { font-size: 0.75em; color: var(--text-muted); margin-top: 2px; }
.btn-small { padding: 4px 8px; font-size: 0.8em; border-radius: 4px; }

.status-footer { display: flex; justify-content: space-between; align-items: center; background-color: var(--bg-card); border: 1px solid var(--border-color); padding: 12px 20px; border-radius: 10px; margin-top: 20px; box-shadow: var(--shadow); }
.status-meta { display: flex; align-items: center; gap: 8px; font-size: 0.9em; font-weight: 500; }
.status-dot { width: 8px; height: 8px; border-radius: 50%; }
.status-manual { background: var(--accent-color); }
.status-playing { background: var(--success-color); }
.status-paused { background: var(--warning-color); }
.status-stopped { background: var(--danger-color); }
.badge { font-size: 0.7em; font-weight: 600; padding: 4px 8px; border-radius: 10px; text-transform: uppercase; letter-spacing: 0.05em; background: var(--bg-primary); border: 1px solid var(--border-color); }

.emergency-zone { position: fixed; bottom: 20px; right: 20px; z-index: 1000; }
.btn-emergency { background-color: var(--danger-color); color: white; font-size: 0.9em; font-weight: 600; padding: 12px 20px; border-radius: 25px; border: 2px solid var(--bg-card); box-shadow: 0 4px 12px rgba(239, 68, 68, 0.4); text-transform: uppercase; }
</style>
</head>
<body>

<div class="overlay" id="overlay" onclick="toggleMenu()"></div>

<div class="side-menu" id="sideMenu">
  <h2>Settings <button class="close-btn" onclick="toggleMenu()">&times;</button></h2>
  
  <div class="settings-item">
    <label>Playback Delay: <span id="delayValue" style="text-transform:none; color:var(--accent-color)">1000ms</span></label>
    <input type="range" min="0" max="5000" value="1000" step="100" class="slider" id="delaySlider" oninput="updateDelay(this.value)">
  </div>

  <div class="settings-item">
    <label>Base Speed (Ax 2): <span id="speedBaseValue" style="text-transform:none; color:var(--accent-color)">1.5x</span></label>
    <input type="range" min="15" max="50" value="15" class="slider" id="speedBaseSlider" oninput="updateSpeedBase(this.value)">
  </div>

  <div class="settings-item">
    <label>Arm Speed (Ax 0,1): <span id="speedArmValue" style="text-transform:none; color:var(--accent-color)">1.5x</span></label>
    <input type="range" min="15" max="50" value="15" class="slider" id="speedArmSlider" oninput="updateSpeedArm(this.value)">
  </div>

  <div class="settings-item">
    <label>Motion Smoothness: <span id="smoothValue" style="text-transform:none; color:var(--accent-color)">50%</span></label>
    <input type="range" min="1" max="100" value="50" class="slider" id="smoothnessSlider" oninput="updateSmoothness(this.value)">
  </div>

  <div class="settings-item">
    <label>Grip Limit: <span id="strengthValue" style="text-transform:none; color:var(--accent-color)">55°</span></label>
    <input type="range" min="30" max="85" value="55" class="slider" id="strengthSlider" oninput="updateStrength(this.value)">
  </div>

  <div class="toggle-container" style="margin-bottom: 15px;">
    <span class="toggle-label">Loop Sequence</span>
    <label class="switch">
      <input type="checkbox" id="loopCheck" onchange="toggleLoop(this.checked)">
      <span class="toggle-slider"></span>
    </label>
  </div>

  <label style="font-size: 0.8em; font-weight: 600; color: var(--text-muted); text-transform: uppercase;">Quick Actions</label>
  <button class="btn btn-primary btn-block" onclick="goHome()" style="margin-top:10px;">Reset to Home</button>
  <button class="btn btn-outline btn-block" onclick="goCustomPos()">Go to Custom Pos</button>
  <button class="btn btn-outline btn-block" onclick="toggleTheme()" id="themeBtn">Toggle Dark Mode</button>
</div>

<div class="container">
  <header>
    <h1>Arm Dashboard</h1>
    <div class="header-controls">
      <button class="btn btn-outline btn-small" onclick="toggleMenu()">☰ Menu</button>
    </div>
  </header>
  
  <div class="grid">
    <div>
      <div class="card">
        <h2>Joint Controls</h2>
        
        <div class="servo-control">
          <div class="servo-header"><span class="servo-title">Shoulder (Axis 0)</span><span class="angle-display" id="angle0">89°</span></div>
          <input type="range" min="60" max="120" value="89" class="slider" id="slider0" oninput="updateServo(0, this.value)">
          <div class="range-labels"><span>60°</span><span>120°</span></div>
        </div>

        <div class="servo-control">
          <div class="servo-header"><span class="servo-title">Elbow (Axis 1)</span><span class="angle-display" id="angle1">85°</span></div>
          <input type="range" min="60" max="130" value="85" class="slider" id="slider1" oninput="updateServo(1, this.value)">
          <div class="range-labels"><span>60°</span><span>130°</span></div>
        </div>

        <div class="servo-control">
          <div class="servo-header"><span class="servo-title">Base (Axis 2)</span><span class="angle-display" id="angle2">25°</span></div>
          <input type="range" min="10" max="85" value="25" class="slider" id="slider2" oninput="updateServo(2, this.value)">
          <div class="range-labels"><span>10°</span><span>85°</span></div>
        </div>
      </div>

      <div class="card" style="margin-top: 20px;">
        <h2>End Effector</h2>
        <div class="flex-row">
          <button class="btn btn-primary btn-flex" onclick="gripObject()">Grip Object</button>
          <button class="btn btn-outline btn-flex" onclick="releaseObject()">Release</button>
        </div>
      </div>
    </div>

    <div>
      <div class="card" style="height: 100%; display: flex; flex-direction: column;">
        <h2>Position Sequences</h2>
        <div class="position-list" id="positionList" style="flex-grow: 1; margin-bottom: 12px;">
          <p style="text-align: center; opacity: 0.5; padding: 20px; font-size: 0.85em;">No positions saved in memory.</p>
        </div>
        <button class="btn btn-primary btn-block" onclick="savePosition()">Save Current Position</button>
        <div class="flex-row">
          <button class="btn btn-outline btn-flex" onclick="startPlayback()">Start</button>
          <button class="btn btn-outline btn-flex" onclick="pausePlayback()">Pause</button>
          <button class="btn btn-outline btn-flex" onclick="stopPlayback()">Stop</button>
        </div>
        <button class="btn btn-outline btn-block" onclick="clearAllPositions()" style="border-color: transparent; color: var(--danger-color);">Clear Memory</button>
      </div>
    </div>
  </div>

  <div class="status-footer">
    <div class="status-meta">
      <span class="status-dot" id="statusIndicator"></span>
      <span id="statusText">System Ready</span>
    </div>
    <span class="badge" id="modeBadge">Manual</span>
  </div>
</div>

<div class="emergency-zone">
  <button class="btn-emergency" onclick="emergencyStop()">Emergency Stop</button>
</div>

<script>
let positions = [];

// NEW: Variables for throttling and smooth UI
let sliderTouched = [0, 0, 0]; 
let lastFetchTime = 0;
let pendingFetch = null;

function toggleMenu() {
  document.getElementById('sideMenu').classList.toggle('open');
  document.getElementById('overlay').classList.toggle('active');
}

function toggleTheme() {
  const body = document.documentElement;
  if (body.getAttribute('data-theme') === 'dark') {
    body.removeAttribute('data-theme');
  } else {
    body.setAttribute('data-theme', 'dark');
  }
}

function updateServo(servo, angle) {
  if (servo < 3) {
    document.getElementById('angle' + servo).textContent = angle + '°';
    sliderTouched[servo] = Date.now(); // Mark that you are actively moving this slider
  }
  
  // Throttle HTTP requests to prevent crashing the ESP32 (Max 20 requests per second)
  const now = Date.now();
  if (now - lastFetchTime > 50) {
    sendServoCommand(servo, angle);
    lastFetchTime = now;
  } else {
    clearTimeout(pendingFetch);
    pendingFetch = setTimeout(() => {
      sendServoCommand(servo, angle);
      lastFetchTime = Date.now();
    }, 50);
  }
}

// Helper function for the throttled fetch
function sendServoCommand(servo, angle) {
  fetch('/set?servo=' + servo + '&angle=' + angle).catch(e => console.error("Network limit:", e));
}

function gripObject() { fetch('/grip'); }
function releaseObject() { fetch('/release'); }
function updateStrength(val) { document.getElementById('strengthValue').textContent = val + '°'; fetch('/setStrength?strength=' + val); }

function savePosition() {
  fetch('/savePosition').then(r => r.json()).then(data => { if(data.success) updatePositionList(); });
}

function updatePositionList() {
  fetch('/getPositions').then(r => r.json()).then(data => {
      positions = data.positions;
      let html = '';
      if(positions.length === 0) {
        html = '<p style="text-align: center; opacity: 0.5; padding: 20px; font-size: 0.85em;">No positions saved in memory.</p>';
      } else {
        positions.forEach((pos, index) => {
          html += `
            <div class="position-item">
              <div class="position-info">
                <strong>Frame ${index + 1}</strong>
                <div class="position-angles">Shldr: ${pos.angles[0].toFixed(0)}° | Elbw: ${pos.angles[1].toFixed(0)}° | Base: ${pos.angles[2].toFixed(0)}° | G: ${pos.angles[3].toFixed(0)}°</div>
              </div>
              <div class="position-actions">
                <button class="btn btn-outline btn-small" onclick="goToPosition(${index})">Run</button>
                <button class="btn btn-outline btn-small" onclick="deletePosition(${index})" style="border-color:transparent; color:var(--danger-color)">Del</button>
              </div>
            </div>`;
        });
      }
      document.getElementById('positionList').innerHTML = html;
    });
}

function deletePosition(index) { fetch('/deletePosition?index=' + index).then(r => r.json()).then(() => updatePositionList()); }
function goToPosition(index) { fetch('/gotoPosition?index=' + index); }
function clearAllPositions() { if(confirm('Clear storage memory?')) { fetch('/clearPositions').then(() => updatePositionList()); } }
function startPlayback() { fetch('/startPlayback'); }
function pausePlayback() { fetch('/pausePlayback'); }
function stopPlayback() { fetch('/stopPlayback'); }
function emergencyStop() { fetch('/emergencyStop'); updateStatus(); }
function goHome() { fetch('/goHome'); }
function goCustomPos() { fetch('/goCustomPos'); }

function updateSpeedBase(value) { 
  let speed = value / 10; 
  document.getElementById('speedBaseValue').textContent = speed.toFixed(1) + 'x'; 
  fetch('/setSpeedBase?speed=' + speed); 
}
function updateSpeedArm(value) { 
  let speed = value / 10; 
  document.getElementById('speedArmValue').textContent = speed.toFixed(1) + 'x'; 
  fetch('/setSpeedArm?speed=' + speed); 
}
function updateSmoothness(value) { 
  document.getElementById('smoothValue').textContent = value + '%'; 
  fetch('/setSmoothness?val=' + value); 
}

function updateDelay(value) { document.getElementById('delayValue').textContent = value + 'ms'; fetch('/setDelay?delay=' + value); }
function toggleLoop(checked) { fetch('/setLoop?loop=' + (checked ? '1' : '0')); }

function updateStatus() {
  fetch('/status')
    .then(r => r.json())
    .then(data => {
      let indicator = document.getElementById('statusIndicator');
      let text = document.getElementById('statusText');
      let badge = document.getElementById('modeBadge');
      
      indicator.className = 'status-dot status-' + data.mode;
      badge.textContent = data.mode;
      
      if(data.mode === 'manual') { text.textContent = 'System Ready'; } 
      else if(data.mode === 'playing') { text.textContent = 'Running Sequence: ' + (data.currentPos + 1) + ' / ' + data.totalPos; } 
      else if(data.mode === 'paused') { text.textContent = 'Paused at Frame ' + (data.currentPos + 1); } 
      else if(data.mode === 'stopped') { text.textContent = 'Execution Stopped'; }
      
      for(let i = 0; i < 3; i++) {
        let el = document.getElementById('angle' + i);
        let slider = document.getElementById('slider' + i);
        
        // FIX: Only sync slider to arm position if you haven't touched the slider in the last 1.5 seconds
        // OR if the system is currently Auto-Playing (so the UI tracks the automated movement smoothly)
        let timeSinceTouched = Date.now() - sliderTouched[i];
        if ((document.activeElement !== slider && timeSinceTouched > 1500) || data.mode === 'playing') {
          if(el) el.textContent = Math.round(data.angles[i]) + '°';
          if(slider) slider.value = Math.round(data.angles[i]);
        }
      }
    });
}

setInterval(updatePositionList, 2000);
// FIX: Sped up UI polling from 500ms to 200ms for smoother visual animations during playback
setInterval(updateStatus, 200); 
updatePositionList();
</script>
</body>
</html>
)rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); 
  
  EEPROM.begin(EEPROM_SIZE);
  
  pwm.begin();
  pwm.setPWMFreq(50);
  
  for(int i = 0; i < 16; i++) {
    pwm.setPWM(i, 0, 0); 
  }
  
  pinMode(closeButton, INPUT_PULLUP);
  pinMode(openButton, INPUT_PULLUP);
  
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  digitalWrite(ledRed, HIGH); 
  digitalWrite(ledGreen, LOW);
  
  loadPositionsFromEEPROM();
  
  WiFi.softAP(ssid, password);
  Serial.println("\n=================================");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  Serial.println("=================================\n");
  
  setupWebServer();
  server.begin();
}

void setupWebServer() {
  server.on("/", []() {
    server.send(200, "text/html", getWebPage());
  });
  
  server.on("/set", []() {
    if(currentMode == MANUAL || currentMode == STOPPED) {
      wakeUpServos(); 
      int servo = server.arg("servo").toInt();
      int angle = server.arg("angle").toInt();
      
      if(servo >= 0 && servo < NUM_SERVOS) {
        angle = constrain(angle, MIN_ANGLES[servo], MAX_ANGLES[servo]);
        
        if(servo < 3) {
          targetAngle[servo] = angle;
        } else if (servo == 3) {
          targetAngle[3] = angle;
          
          int s2_angle = (180 - angle) + offset2;
          targetAngle[4] = constrain(s2_angle, MIN_ANGLES[4], MAX_ANGLES[4]);
        }
        server.send(200, "text/plain", "OK");
      }
    } else {
      server.send(400, "text/plain", "Not in manual mode");
    }
  });

  server.on("/grip", []() {
    if(currentMode == MANUAL || currentMode == STOPPED) {
      wakeUpServos();
      targetAngle[3] = constrain(gripStrength, MIN_ANGLES[3], MAX_ANGLES[3]);
      targetAngle[4] = constrain((180 - gripStrength) + offset2, MIN_ANGLES[4], MAX_ANGLES[4]);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Busy");
    }
  });

  server.on("/release", []() {
    if(currentMode == MANUAL || currentMode == STOPPED) {
      wakeUpServos();
      targetAngle[3] = constrain(openAngle, MIN_ANGLES[3], MAX_ANGLES[3]);
      targetAngle[4] = constrain((180 - openAngle) + offset2, MIN_ANGLES[4], MAX_ANGLES[4]);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Busy");
    }
  });

  server.on("/setStrength", []() {
    gripStrength = server.arg("strength").toInt();
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/savePosition", []() {
    String response = "{";
    if(positionCount < MAX_POSITIONS) {
      savedPositions[positionCount].valid = true;
      for(int i = 0; i < NUM_SERVOS; i++) {
        savedPositions[positionCount].angles[i] = currentAngle[i];
      }
      savedPositions[positionCount].delayMs = defaultDelay;
      positionCount++;
      savePositionsToEEPROM();
      response += "\"success\":true,\"count\":" + String(positionCount);
    } else {
      response += "\"success\":false,\"message\":\"Maximum positions reached\"";
    }
    response += "}";
    server.send(200, "application/json", response);
  });

  server.on("/getPositions", []() {
    String json = "{\"positions\":[";
    for(int i = 0; i < positionCount; i++) {
      if(savedPositions[i].valid) {
        json += "{\"angles\":[";
        for(int j = 0; j < NUM_SERVOS; j++) {
          json += String(savedPositions[i].angles[j], 1);
          if(j < NUM_SERVOS - 1) json += ",";
        }
        json += "],\"delay\":" + String(savedPositions[i].delayMs) + "}";
        if(i < positionCount - 1) json += ",";
      }
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/deletePosition", []() {
    int index = server.arg("index").toInt();
    if(index >= 0 && index < positionCount) {
      for(int i = index; i < positionCount - 1; i++) {
        savedPositions[i] = savedPositions[i + 1];
      }
      positionCount--;
      savePositionsToEEPROM();
      server.send(200, "application/json", "{\"success\":true}");
    }
  });

  server.on("/gotoPosition", []() {
    int index = server.arg("index").toInt();
    if(index >= 0 && index < positionCount && currentMode == MANUAL) {
      wakeUpServos();
      for(int i = 0; i < NUM_SERVOS; i++) {
        targetAngle[i] = constrain(savedPositions[index].angles[i], MIN_ANGLES[i], MAX_ANGLES[i]);
      }
      server.send(200, "text/plain", "OK");
    }
  });

  server.on("/clearPositions", []() {
    positionCount = 0;
    savePositionsToEEPROM();
    server.send(200, "text/plain", "OK");
  });

  server.on("/startPlayback", []() {
    if(positionCount > 0) {
      wakeUpServos();
      currentMode = AUTO_PLAY;
      currentPlaybackIndex = 0;
      emergencyStop = false;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/pausePlayback", []() {
    if(currentMode == AUTO_PLAY) currentMode = PAUSED;
    server.send(200, "text/plain", "OK");
  });

  server.on("/stopPlayback", []() {
    currentMode = STOPPED;
    server.send(200, "text/plain", "OK");
  });

  server.on("/emergencyStop", []() {
    emergencyStop = true;
    currentMode = STOPPED;
    for(int i = 0; i < NUM_SERVOS; i++) {
      velocity[i] = 0;
      targetAngle[i] = currentAngle[i];
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/goHome", []() {
    if(currentMode == MANUAL || currentMode == STOPPED) {
      wakeUpServos();
      for(int i = 0; i < NUM_SERVOS; i++) {
        targetAngle[i] = constrain(homePosition[i], MIN_ANGLES[i], MAX_ANGLES[i]);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/goCustomPos", []() {
    if(currentMode == MANUAL || currentMode == STOPPED) {
      wakeUpServos();
      for(int i = 0; i < NUM_SERVOS; i++) {
        targetAngle[i] = constrain(customPosition[i], MIN_ANGLES[i], MAX_ANGLES[i]);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // --- NEW SPEED & SMOOTHNESS ENDPOINTS ---
  server.on("/setSpeedBase", []() {
    speedBase = server.arg("speed").toFloat();
    maxSpeedBase = 0.3f * speedBase; 
    server.send(200, "text/plain", "OK");
  });

  server.on("/setSpeedArm", []() {
    speedArm = server.arg("speed").toFloat();
    maxSpeedArm = 0.3f * speedArm;
    server.send(200, "text/plain", "OK");
  });

  server.on("/setSmoothness", []() {
    smoothnessFactor = server.arg("val").toFloat();
    // Maps 1 to 100 into physics variables. 1 = Jerky/Fast, 100 = Cinematic/Slow
    currentAccel = 0.1f - ((smoothnessFactor - 1.0f) / 99.0f) * (0.1f - 0.005f);
    currentDecelDist = 5.0f + ((smoothnessFactor - 1.0f) / 99.0f) * (25.0f - 5.0f);
    server.send(200, "text/plain", "OK");
  });

  server.on("/setDelay", []() {
    defaultDelay = server.arg("delay").toInt();
    server.send(200, "text/plain", "OK");
  });

  server.on("/setLoop", []() {
    loopMode = server.arg("loop").toInt() == 1;
    server.send(200, "text/plain", "OK");
  });

  server.on("/status", []() {
    String mode = "manual";
    if(currentMode == AUTO_PLAY) mode = "playing";
    else if(currentMode == PAUSED) mode = "paused";
    else if(currentMode == STOPPED) mode = "stopped";
    
    String json = "{";
    json += "\"mode\":\"" + mode + "\",";
    json += "\"currentPos\":" + String(currentPlaybackIndex) + ",";
    json += "\"totalPos\":" + String(positionCount) + ",";
    json += "\"angles\":[";
    for(int i = 0; i < NUM_SERVOS; i++) {
      json += String(currentAngle[i], 1);
      if(i < NUM_SERVOS - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });
}

void loop() {
  server.handleClient();
  if(emergencyStop) return;
  
  unsigned long currentTime = millis();

  bool closeState = digitalRead(closeButton);
  bool openState  = digitalRead(openButton);
  
  if (currentMode == MANUAL || currentMode == STOPPED) {
    if (lastCloseState == HIGH && closeState == LOW) {
      wakeUpServos();
      targetAngle[3] = constrain(gripStrength, MIN_ANGLES[3], MAX_ANGLES[3]);
      targetAngle[4] = constrain((180 - gripStrength) + offset2, MIN_ANGLES[4], MAX_ANGLES[4]);
    }
    if (lastOpenState == HIGH && openState == LOW) {
      wakeUpServos();
      targetAngle[3] = constrain(openAngle, MIN_ANGLES[3], MAX_ANGLES[3]);
      targetAngle[4] = constrain((180 - openAngle) + offset2, MIN_ANGLES[4], MAX_ANGLES[4]);
    }
  }
  lastCloseState = closeState;
  lastOpenState = openState;
  
  bool isHoldingObject = (currentAngle[3] <= ((openAngle + gripStrength) / 2));
  digitalWrite(ledGreen, isHoldingObject ? HIGH : LOW);
  digitalWrite(ledRed, isHoldingObject ? LOW : HIGH);

  if (currentTime - lastSerialPrint >= 500) {
    lastSerialPrint = currentTime;
    Serial.print("[Arm] Shldr: "); Serial.print(currentAngle[0], 1);
    Serial.print(" | Elbow: "); Serial.print(currentAngle[1], 1);
    Serial.print(" | Base: "); Serial.print(currentAngle[2], 1);
    Serial.print(" | Grip: ");  Serial.print(currentAngle[3], 1);
    Serial.print(" | Status: "); Serial.println(isHoldingObject ? "PICKED" : "EMPTY");
  }
  
  if(currentMode == AUTO_PLAY) {
    handlePlayback(currentTime);
  }
  
  if(currentTime - lastUpdate >= updateInterval) {
    lastUpdate = currentTime;
    if (servosActive) {
      updateServoPositions();
    }
  }
}

void handlePlayback(unsigned long currentTime) {
  if(positionCount == 0) {
    currentMode = STOPPED;
    return;
  }
  
  bool allAtTarget = true;
  for(int i = 0; i < NUM_SERVOS; i++) {
    if(abs(targetAngle[i] - currentAngle[i]) > 0.5f) {
      allAtTarget = false;
      break;
    }
  }
  
  if(allAtTarget && (currentTime - lastPositionTime >= savedPositions[currentPlaybackIndex].delayMs)) {
    currentPlaybackIndex++;
    if(currentPlaybackIndex >= positionCount) {
      if(loopMode) {
        currentPlaybackIndex = 0;
      } else {
        currentMode = STOPPED;
        return;
      }
    }
    
    for(int i = 0; i < NUM_SERVOS; i++) {
      targetAngle[i] = constrain(savedPositions[currentPlaybackIndex].angles[i], MIN_ANGLES[i], MAX_ANGLES[i]);
    }
    lastPositionTime = currentTime;
  }
}

void updateServoPositions() {
  static int gripSettledCount = 0; 

  for(int i = 0; i < NUM_SERVOS; i++) {
    float error = targetAngle[i] - currentAngle[i];
    
    if(abs(error) > 0.1f) {
      // Determines which independent speed control this servo follows
      float dynamicMaxSpeed = 0.4f; 
      if (i == 2) dynamicMaxSpeed = maxSpeedBase;
      else if (i == 0 || i == 1) dynamicMaxSpeed = maxSpeedArm;
      
      // FIX: Real-time velocity braking clamp
      // Forces the motor to instantly drop to the new max speed if you slide it down mid-motion
      if (velocity[i] > dynamicMaxSpeed) {
         velocity[i] = dynamicMaxSpeed; 
      }

      float distance = abs(error);
      
      if(distance > currentDecelDist && velocity[i] < dynamicMaxSpeed) {
        velocity[i] += currentAccel;
        if(velocity[i] > dynamicMaxSpeed) velocity[i] = dynamicMaxSpeed;
      } else if(distance <= currentDecelDist) {
        float slowdown = (distance / currentDecelDist) * dynamicMaxSpeed;
        velocity[i] = max(slowdown, 0.1f);
      }
      
      float step = velocity[i];
      if(error < 0) step = -step;
      currentAngle[i] += step;
      
      if(abs(targetAngle[i] - currentAngle[i]) < abs(step)) {
        currentAngle[i] = targetAngle[i];
        velocity[i] = 0;
      }
      
      if(i < 3) {
        setServoPulse(i, currentAngle[i]);
      } else if(i == 3) {
        setGripperChannels(currentAngle[3]);
        gripSettledCount = 0; 
      }
    } else {
      velocity[i] = 0;
      if (i == 3) {
        gripSettledCount++;
        if (gripSettledCount == 20) {
          pwm.setPWM(3, 0, 0); 
          pwm.setPWM(4, 0, 0); 
        }
      }
    }
  }
}

void setServoPulse(int channel, float angle) {
  angle = constrain(angle, MIN_ANGLES[channel], MAX_ANGLES[channel]);
  int pulse = map(angle * 10, 0, 1800, SERVOMIN, SERVOMAX);
  pwm.setPWM(channel, 0, pulse);
}

void setGripperChannels(float pos) {
  float angle1 = constrain(pos, MIN_ANGLES[3], MAX_ANGLES[3]);
  float angle2 = constrain((180 - pos) + offset2, MIN_ANGLES[4], MAX_ANGLES[4]);
  
  int pulse1 = map(angle1 * 10, 0, 1800, SERVOMIN, SERVOMAX);
  int pulse2 = map(angle2 * 10, 0, 1800, SERVOMIN, SERVOMAX);
  
  pwm.setPWM(3, 0, pulse1); 
  pwm.setPWM(4, 0, pulse2); 
}

void savePositionsToEEPROM() {
  int addr = 0;
  EEPROM.write(addr++, positionCount);
  for(int i = 0; i < positionCount; i++) {
    for(int j = 0; j < NUM_SERVOS; j++) {
      int angleInt = (int)(savedPositions[i].angles[j] * 10);
      EEPROM.write(addr++, angleInt >> 8);
      EEPROM.write(addr++, angleInt & 0xFF);
    }
    EEPROM.write(addr++, savedPositions[i].delayMs >> 8);
    EEPROM.write(addr++, savedPositions[i].delayMs & 0xFF);
  }
  EEPROM.commit();
}

void loadPositionsFromEEPROM() {
  int addr = 0;
  positionCount = EEPROM.read(addr++);
  if(positionCount > MAX_POSITIONS) {
    positionCount = 0;
    return;
  }
  for(int i = 0; i < positionCount; i++) {
    savedPositions[i].valid = true;
    for(int j = 0; j < NUM_SERVOS; j++) {
      int angleInt = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
      savedPositions[i].angles[j] = angleInt / 10.0f;
    }
    savedPositions[i].delayMs = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
  }
}