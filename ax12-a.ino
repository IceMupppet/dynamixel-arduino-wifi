#include <SPI.h>
#include <WiFiS3.h>
#include <Dynamixel2Arduino.h>
#include <ctype.h>

// ===== WiFi credentials =====
char ssid[] = "SASSY";
char pass[] = "mrssneaks7";

// Web server
WiFiServer server(80);

// ===== Dynamixel on UNO R4 + Shield =====
// - Serial1 on pins 0/1 goes to Dynamixel Shield UART
// - Pin 2 is DIR (half-duplex direction)
#define DXL_SERIAL    Serial1
#define DEBUG_SERIAL  Serial
const uint8_t DXL_DIR_PIN = 2;

// Servo config
const uint8_t DXL_ID = 2;               // AX-12A ID (change here if needed)
const float   DXL_PROTOCOL_VERSION = 1.0;   // AX series = protocol 1.0

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
using namespace ControlTableItem;

// Tracking state
bool torqueEnabled    = false;
bool ledOn            = false;
uint16_t lastPosition = 512;   // raw position 0–1023
uint16_t currentSpeedRaw = 80; // MOVING_SPEED 0–1023

String ipString = "0.0.0.0";

// -------- Conversion helpers --------
float rawToDegrees(uint16_t raw) {
  return (300.0f * raw) / 1023.0f;
}

uint16_t degreesToRaw(float deg) {
  if (deg < 0.0f) deg = 0.0f;
  if (deg > 300.0f) deg = 300.0f;
  return (uint16_t)(deg * 1023.0f / 300.0f + 0.5f);
}

// -------- HTTP helpers --------
int getQueryParam(const String &req, const String &key, int defaultVal) {
  String pattern = key + "=";
  int idx = req.indexOf(pattern);
  if (idx < 0) return defaultVal;

  idx += pattern.length();
  int end = idx;

  while (end < (int)req.length() && isDigit((unsigned char)req[end])) {
    end++;
  }

  if (end == idx) return defaultVal;   // no digits

  int val = req.substring(idx, end).toInt();
  return val;
}

void logDebug(const String &msg) {
  DEBUG_SERIAL.println(msg);
}

// ===== Web UI =====
void sendMainPage(WiFiClient &client) {
  // Read live values from servo (best-effort)
  int32_t posRaw  = dxl.readControlTableItem(PRESENT_POSITION,  DXL_ID);
  int32_t spdRaw  = dxl.readControlTableItem(PRESENT_SPEED,     DXL_ID);
  int32_t loadRaw = dxl.readControlTableItem(PRESENT_LOAD,      DXL_ID);
  int32_t tempRaw = dxl.readControlTableItem(PRESENT_TEMPERATURE, DXL_ID);
  int32_t voltRaw = dxl.readControlTableItem(PRESENT_VOLTAGE,   DXL_ID);
  int32_t mvRaw   = dxl.readControlTableItem(MOVING_SPEED,      DXL_ID);

  if (posRaw >= 0 && posRaw <= 1023) {
    lastPosition = (uint16_t)posRaw;
  }
  if (mvRaw >= 0 && mvRaw <= 1023) {
    currentSpeedRaw = (uint16_t)mvRaw;
  }

  float posDeg   = rawToDegrees(lastPosition);
  float voltVal  = (voltRaw >= 0) ? (voltRaw / 10.0f) : 0.0f;

  // Defaults if read failed
  if (spdRaw  < 0) spdRaw  = 0;
  if (loadRaw < 0) loadRaw = 0;
  if (tempRaw < 0) tempRaw = 0;

  // Degrees slider default (rounded)
  int defaultDeg = (int)(posDeg + 0.5f);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  const char *ICON_URL =
    "https://images.squarespace-cdn.com/content/v1/68f79caf137bee198678b5d2/557a29da-93df-4da6-8493-6131894827e4/obsidia-site-icon.png?format=300w";

  const char *SERVO_IMG_URL =
    "https://cdn2.botland.store/55225-large_default/servo-robotis-dynamixel-ax-12a.jpg";

  client.println(F("<!doctype html><html><head>"));
  client.println(F("<meta name='viewport' content='width=device-width, initial-scale=1'>"));
  client.println(F("<title>Dynamixel Control Panel</title>"));
  client.println(F("<style>"
    ":root{--bg:#05070b;--panel:#111822;--panel2:#151e29;--panel3:#0d141d;"
    "--text:#e6edf3;--muted:#909fb3;--green:#00c853;--red:#ff1744;--accent:#2bb8a5;}"
    "body{margin:0;background:radial-gradient(circle at top,#101521,#05070b);"
    "color:var(--text);font-family:-apple-system,system-ui,'Segoe UI',Roboto,sans-serif;}"
    ".topbar{background:rgba(3,7,13,0.95);border-bottom:1px solid #1f2933;"
    "backdrop-filter:blur(6px);padding:10px 18px;}"
    ".brand{display:flex;align-items:center;gap:12px;max-width:520px;margin:0 auto;}"
    ".brand-logo{width:32px;height:32px;border-radius:8px;background:#021013;"
    "display:flex;align-items:center;justify-content:center;overflow:hidden;}"
    ".brand-logo img{width:100%;height:100%;object-fit:cover;}"
    ".brand-text-main{font-size:18px;font-weight:600;letter-spacing:0.14em;"
    "text-transform:uppercase;color:var(--accent);}"
    ".brand-text-sub{font-size:11px;color:var(--muted);}"
    ".wrap{max-width:820px;margin:0 auto;padding:18px 18px 28px 18px;}"
    ".grid{display:grid;grid-template-columns:minmax(0,1.1fr) minmax(0,1fr);gap:16px;}"
    "@media(max-width:780px){.grid{grid-template-columns:1fr;}}"
    ".panel{background:var(--panel);border-radius:14px;padding:16px;"
    "box-shadow:0 14px 32px rgba(0,0,0,0.55);border:1px solid #1e2633;margin-bottom:16px;}"
    ".panel-header{font-size:13px;text-transform:uppercase;letter-spacing:0.18em;"
    "color:var(--muted);margin-bottom:8px;}"
    "h2{margin:0 0 6px 0;font-size:18px;}"
    "p{margin:0 0 8px 0;font-size:13px;color:var(--muted);}"
    "button{font-size:15px;padding:9px 14px;border-radius:10px;border:0;color:#fff;"
    "cursor:pointer;font-weight:600;letter-spacing:0.03em;margin-right:6px;margin-top:6px;}"
    ".btn-green{background:linear-gradient(135deg,#00c853,#1de9b6);}"
    ".btn-gray{background:#374151;color:#e5e7eb;font-size:14px;}"
    ".row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;}"
    ".slider-group{margin-top:10px;font-size:12px;color:var(--muted);}"
    ".slider-label{display:flex;justify-content:space-between;margin-bottom:4px;}"
    ".slider{width:100%;}"
    ".stat-table{width:100%;border-collapse:collapse;font-size:13px;margin-top:6px;}"
    ".stat-table th,.stat-table td{padding:4px 6px;}"
    ".stat-table th{background:#f5f5f5;color:#111827;text-align:left;}"
    ".stat-table td{background:#111827;border-top:1px solid #1f2933;}"
    ".highlight-cell{background:#fef3c7;color:#111827;font-weight:600;}"
    ".stat-val{color:var(--text);}"
    ".servo-img{width:100%;border-radius:12px;background:#020509;"
    "border:1px solid #202938;overflow:hidden;}"
    ".servo-img img{width:100%;display:block;}"
    ".servo-label{margin-top:8px;font-size:14px;font-weight:600;}"
    ".servo-sub{font-size:12px;color:var(--muted);}"
    ".toggle-row{display:flex;align-items:center;gap:10px;margin-top:4px;}"
    ".toggle-label{font-size:13px;color:var(--muted);}"
    ".toggle{position:relative;display:inline-block;width:46px;height:24px;}"
    ".toggle input{display:none;}"
    ".toggle-slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;"
      "background:#374151;border-radius:999px;transition:.2s;}"
    ".toggle-slider:before{content:'';position:absolute;height:18px;width:18px;left:3px;top:3px;"
      "background:#9ca3af;border-radius:50%;transition:.2s;}"
    ".toggle input:checked + .toggle-slider{background:linear-gradient(135deg,#00c853,#1de9b6);}"
    ".toggle input:checked + .toggle-slider:before{transform:translateX(20px);background:#ffffff;}"
    "</style>"));
  client.println(F("</head><body>"));

  // Header
  client.println(F("<header class='topbar'><div class='brand'>"));
  client.print(F("<div class='brand-logo'><img src='"));
  client.print(ICON_URL);
  client.println(F("' alt='icon'></div>"));
  client.println(F("<div>"));
  client.println(F("<div class='brand-text-main'>DYNAMIXEL CONTROL</div>"));
  client.print(F("<div class='brand-text-sub'>AX-12A Servo &bull; ID "));
  client.print(DXL_ID);
  client.println(F("</div></div></div></header>"));

  client.println(F("<div class='wrap'>"));

  // Main layout: left = servo/info, right = controls
  client.println(F("<div class='grid'>"));

  // Left panel: servo image + status
  client.println(F("<div class='panel'>"));
  client.println(F("<div class='panel-header'>Servo</div>"));
  client.println(F("<div class='servo-img'>"));
  client.print(F("<img src='"));
  client.print(SERVO_IMG_URL);
  client.println(F("' alt='AX-12A servo'>"));
  client.println(F("</div>"));
  client.print(F("<div class='servo-label'>AX-12A Servo &mdash; ID "));
  client.print(DXL_ID);
  client.println(F("</div>"));
  client.println(F("<div class='servo-sub'>Live telemetry read from the servo on page load.</div>"));

  // Status table like Wizard
  client.println(F("<table class='stat-table'>"));
  client.println(F("<tr><th class='highlight-cell'>Position</th>"
                   "<td class='stat-val'>"));
  client.print(posDeg, 1);
  client.print(F(" &deg;  (raw "));
  client.print(lastPosition);
  client.println(F(")</td></tr>"));

  client.println(F("<tr><th class='highlight-cell'>Speed</th><td class='stat-val'>"));
  client.print(spdRaw);
  client.println(F(" [raw]</td></tr>"));

  client.println(F("<tr><th class='highlight-cell'>Load</th><td class='stat-val'>"));
  client.print(loadRaw);
  client.println(F(" [raw]</td></tr>"));

  client.println(F("<tr><th class='highlight-cell'>Temperature</th><td class='stat-val'>"));
  client.print(tempRaw);
  client.println(F(" &deg;C</td></tr>"));

  client.println(F("<tr><th class='highlight-cell'>Voltage</th><td class='stat-val'>"));
  client.print(voltVal, 2);
  client.println(F(" V</td></tr>"));

  client.println(F("</table>"));

  client.println(F("</div>")); // end left panel

  // Right panel: controls
  client.println(F("<div class='panel'>"));
  client.println(F("<div class='panel-header'>Servo Controls</div>"));
  client.println(F("<h2>Realtime Control</h2>"));
  client.println(F("<p>Toggle torque and LED, adjust goal position (in degrees), and set velocity.</p>"));

  // Torque toggle
  client.println(F("<div class='toggle-row'>"));
  client.println(F("<span class='toggle-label'>Torque</span>"));
  client.println(F("<label class='toggle'>"));
  client.print(F("<input type='checkbox' id='torqueToggle' "));
  if (torqueEnabled) client.print(F("checked"));
  client.println(F("><span class='toggle-slider'></span></label>"));
  client.println(F("</div>"));

  // LED toggle
  client.println(F("<div class='toggle-row'>"));
  client.println(F("<span class='toggle-label'>LED</span>"));
  client.println(F("<label class='toggle'>"));
  client.print(F("<input type='checkbox' id='ledToggle' "));
  if (ledOn) client.print(F("checked"));
  client.println(F("><span class='toggle-slider'></span></label>"));
  client.println(F("</div>"));

  // Position slider (degrees -> raw)
  client.println(F("<div class='slider-group'>"));
  client.println(F("<div class='slider-label'><span>Goal Position</span>"
                   "<span><span id='degVal'>"));
  client.print(defaultDeg);
  client.println(F("</span>&deg;  (<span id='rawVal'>"));
  client.print(lastPosition);
  client.println(F("</span>)</span></div>"));
  client.print(F("<input id='degSlider' class='slider' type='range' min='0' max='300' value='"));
  client.print(defaultDeg);
  client.println(F("'>"));
  client.println(F("<div class='row'>"));
  client.println(F("<button class='btn-gray' onclick='moveToPosition()'>Move to Position</button>"));
  client.println(F("<button class='btn-gray' onclick='moveToCenter()'>Move to Center</button>"));
  client.println(F("</div>"));
  client.println(F("</div>"));

  // Speed slider
  client.println(F("<div class='slider-group'>"));
  client.println(F("<div class='slider-label'><span>Velocity (MOVING_SPEED)</span>"
                   "<span><span id='speedVal'>"));
  client.print(currentSpeedRaw);
  client.println(F("</span> [0&ndash;1023]</span></div>"));
  client.print(F("<input id='speedSlider' class='slider' type='range' min='0' max='1023' value='"));
  client.print(currentSpeedRaw);
  client.println(F("'>"));
  client.println(F("<button class='btn-gray' onclick='setSpeed()'>Update Speed</button>"));
  client.println(F("</div>"));

  client.println(F("</div>")); // right panel

  client.println(F("</div>")); // grid

  // Footer-ish info
  client.println(F("<div class='panel'>"));
  client.println(F("<div class='panel-header'>Connection</div>"));
  client.print(F("<p>Device IP: <b>"));
  client.print(ipString);
  client.println(F("</b></p>"));
  client.println(F("</div>"));

  // JS
  client.println(F("<script>"
    "document.addEventListener('DOMContentLoaded',function(){"
      "var dS=document.getElementById('degSlider');"
      "var dV=document.getElementById('degVal');"
      "var rV=document.getElementById('rawVal');"
      "function updPos(){"
        "var deg=parseInt(dS.value)||0;"
        "if(deg<0)deg=0;if(deg>300)deg=300;"
        "var raw=Math.round(deg*1023/300);"
        "dV.textContent=deg;"
        "rV.textContent=raw;"
      "}"
      "dS.addEventListener('input',updPos);"
      "updPos();"

      "var sS=document.getElementById('speedSlider');"
      "var sV=document.getElementById('speedVal');"
      "function updSpeed(){sV.textContent=sS.value;}"
      "sS.addEventListener('input',updSpeed);"
      "updSpeed();"

      "document.getElementById('torqueToggle').addEventListener('change',function(){"
        "var on=this.checked?1:0;"
        "fetch('/torque?val='+on);"
      "});"
      "document.getElementById('ledToggle').addEventListener('change',function(){"
        "var on=this.checked?1:0;"
        "fetch('/led?val='+on);"
      "});"
    "});"

    "function moveToPosition(){"
      "var dS=document.getElementById('degSlider');"
      "var deg=parseInt(dS.value)||0;"
      "if(deg<0)deg=0;if(deg>300)deg=300;"
      "var raw=Math.round(deg*1023/300);"
      "fetch('/move?pos='+raw);"
    "}"
    "function moveToCenter(){"
      "var centerDeg=150;"
      "var raw=Math.round(centerDeg*1023/300);"
      "document.getElementById('degSlider').value=centerDeg;"
      "var dV=document.getElementById('degVal');"
      "var rV=document.getElementById('rawVal');"
      "dV.textContent=centerDeg;"
      "rV.textContent=raw;"
      "fetch('/move?pos='+raw);"
    "}"
    "function setSpeed(){"
      "var sS=document.getElementById('speedSlider');"
      "var val=parseInt(sS.value)||0;"
      "if(val<0)val=0;if(val>1023)val=1023;"
      "fetch('/speed?val='+val);"
    "}"
    "</script>"));

  client.println(F("</div></body></html>"));
}

void handleClient(WiFiClient &client) {
  String req = "";
  unsigned long timeout = millis();

  while (client.connected() && millis() - timeout < 1000) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (c == '\n' && req.endsWith("\r\n\r\n")) break;
    }
  }

  if (req.indexOf("GET /torque") >= 0) {
    int val = getQueryParam(req, "val", 1);
    if (val != 0) {
      dxl.torqueOn(DXL_ID);
      torqueEnabled = true;
      logDebug("Torque ON");
    } else {
      dxl.torqueOff(DXL_ID);
      torqueEnabled = false;
      logDebug("Torque OFF");
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println();
  }
  else if (req.indexOf("GET /led") >= 0) {
    int val = getQueryParam(req, "val", 1);
    if (val != 0) {
      dxl.ledOn(DXL_ID);
      ledOn = true;
      logDebug("LED ON");
    } else {
      dxl.ledOff(DXL_ID);
      ledOn = false;
      logDebug("LED OFF");
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println();
  }
  else if (req.indexOf("GET /move") >= 0) {
    int pos = getQueryParam(req, "pos", (int)lastPosition);
    if (pos < 0) pos = 0;
    if (pos > 1023) pos = 1023;
    lastPosition = (uint16_t)pos;

    logDebug("Move to position: " + String(lastPosition));
    dxl.writeControlTableItem(GOAL_POSITION, DXL_ID, lastPosition);

    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println();
  }
  else if (req.indexOf("GET /speed") >= 0) {
    int val = getQueryParam(req, "val", (int)currentSpeedRaw);
    if (val < 0) val = 0;
    if (val > 1023) val = 1023;
    currentSpeedRaw = (uint16_t)val;

    dxl.writeControlTableItem(MOVING_SPEED, DXL_ID, currentSpeedRaw);
    logDebug("Set speed (MOVING_SPEED) to: " + String(currentSpeedRaw));

    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println();
  }
  else {
    // Main page
    sendMainPage(client);
  }

  delay(1);
  client.stop();
}

// ===== Setup / loop =====
void setup() {
  DEBUG_SERIAL.begin(115200);
  while (!DEBUG_SERIAL) {}

  DEBUG_SERIAL.println("=== Dynamixel Web Control (UNO R4 + Shield + AX-12A ID 2) ===");

  // WiFi
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    DEBUG_SERIAL.print("Connecting to WiFi ");
    DEBUG_SERIAL.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }
  ipString = WiFi.localIP().toString();
  DEBUG_SERIAL.print("WiFi connected. IP = ");
  DEBUG_SERIAL.println(ipString);

  server.begin();

  // Dynamixel
  dxl.begin(1000000); // 1 Mbps
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);

  if (dxl.ping(DXL_ID)) {
    DEBUG_SERIAL.print("Ping OK on ID ");
    DEBUG_SERIAL.println(DXL_ID);
  } else {
    DEBUG_SERIAL.println("Ping FAILED. Check ID, power, wiring.");
  }

  // Ensure joint mode: CW_ANGLE_LIMIT < CCW_ANGLE_LIMIT
  dxl.torqueOff(DXL_ID);
  dxl.writeControlTableItem(CW_ANGLE_LIMIT,  DXL_ID, 0);
  dxl.writeControlTableItem(CCW_ANGLE_LIMIT, DXL_ID, 1023);

  // Set moderate moving speed
  dxl.writeControlTableItem(MOVING_SPEED, DXL_ID, currentSpeedRaw);

  dxl.torqueOn(DXL_ID);
  torqueEnabled = true;

  dxl.ledOff(DXL_ID);
  ledOn = false;

  // Initialize lastPosition from present position if possible
  int32_t p = dxl.readControlTableItem(PRESENT_POSITION, DXL_ID);
  if (p >= 0 && p <= 1023) {
    lastPosition = (uint16_t)p;
  }

  DEBUG_SERIAL.println("Ready. Open http://" + ipString + " in your browser.");
}

void loop() {
  WiFiClient client = server.available();
  if (client) handleClient(client);
}
