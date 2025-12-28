#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_now.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_wifi.h"
#include "driver/adc.h"
#include <Preferences.h>

// --------------------- CONFIGURATION -------------------------------
const char* ssid = "RC_Car";
const char* password = "23571113";
#define WIFI_CHANNEL 1

// --------------------- PINS ----------------------------------------
const int LED_PIN = 2;
const int BTN_PIN = 5;

int m1_In1 = 14;
int m1_In2 = 33;
int m1_En = 13;

int m3_In1 = 22;
int m3_In2 = 23;
int m3_En = 32;

int m2_In1 = 27;
int m2_In2 = 26;
int m2_En = 25;

int m4_In1 = 19;
int m4_In2 = 21;
int m4_En = 18;

// --------------------- OBJECTS & VARS ------------------------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences preferences;

// Data Structure
typedef struct struct_message {
  int throttle;
  int steering;
  bool armed;
} struct_message;

struct_message gloveData;

const int freq = 30000;
const int resolution = 8;

// State Variables
bool useGloveMode = true;
unsigned long lastPacketTime = 0;
const int SAFETY_TIMEOUT = 1000;
bool systemArmed = false;

// Button Debounce
unsigned long btnPressTime = 0;
bool btnHeld = false;

// LED State
unsigned long ledTimer = 0;
bool ledState = false;
bool isPinging = false;
unsigned long pingStart = 0;

// --------------------- HTML PAGE -----------------------------------
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
    <style>
      body {
        font-family: sans-serif;
        text-align: center;
        background: #1a1a1a;
        color: #fff;
        margin: 0;
        height: 100vh;
        display: flex;
        flex-direction: column;
        touch-action: none;
        overflow: hidden;
      }

      /* TOP BAR */
      .top-bar {
        padding: 5px 10px;
        background: #222;
        border-bottom: 1px solid #444;
        display: flex;
        justify-content: space-between;
        align-items: center;
        height: 40px;
        flex-shrink: 0;
      }

      #status {
        font-family: monospace;
        color: #ffeb3b;
        font-weight: bold;
        margin-left: 5px;
        font-size: 0.9rem;
      }

      .conn-dot {
        height: 8px;
        width: 8px;
        background-color: #bbb;
        border-radius: 50%;
        display: inline-block;
        margin-right: 5px;
      }

      .connected {
        background-color: #0f0;
        box-shadow: 0 0 5px #0f0;
      }

      .btn-stop {
        background: #d9534f;
        width: 60px;
        height: 28px;
        font-size: 0.8rem;
        font-weight: bold;
        border: 2px solid #ffaaaa;
        border-radius: 4px;
        display: flex;
        justify-content: center;
        align-items: center;
        cursor: pointer;
      }

      /* OVERLAY */
      #overlay {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background: rgba(0, 0, 0, 0.9);
        z-index: 99;
        display: none;
        flex-direction: column;
        justify-content: center;
        align-items: center;
      }

      #overlay h1 {
        color: #d63384;
        font-size: 2.5rem;
      }

      /* MAIN CONTAINER */
      .main-container {
        flex-grow: 1;
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        width: 100%;
        padding: 5px;
        box-sizing: border-box;
      }

      /* CENTER PANEL */
      .center-panel {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 10px;
        margin-bottom: 15px;
        z-index: 10;
      }

      .toggles {
        display: flex;
        justify-content: center;
        gap: 8px;
        flex-wrap: wrap;
      }

      .btn {
        padding: 8px 12px;
        font-weight: bold;
        border: none;
        border-radius: 6px;
        cursor: pointer;
        color: white;
        font-size: 0.85rem;
      }

      .btn-arm {
        background: #5bc0de;
        color: #000;
      }

      .btn-ping {
        background: #f0ad4e;
        color: #000;
      }

      .btn-gyro {
        background: #6c757d;
      }

      .active {
        background: #5cb85c;
        color: white;
        border: 2px solid #fff;
      }

      .slider-cont {
        width: 220px;
        text-align: center;
        margin-top: 5px;
      }

      input[type=range].max-slider {
        width: 100%;
        height: 15px;
        border-radius: 10px;
        background: #555;
        outline: none;
      }

      /* CONTROLS */
      .drive-btn {
        width: 70px;
        height: 70px;
        font-size: 30px;
        border: none;
        border-radius: 50%;
        background: #444;
        color: white;
        box-shadow: 0 4px #000;
        touch-action: none;
        display: flex;
        justify-content: center;
        align-items: center;
        line-height: 0;
      }

      .drive-btn:active {
        background: #007bff;
        transform: translateY(4px);
        box-shadow: 0 0 #000;
      }

      .pressed {
        background: #007bff !important;
        transform: translateY(4px);
        box-shadow: 0 0 #000 !important;
      }

      input[type=range].custom-slider {
        -webkit-appearance: none;
        background: transparent;
        outline: none;
        cursor: pointer;
        margin: 0;
        padding: 0;
      }

      input[type=range].custom-slider::-webkit-slider-runnable-track {
        width: 100%;
        height: 14px;
        background: #444;
        border-radius: 7px;
        border: 1px solid #555;
      }

      input[type=range].custom-slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        height: 45px;
        width: 45px;
        border-radius: 50%;
        background: #007bff;
        margin-top: -16px;
        box-shadow: 0 0 8px rgba(0, 0, 0, 0.8);
        border: 2px solid #fff;
      }

      .throttle-wrapper {
        position: relative;
        width: 54px;
        height: 216px;
        background: rgba(0, 0, 0, 0.3);
        border-radius: 15px;
        border: 1px solid #555;
        margin-bottom: 20vh;
        overflow: hidden;
      }

      .throttle-slider {
        position: absolute;
        top: 50%;
        left: 50%;
        height: 60px;
        transform: translate(-50%, -50%) rotate(-90deg);
        margin: 0;
        padding: 0;
      }

      .steering-group {
        display: flex;
        gap: 20px;
        align-items: center;
        margin-bottom: 20vh;
      }

      .throttle-group {
        display: flex;
        align-items: center;
        justify-content: center;
      }

      .control-row,
      #steering-zone,
      #throttle-zone {
        opacity: 0.3;
        pointer-events: none;
        transition: 0.3s;
      }

      .control-active {
        opacity: 1 !important;
        pointer-events: auto !important;
      }

      @media (orientation: landscape) {
        .main-container {
          flex-direction: row;
          justify-content: space-between;
          padding: 0 40px;
        }

        .control-row {
          display: none;
        }

        #steering-zone {
          display: flex;
          order: 1;
          margin-bottom: 0;
        }

        #center-group {
          display: flex;
          order: 2;
          flex-direction: column;
        }

        #throttle-zone {
          display: flex;
          order: 3;
          margin-bottom: 0;
        }

        .throttle-wrapper {
          height: 198px;
          margin-bottom: 0;
          transform: translateY(-5vh);
        }

        .throttle-slider {
          width: 194px;
        }

        .drive-btn {
          width: 85px;
          height: 85px;
          font-size: 35px;
          opacity: 0.9;
        }
      }

      @media (orientation: portrait) {
        #steering-zone,
        #throttle-zone {
          display: none;
        }

        .control-row {
          display: flex;
          width: 100%;
          justify-content: space-between;
          align-items: center;
          padding: 0 10px;
          margin-top: auto;
        }

        .throttle-slider {
          width: 216px;
        }
      }
    </style>
  </head>
  <body>
    <div id="overlay">
      <h1>GLOVE MODE</h1>
      <p>Web Control Disabled</p>
    </div>
    <div class="top-bar">
      <div id="status">
        <span id="dot" class="conn-dot"></span>CONNECTING...
      </div>
      <div class="btn btn-stop" onclick="emergencyStop()">STOP</div>
    </div>
    <div class="main-container">
      <div id="steering-zone" class="steering-group">
        <button class="drive-btn" id="btnLeftL" oncontextmenu="return false;">&#9668;</button>
        <button class="drive-btn" id="btnRightL" oncontextmenu="return false;">&#9658;</button>
      </div>
      <div class="center-panel" id="center-group">
        <div class="toggles">
          <button class="btn btn-ping" onclick="sendPing()">PING</button>
          <button id="armBtn" class="btn btn-arm" onclick="toggleArm()">UNLOCK</button>
          <button id="gyroBtn" class="btn btn-gyro" onclick="togglePhoneGyro()">PHONE GYRO</button>
        </div>
        <div class="slider-cont"> Max: <span id="spdVal">200</span>
          <input type="range" class="max-slider" min="100" max="255" value="200" oninput="setLimit(this.value)">
        </div>
      </div>
      <div id="throttle-zone" class="throttle-group">
        <div class="throttle-wrapper">
          <input type="range" class="custom-slider throttle-slider" id="thrSliderL" min="-255" max="255" value="0" step="10" oninput="updateThrottle(this.value)">
        </div>
      </div>
      <div class="control-row" id="portraitControls">
        <div class="steering-group">
          <button class="drive-btn" id="btnLeftP" oncontextmenu="return false;">&#9668;</button>
          <button class="drive-btn" id="btnRightP" oncontextmenu="return false;">&#9658;</button>
        </div>
        <div class="throttle-group">
          <div class="throttle-wrapper">
            <input type="range" class="custom-slider throttle-slider" id="thrSliderP" min="-255" max="255" value="0" step="10" oninput="updateThrottle(this.value)">
          </div>
        </div>
      </div>
    </div>
    <script>
      var socket;
      var isArmed = false;
      var isPhoneGyro = false;
      var throttle = 0;
      var steering = 0;
      var maxSpd = 200;
      var keys = {
        Left: false,
        Right: false,
        Up: false,
        Down: false
      };

      function initWebSocket() {
        socket = new WebSocket('ws://' + window.location.hostname + ':81/');
        socket.onopen = function() {
          document.getElementById("status").innerHTML = '<span id="dot" class="conn-dot connected"></span>ONLINE';
          setInterval(sendDataLoop, 50);
        };
        socket.onmessage = function(event) {
          if (event.data === "GLOVE") {
            document.getElementById("overlay").style.display = "flex";
          } else if (event.data === "WEB") {
            document.getElementById("overlay").style.display = "none";
          }
        };
        socket.onclose = function() {
          document.getElementById("status").innerHTML = '<span id="dot" class="conn-dot"></span>OFFLINE';
          setTimeout(initWebSocket, 500);
        };
      }

      function bump() {
        if (window.navigator && window.navigator.vibrate) {
          window.navigator.vibrate(5);
        }
      }

      function updateThrottle(val) {
        if (!isArmed || isPhoneGyro) return;
        let spd = parseInt(val);
        if (spd > maxSpd) spd = maxSpd;
        if (spd < -maxSpd) spd = -maxSpd;
        if (throttle !== spd) bump();
        throttle = spd;
        document.getElementById("thrSliderL").value = val;
        document.getElementById("thrSliderP").value = val;
      }

      function releaseThrottle() {
        throttle = 0;
        document.getElementById("thrSliderL").value = 0;
        document.getElementById("thrSliderP").value = 0;
        bump();
      }

      function sendDataLoop() {
        if (!isArmed || socket.readyState !== WebSocket.OPEN) return;
        if (!isPhoneGyro) {
          steering = 0;
          if (keys.Left) steering -= maxSpd;
          if (keys.Right) steering += maxSpd;
        }
        socket.send(Math.round(throttle) + ":" + Math.round(steering));
      }

      function sendPing() {
        if (socket.readyState == WebSocket.OPEN) {
          socket.send("P");
          bump();
        }
      }

      function emergencyStop() {
        isArmed = false;
        isPhoneGyro = false;
        resetControls();
        updateUI();
        bump();
        if (socket.readyState == WebSocket.OPEN) {
          socket.send("0:0");
        }
      }

      function toggleArm() {
        isArmed = !isArmed;
        bump();
        if (!isArmed) {
          isPhoneGyro = false;
          resetControls();
        }
        updateUI();
      }

      function resetControls() {
        throttle = 0;
        steering = 0;
        keys = {
          Left: false,
          Right: false,
          Up: false,
          Down: false
        };
        document.getElementById("thrSliderL").value = 0;
        document.getElementById("thrSliderP").value = 0;
      }

      function setupSpring(id) {
        let sl = document.getElementById(id);
        if (!sl) return;
        sl.addEventListener("touchend", releaseThrottle);
        sl.addEventListener("mouseup", releaseThrottle);
      }
      setupSpring("thrSliderL");
      setupSpring("thrSliderP");

      function setupBtn(id, key) {
        let btn = document.getElementById(id);
        if (!btn) return;
        btn.addEventListener('touchstart', (e) => {
          e.preventDefault();
          if (!isPhoneGyro) keys[key] = true;
          btn.classList.add('pressed');
        });
        btn.addEventListener('touchend', (e) => {
          e.preventDefault();
          if (!isPhoneGyro) keys[key] = false;
          btn.classList.remove('pressed');
        });
        btn.addEventListener('mousedown', () => {
          if (!isPhoneGyro) keys[key] = true;
        });
        btn.addEventListener('mouseup', () => {
          if (!isPhoneGyro) keys[key] = false;
        });
        btn.addEventListener('mouseleave', () => {
          if (!isPhoneGyro) keys[key] = false;
        });
      }
      setupBtn('btnLeftL', 'Left');
      setupBtn('btnRightL', 'Right');
      setupBtn('btnLeftP', 'Left');
      setupBtn('btnRightP', 'Right');

      function togglePhoneGyro() {
        bump();
        if (!isArmed) {
          alert("PLEASE UNLOCK FIRST!");
          return;
        }
        if (typeof DeviceOrientationEvent.requestPermission === 'function') {
          DeviceOrientationEvent.requestPermission().then(p => {
            if (p === 'granted') activatePhoneGyro();
            else alert("Denied");
          }).catch(e => alert(e));
        } else if (window.DeviceOrientationEvent) {
          activatePhoneGyro();
        } else {
          alert("No Gyro Support");
        }
      }

      function activatePhoneGyro() {
        isPhoneGyro = !isPhoneGyro;
        resetControls();
        if (isPhoneGyro) window.addEventListener("deviceorientation", handlePhoneOrientation);
        else window.removeEventListener("deviceorientation", handlePhoneOrientation);
        updateUI();
      }

      function handlePhoneOrientation(e) {
        if (!isPhoneGyro || !isArmed) return;
        var y = e.beta;
        if (y > 35) y = 35;
        if (y < -35) y = -35;
        var x = e.gamma;
        if (x > 35) x = 35;
        if (x < -35) x = -35;
        throttle = (y / 35) * maxSpd * -1;
        steering = (x / 35) * maxSpd * -1;
        if (Math.abs(throttle) < 30) throttle = 0;
        if (Math.abs(steering) < 30) steering = 0;
      }

      function updateUI() {
        let armBtn = document.getElementById("armBtn");
        let gyroBtn = document.getElementById("gyroBtn");
        let pControls = document.getElementById("portraitControls");
        let lsSteer = document.getElementById("steering-zone");
        let lsThrot = document.getElementById("throttle-zone");
        if (isArmed) {
          armBtn.innerText = "ARMED";
          armBtn.classList.add("active");
          pControls.classList.add("control-active");
          lsSteer.classList.add("control-active");
          lsThrot.classList.add("control-active");
        } else {
          armBtn.innerText = "UNLOCK";
          armBtn.classList.remove("active");
          pControls.classList.remove("control-active");
          lsSteer.classList.remove("control-active");
          lsThrot.classList.remove("control-active");
        }
        if (isPhoneGyro) {
          gyroBtn.classList.add("active");
          gyroBtn.innerText = "GYRO ON";
        } else {
          gyroBtn.classList.remove("active");
          gyroBtn.innerText = "PHONE GYRO";
        }
      }

      function setLimit(val) {
        maxSpd = val;
        document.getElementById("spdVal").innerText = val;
      }
      window.addEventListener('keydown', (e) => {
        if (!isArmed || isPhoneGyro) return;
        if (e.key === "ArrowLeft") keys.Left = true;
        if (e.key === "ArrowRight") keys.Right = true;
        if (e.key === "ArrowUp") {
          let val = parseInt(document.getElementById("thrSliderP").value) + 10;
          updateThrottle(val);
        }
        if (e.key === "ArrowDown") {
          let val = parseInt(document.getElementById("thrSliderP").value) - 10;
          updateThrottle(val);
        }
      });
      window.addEventListener('keyup', (e) => {
        if (e.key === "ArrowLeft") keys.Left = false;
        if (e.key === "ArrowRight") keys.Right = false;
        if (e.key === "ArrowUp" || e.key === "ArrowDown") releaseThrottle();
      });
      window.onload = initWebSocket;
    </script>
  </body>
</html>
)rawliteral";

// ------------------------ HARDWARE SETUP ---------------------------
void setupMotor(int p1, int p2, int en) {
  pinMode(p1, OUTPUT);
  pinMode(p2, OUTPUT);
  pinMode(en, OUTPUT);
  ledcAttach(en, freq, resolution);
  ledcWrite(en, 0);
}

void driveRaw(int p1, int p2, int en, int spd) {
  spd = constrain(spd, -255, 255);
  if (spd == 0) {
    digitalWrite(p1, LOW);
    digitalWrite(p2, LOW);
    ledcWrite(en, 0);
  } else if (spd > 0) {
    digitalWrite(p1, HIGH);
    digitalWrite(p2, LOW);
    ledcWrite(en, spd);
  } else {
    digitalWrite(p1, LOW);
    digitalWrite(p2, HIGH);
    ledcWrite(en, abs(spd));
  }
}

void applyMixer(int thr, int str) {
  if (thr < 0) str = -str;
  int leftSpeed = constrain(thr + str, -255, 255);
  int rightSpeed = constrain(thr - str, -255, 255);
  driveRaw(m2_In1, m2_In2, m2_En, leftSpeed);
  driveRaw(m4_In1, m4_In2, m4_En, leftSpeed);
  driveRaw(m1_In1, m1_In2, m1_En, rightSpeed);
  driveRaw(m3_In1, m3_In2, m3_En, rightSpeed);
}

// ------------------------ ESP-NOW CALLBACK -------------------------
void OnDataRecv(const uint8_t * mac, const uint8_t * incomingData, int len) {
  memcpy(&gloveData, incomingData, sizeof(gloveData));
  
  if (useGloveMode) {
    lastPacketTime = millis();
    systemArmed = gloveData.armed;
    if (systemArmed) {
      applyMixer(gloveData.throttle, gloveData.steering);
    } else {
      applyMixer(0, 0);
    }
  }
}

// ------------------------ MODE SWITCHING ---------------------------
void toggleMode() {
  useGloveMode = !useGloveMode;
  preferences.putBool("mode", useGloveMode);

  applyMixer(0, 0); // Safety stop

  // Flash LED 3 times to confirm switch
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);

  if (useGloveMode) {
    webSocket.broadcastTXT("GLOVE");
  } else {
    webSocket.broadcastTXT("WEB");
  }
}

void handleButton() {
  if (digitalRead(BTN_PIN) == LOW) {
    if (btnPressTime == 0) btnPressTime = millis();
    if ((millis() - btnPressTime > 1000) && !btnHeld) {
      btnHeld = true;
      toggleMode();
    }
  } else {
    btnPressTime = 0;
    btnHeld = false;
  }
}

// ------------------------ SETUP ------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);

  setupMotor(m1_In1, m1_In2, m1_En);
  setupMotor(m2_In1, m2_In2, m2_En);
  setupMotor(m3_In1, m3_In2, m3_En);
  setupMotor(m4_In1, m4_In2, m4_En);

  preferences.begin("car-config", false);
  useGloveMode = preferences.getBool("mode", true);

  WiFi.mode(WIFI_AP_STA); // Both modes active

  WiFi.softAP(ssid, password, WIFI_CHANNEL, 0);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Fail");
  }
  esp_now_register_recv_cb((esp_now_recv_cb_t) OnDataRecv);

  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ------------------------ MAIN LOOP --------------------------------
void loop() {
  handleButton();
  server.handleClient(); // Always run web server
  webSocket.loop(); // Always run websocket

  unsigned long now = millis();

  // Safety Timeout
  if (systemArmed && (now - lastPacketTime > SAFETY_TIMEOUT)) {
    applyMixer(0, 0);
    systemArmed = false;
  }

  // LED ping
  if (isPinging) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);

    isPinging = false;
  } 
  else if (systemArmed) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    // Blink to show standby
    if (now - ledTimer >= 500) {
      ledTimer = now;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

// ------------------------ WEBSOCKET HANDLER ------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  if (type == WStype_TEXT) {
    String msg = (char*)payload;
    
    // PING HANDLER
    if (msg == "P") {
        isPinging = true; // Triggers loop LED logic
        return;
    }

    // Only process Web commands if NOT in Glove Mode
    if (!useGloveMode) {
      int colon = msg.indexOf(':');
      if (colon != -1) {
        int thr = msg.substring(0, colon).toInt();
        int str = msg.substring(colon + 1).toInt();
        lastPacketTime = millis();
        systemArmed = true;
        applyMixer(thr, str);
      }
    }
  } else if (type == WStype_CONNECTED) {
    // Send current mode to new client
    if (useGloveMode) {
      webSocket.sendTXT(num, "GLOVE");
    } else {
      webSocket.sendTXT(num, "WEB");
    }
  }
}