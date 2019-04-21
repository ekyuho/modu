#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
const char* ssid = "cookie";
const char* password = "0317137263";
WebServer server(80);

void handleRoot() {
  got_cmd = true;
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "cmd") {
      cmd = server.arg(i);
      break;
    }
  }
  server.send(200, "text/plain", cmd);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void announce(void) {
  WiFiClient client;
  HTTPClient http;
  Serial.println("Announce my address");
  String ip = WiFi.localIP().toString();
  if (http.begin(client, "http://t.damoa.io/parking?a=" + ip)) {
    int code = http.GET();
    if (code == HTTP_CODE_OK) Serial.println(http.getString());
    else Serial.println(http.errorToString(code));
    http.end();
  } else
    Serial.println("failed to connect");
}

void net_init(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  announce();

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void net_loop(void) {
  server.handleClient();
}
