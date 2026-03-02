// WIT32 MJPEG proxy server (full example)
// - Put this on WIT32 (ESP32).
// - It will run AP+STA, act as web server and provide /cam endpoint
// - /cam will proxy the MJPEG stream from the ESP32-CAM.
//
// IMPORTANT: adjust sta_ssid/sta_pass and camIP/camPort/camPath to your setup.

#include <WiFi.h>
#include <WebServer.h>

const char* sta_ssid = "Kong_Wifi";       // your home WiFi (for internet)
const char* sta_pass = "-";

const char* ap_ssid = "FIRE_ROBOT";      // local AP for robot control
const char* ap_pass = "-";

WebServer server(80);

// CAM connection info (change as needed)
// If your cam stream is at http://192.168.1.200/  => camPort = 80, camPath = "/"
// If your cam stream is at http://192.168.1.200:81/stream => camPort = 81, camPath = "/stream"
const char* camIP = "192.168.1.200";
const uint16_t camPort = 80;
const char* camPath = "/";    // path on camera that serves multipart MJPEG

// HTML page served on root: embed the proxied stream
String pageHTML() {
  String s = R"rawliteral(
  <!doctype html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
      body{background:#111;color:#fff;font-family:Arial;text-align:center}
      img{width:95%;max-width:720px;border-radius:8px;margin-top:10px}
      button{padding:12px 18px;margin:8px;font-size:16px;border-radius:8px;border:none}
    </style>
  </head>
  <body>
    <h2>🔥 Fire Robot Control</h2>
    <p>Camera (proxied):</p>
    <img id="cam" src="/cam" />
    <div style="margin-top:12px">
      <button onclick="fetch('/pump/on')">PUMP ON</button>
      <button onclick="fetch('/pump/off')">PUMP OFF</button>
    </div>
  </body>
  </html>
  )rawliteral";
  return s;
}

// helper: read a header line from WiFiClient (terminated by \r\n)
String readHeaderLine(WiFiClient &client, uint32_t timeout = 2000) {
  String line;
  uint32_t t0 = millis();
  while (true) {
    while (client.available()) {
      char c = client.read();
      line += c;
      int L = line.length();
      if (L >= 2 && line[L-2] == '\r' && line[L-1] == '\n') {
        // return without the trailing \r\n
        line.remove(L-2, 2);
        return line;
      }
    }
    if (millis() - t0 > timeout) return String(""); // timeout
    yield();
  }
}

// Proxy MJPEG stream: open connection to cam and forward bytes to client
void handleCamProxy() {
  WiFiClient client = server.client(); // client connected to WIT32
  // Prevent multiple simultaneous clients
  static bool busy = false;
  if (busy) {
    // return simple error
    server.send(503, "text/plain", "Busy");
    return;
  }
  busy = true;

  WiFiClient cam;
  if (!cam.connect(camIP, camPort)) {
    server.send(502, "text/plain", "Cannot connect to CAM");
    busy = false;
    return;
  }

  // send request to camera
  String req = String("GET ") + camPath + " HTTP/1.1\r\nHost: " + camIP + "\r\nConnection: close\r\n\r\n";
  cam.print(req);

  // read camera response status line
  String status = readHeaderLine(cam);
  if (status.length() == 0) {
    server.send(502, "text/plain", "No response from CAM");
    cam.stop();
    busy = false;
    return;
  }

  // read and forward headers until blank line; find Content-Type
  String contentType = "";
  while (true) {
    String h = readHeaderLine(cam);
    if (h.length() == 0) break; // end headers
    // parse header for Content-Type
    if (h.startsWith("Content-Type:") || h.startsWith("content-type:")) {
      int p = h.indexOf(':');
      if (p >= 0) {
        contentType = h.substring(p+1);
        contentType.trim();
      }
    }
  }

  if (contentType == "") {
    // fallback: assume MJPEG multipart
    contentType = "multipart/x-mixed-replace;boundary=frame";
  }

  // Send our own HTTP response header to the browser/client
  // We write raw HTTP header to the connected client socket (bypass WebServer send)
  client.print(String("HTTP/1.1 200 OK\r\n"));
  client.print(String("Content-Type: ") + contentType + "\r\n");
  client.print(String("Cache-Control: no-cache\r\n"));
  client.print(String("Connection: close\r\n\r\n"));
  client.setNoDelay(true);

  // Now relay data from cam -> client until either disconnects
  const size_t BUF_SZ = 1024;
  uint8_t buf[BUF_SZ];

  // Relay loop
  while (cam.connected() && client.connected()) {
    // if cam has data, read and forward
    while (cam.available()) {
      int r = cam.read(buf, BUF_SZ);
      if (r > 0) {
        int w = client.write(buf, r);
        // If write fails or client disconnected, break
        if (w <= 0) {
          cam.stop();
          client.stop();
          break;
        }
      }
    }
    // if no data, small sleep to avoid hogging CPU
    if (!cam.available()) {
      delay(1);
    }
    // If remote client closed, break
    if (!client.connected()) break;
    yield();
  }

  // close both connections
  cam.stop();
  client.stop();

  busy = false;
}

// Simple handlers for root and pump (demo)
void handleRoot() {
  server.send(200, "text/html", pageHTML());
}
void handlePumpOn() {
  Serial.println("PUMP ON (web)");
  server.send(200, "text/plain", "OK");
}
void handlePumpOff() {
  Serial.println("PUMP OFF (web)");
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // AP + STA mode
  WiFi.mode(WIFI_AP_STA);

  // Start AP for local control
  WiFi.softAP(ap_ssid, ap_pass);
  Serial.print("AP started: ");
  Serial.println(ap_ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP()); // usually 192.168.4.1

  // Connect to home WiFi for internet (optional)
  WiFi.begin(sta_ssid, sta_pass);
  Serial.print("Connecting to home WiFi");
  unsigned long tstart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - tstart < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to home WiFi");
    Serial.print("Home IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nCould not connect to home WiFi (continuing with AP only)");
  }

  // Routes
  server.on("/", handleRoot);
  server.on("/cam", HTTP_GET, [](){ handleCamProxy(); });
  server.on("/pump/on", HTTP_GET, handlePumpOn);
  server.on("/pump/off", HTTP_GET, handlePumpOff);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  yield();
}
