#include <Adafruit_NeoPixel.h>
#include "src/RubberNugget.h"
#include "Arduino.h"
#include <base64.h>
#include "base64.hpp"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>

#include "webUI/index.h"
#include "SH1106Wire.h"
#include "Nugget_Interface.h"

// for file streaming
#include "cdcusb.h"
#include "mscusb.h"
#include "flashdisk.h"

#include "src/utils.h"
#include "src/interface/screens/dir.h"
#include "src/interface/lib/NuggetInterface.h"

extern SH1106Wire display;
extern String rubberCss;
extern String rubberHtml;
extern String rubberJs;
extern String createHtml;

extern String payloadPath;
bool webstuffhappening = false;

extern Nugget_Interface payloadSelector;

Adafruit_NeoPixel strip(1, 12, NEO_RGB + NEO_KHZ800);

const char *ssid = "Nugget AP";
const char *password = "nugget123";

WebServer server(80);

TaskHandle_t webapp;
TaskHandle_t nuggweb;

void getPayloads() {
  String* payloadPaths = RubberNugget::allPayloadPaths();
  Serial.printf("[SERVER][getPayloads] %s\n", payloadPaths->c_str());
  server.send(200, "text/plain", *payloadPaths);
}

void handleRoot() {
  Serial.println("handling root!");
  server.send(200, "text/html", String(INDEX));
}

void delpayload() {
  String path(server.arg("path"));
  FRESULT res = f_unlink(path.c_str());
  if (res == FR_OK){
    server.send(200);
  } else {
    server.send(500);
  }
}

void websave() {
  fileOp op = saveFileBase64(server.arg("path"), server.arg("payloadText"));
  if (!op.ok) {
    server.send(500, "text/plain", op.err_msg);
    return;
  }
  server.send(200, "text/plain", "payload created successfully");
}

void webget() {
  FRESULT fr;
  FIL file;
  uint16_t size;
  UINT bytesRead;

  String path = server.arg("path");
  fr = f_open(&file, path.c_str(), FA_READ);

  if (fr != FR_OK) {
    // TODO: most likely file not found, but we need to check why fr != OK.
    // Marking 500 until resolved
    server.send(500, "plain/text", String("Error loading script"));
    return;
  }

  size = f_size(&file);
  char * data = NULL;

  data = (char*) malloc(size);

  fr = f_read(&file, data, (UINT) size, &bytesRead);
  if (fr == FR_OK) {
    String payload = String(data);
    payload = payload.substring(0, bytesRead);
    payload = base64::encode(payload);
    server.send(200, "plain/text", payload);
  } else {
    server.send(500, "plain/text", String("Error reading script"));
  }
  f_close(&file);

}

NuggetInterface* nuggetInterface;

// run payload with get request path
void webrun() {
  server.send(200, "text/html", "Running payload...");
  String path = server.arg("path");
  NuggetScreen* runner = new ScriptRunnerScreen(path);
  bool ok = nuggetInterface->injectScreen(runner);
  if (!ok) {
    // TODO: send 503 when device is busy
    //server.send(503, "text/html", "Device busy");
    return;
  }
}

void webrunlive() {
  // TODO: use server.arg "content" or "payload" instead of "plain"
  String path("/.tmp_webrun");
  fileOp op = saveFileBase64(path, server.arg("plain"));
  if (!op.ok) {
    server.send(500, "text/plain", op.err_msg);
    return;
  }
  server.send(200, "text/plain", "running live payload");
  NuggetScreen* runner = new ScriptRunnerScreen(path);
  bool ok = nuggetInterface->injectScreen(runner);
  if (!ok) {
    // TODO: send 503 when device is busy
    //server.send(503, "text/html", "Device busy");
    return;
  }
}

void webserverInit(void *p) {
  while (1) {
    server.handleClient();
    vTaskDelay(2);
  }
}

extern String netPassword;
extern String networkName;

void setup() {
  pinMode(12, OUTPUT); 
  strip.begin(); 
  delay(500);

  Serial.begin(115200);

  RubberNugget::init();
 
  if (networkName.length() >0) {
    Serial.println(networkName);
    const char * c = networkName.c_str();
    ssid=c;
  }
  if (netPassword.length() >=8) {
    Serial.println(netPassword);
    const char * d = netPassword.c_str();
    password=d;
  }

  WiFi.softAP(ssid, password);
  // } 
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/payloads", getPayloads);
  server.on("/savepayload", HTTP_POST, websave);
  server.on("/deletepayload", HTTP_POST, delpayload);
  server.on("/runlive", HTTP_POST, webrunlive);
  server.on("/getpayload", HTTP_GET, webget);
  server.on("/runpayload", HTTP_GET, webrun);

  server.begin();

  strip.clear(); 
  strip.setPixelColor(0, strip.Color(0, 0, 0)); 
  strip.show();
  strip.show();

  xTaskCreate(webserverInit, "webapptask", 12 * 1024, NULL, 5, &webapp); // create task priority 1
  nuggetInterface = new NuggetInterface;
  NuggetScreen* dirScreen = new DirScreen("/");
  nuggetInterface->pushScreen(dirScreen);
  nuggetInterface->start();

}

void loop() { return; }
