#define TUD_HID_REPORT_SET_SUPPORT 1
#define TUD_HID_REPORT_GET_SUPPORT 1


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include "USB.h"
#include "USBHID.h"

WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;
int socketToPlayer[20];
int globalButtonMap[5] = {0, 4, 3, 2, 1};
uint8_t ledState[7] = {0};

static const uint8_t report_descriptor[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x14, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x14, 0x75, 0x01, 0x81, 0x02, 0x75, 0x01, 0x95, 0x0C, 0x81, 0x03,
    0x06, 0x00, 0xFF, 0x09, 0x01, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x07, 0x91, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x02, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x08, 0x95, 0x01, 0xB1, 0x02,
    0xC0
};

class BuzzDevice : public USBHIDDevice {
public:
    BuzzDevice() {}
    uint16_t _onGetDescriptor(uint8_t* buffer) override {
        memcpy(buffer, report_descriptor, sizeof(report_descriptor));
        return sizeof(report_descriptor);
    }
    void _onOutput(uint8_t report_id, const uint8_t* data, uint16_t len) override {
        if (len >= 1 && len <= 7) {
            memcpy(ledState, data, len);
            for (int i = 0; i < 20; i++) {
                if (socketToPlayer[i] >= 0) webSocket.sendTXT(i, "LED");
            }
        }
    }
    uint16_t _onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t len) override {
        if (len >= 1) { buffer[0] = 0; return 1; }
        return 0;
    }
};

USBHID HID;
BuzzDevice Buzz;
uint8_t buzz_report[6] = {0x7F, 0x7F, 0x00, 0x00, 0xF0, 0x00};

struct PlayerState {
    bool connected = false;
    char deviceId[37] = "";
};
PlayerState players[4];


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no,maximum-scale=1">
<meta name="apple-mobile-web-app-capable" content="yes">
<style>
  body{margin:0;padding:0;background:#050505;color:#fff;font-family:sans-serif;touch-action:none;overflow:hidden;display:flex;justify-content:center;align-items:center;height:100vh}
  .controller{width:280px;height:90vh;max-height:600px;background:#000;border-radius:55px;position:relative;display:flex;flex-direction:column;align-items:center;padding:25px;box-sizing:border-box;border:2px solid #222;box-shadow:inset 0 0 20px #111, 0 0 50px #000}
  .buzzer{width:150px;height:150px;background:radial-gradient(#ff2222, #660000);border-radius:50%;border:6px solid #111;box-shadow:0 8px 0 #300, 0 10px 20px rgba(0,0,0,0.8);cursor:pointer;display:flex;justify-content:center;align-items:center;font-size:1.8rem;font-weight:bold;margin:15px 0 25px 0;transition:transform 0.05s}
  .buzzer.glow{box-shadow:0 0 30px #f00, 0 0 60px #f55, inset 0 0 20px #f00}
  .buttons{width:100%;display:flex;flex-direction:column;gap:12px}
  .btn{width:100%;height:58px;border-radius:15px;cursor:pointer;display:flex;justify-content:center;align-items:center;border:4px solid #111;transition:transform 0.05s}
  .blue{background:#0055ff;box-shadow:inset 0 0 15px #000} .org{background:#ff6600;box-shadow:inset 0 0 15px #000} 
  .grn{background:#00cc00;box-shadow:inset 0 0 15px #000} .ylw{background:#ffcc00;box-shadow:inset 0 0 15px #000}
  .footer{position:absolute;bottom:20px;font-size:0.7rem;color:#0055ff;letter-spacing:1px}
  .fs{position:fixed;top:10px;right:10px;background:#222;color:#fff;border:1px solid #444;padding:8px;border-radius:5px;font-size:0.6rem;z-index:99}
  #status{font-size:0.8rem;color:#555;margin-bottom:5px;letter-spacing:2px}
</style></head><body>
<button class="fs" id="fsBtn">FULLSCREEN</button>
<div class="controller">
  <div id="status">CONNECTING...</div>
  <div class="buzzer" id="b0">BUZZ!</div>
  <div class="buttons">
    <div class="btn blue" id="b1"></div>
    <div class="btn org" id="b2"></div>
    <div class="btn grn" id="b3"></div>
    <div class="btn ylw" id="b4"></div>
  </div>
  <div class="footer">PlayStation 2</div>
</div>
<script>
  let ws;
  const devId = localStorage.getItem('buzz_id') || Math.random().toString(36).substr(2, 9);
  localStorage.setItem('buzz_id', devId);
  function toggleFullscreen() {
    const elem = document.documentElement;
    if (!document.fullscreenElement && !document.webkitFullscreenElement) {
      if (elem.requestFullscreen) elem.requestFullscreen();
      else if (elem.webkitRequestFullscreen) elem.webkitRequestFullscreen();
    } else {
      if (document.exitFullscreen) document.exitFullscreen();
      else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
    }
  }
  function connect(){
    ws = new WebSocket('ws://'+location.hostname+':81/');
    ws.onopen = () => ws.send('JOIN:'+devId);
    ws.onmessage = (e) => {
      if(e.data.startsWith('P:')) document.getElementById('status').innerText = "PLAYER " + (parseInt(e.data.split(':')[1])+1);
      else if(e.data === 'LED') {
        const b = document.getElementById('b0'); b.classList.add('glow');
        setTimeout(() => b.classList.remove('glow'), 300);
      }
    };
    ws.onclose = () => setTimeout(connect, 1000);
  }
  function s(b,t){ if(ws && ws.readyState===1) ws.send(t+':'+b); }
  ['b0','b1','b2','b3','b4'].forEach((id,i)=>{
    const el=document.getElementById(id);
    el.onpointerdown = e=>{e.preventDefault(); s(i,'P')};
    el.onpointerup = e=>{e.preventDefault(); s(i,'R')};
  });
  document.getElementById('fsBtn').onclick = toggleFullscreen;
  connect();
</script></body></html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_DISCONNECTED || type == WStype_CONNECTED) {
        socketToPlayer[num] = -1;
        return;
    }
    if (type != WStype_TEXT) return;
    String msg = String((char*)payload);
    int sep = msg.indexOf(':');
    if (sep < 0) return;
    String cmd = msg.substring(0, sep);
    String arg = msg.substring(sep + 1);

    if (cmd == "JOIN") {
        int slot = -1;
        for (int i = 0; i < 4; i++) {
            if (players[i].connected && strcmp(players[i].deviceId, arg.c_str()) == 0) { slot = i; break; }
        }
        if (slot == -1) {
            for (int i = 0; i < 4; i++) {
                if (!players[i].connected) {
                    slot = i; strncpy(players[i].deviceId, arg.c_str(), 36);
                    players[i].connected = true; break;
                }
            }
        }
        if (slot >= 0) {
            socketToPlayer[num] = slot;
            webSocket.sendTXT(num, "P:" + String(slot));
        }
    } else {
        int pIdx = socketToPlayer[num];
        int localBtn = arg.toInt();
        if (pIdx >= 0 && localBtn >= 0 && localBtn < 5) {
            int logicalBtn = globalButtonMap[localBtn];
            int idx = (pIdx * 5) + logicalBtn;
            int byteIdx = (idx / 8) + 2;
            int bitPos = idx % 8;
            if (cmd == "P") buzz_report[byteIdx] |= (1 << bitPos);
            else if (cmd == "R") buzz_report[byteIdx] &= ~(1 << bitPos);
            buzz_report[4] = (buzz_report[4] & 0x0F) | 0xF0;
            HID.SendReport(0, buzz_report, 6);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    USB.VID(0x054C); USB.PID(0x1000);
    USB.manufacturerName("Namtai"); USB.productName("Buzz");
    USB.serialNumber("");
    HID.addDevice(&Buzz, sizeof(report_descriptor));
    HID.begin();
    USB.begin();
    delay(4000);

    for (int i = 0; i < 20; i++) socketToPlayer[i] = -1;

    preferences.begin("wifi-store", false);
    String sS = preferences.getString("ssid", "");
    String sP = preferences.getString("pass", "");

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Buzz_Emulator_AP", NULL);

    if (sS != "") WiFi.begin(sS.c_str(), sP.c_str());

    server.on("/", [](){ server.send(200, "text/html", index_html); });
    

    server.on("/debug", [](){
        String h = "<html><body style='font-family:sans-serif;padding:20px'><h2>Buzzer Remapper</h2>";
        for(int i=0; i<4; i++) h += "Slot "+String(i+1)+": "+(players[i].connected ? players[i].deviceId : "Free")+"<br>";
        h += "<br><button onclick=\"location.href='/clear'\">Clear All Slots</button><hr>";
        
        h += "<h3>WiFi Connection</h3>";
        h += "Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected to " + WiFi.SSID() : "Disconnected") + "<br>";
        h += "IP: " + WiFi.localIP().toString() + "<br><br>";
        h += "<form action='/connect' method='POST'>Join Home WiFi:<br>SSID: <input name='s'><br>Pass: <input name='p' type='password'><br><input type='submit' value='Save'></form><hr>";

        h += "<form action='/setmap'><h3>Button Mapping</h3>";
        h += "Red: <input name='m0' value='"+String(globalButtonMap[0])+"'><br>";
        h += "Blue: <input name='m1' value='"+String(globalButtonMap[1])+"'><br>";
        h += "Orange: <input name='m2' value='"+String(globalButtonMap[2])+"'><br>";
        h += "Green: <input name='m3' value='"+String(globalButtonMap[3])+"'><br>";
        h += "Yellow: <input name='m4' value='"+String(globalButtonMap[4])+"'><br>";
        h += "<input type='submit' value='Update Map'></form>";
        h += "<br><br><a href='/reset' style='color:red'>Factory Reset WiFi</a></body></html>";
        server.send(200, "text/html", h);
    });

    server.on("/clear", [](){
        for(int i=0; i<4; i++){ players[i].connected = false; players[i].deviceId[0] = '\0'; }
        server.send(200, "text/plain", "Slots Cleared.");
    });

    server.on("/setmap", [](){
        for(int i=0; i<5; i++) globalButtonMap[i] = server.arg("m"+String(i)).toInt();
        server.send(200, "text/plain", "Map Updated.");
    });

    server.on("/connect", HTTP_POST, [](){
        preferences.putString("ssid", server.arg("s"));
        preferences.putString("pass", server.arg("p"));
        WiFi.begin(server.arg("s").c_str(), server.arg("p").c_str());
        server.send(200, "text/plain", "WiFi Credentials Saved.");
    });

    server.on("/reset", [](){ preferences.clear(); ESP.restart(); });

    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
    webSocket.loop();
    server.handleClient();
    yield();
}