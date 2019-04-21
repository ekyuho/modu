// https://github.com/sandeepmistry/arduino-LoRa
#include <U8x8lib.h>
#include <LoRa.h>
#include <Arduino.h>
#include <Ticker.h>
Ticker timer;

// WIFI_LoRa_32 ports
// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// GPIO26 -- SX1278's IRQ(Interrupt Request)

#define SS      18
#define RST     14
#define DI0     26
#define BAND    433E6
#define LED     25

#define OPENING 36
#define CLOSING 37
#define PILOT 38

// the OLED used
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
#define U8LOG_WIDTH 16
#define U8LOG_HEIGHT 8
uint8_t u8log_buffer[U8LOG_WIDTH*U8LOG_HEIGHT];
U8X8LOG u8x8log;

char dbuf[U8LOG_HEIGHT][80] = { 0, 0, 0, 0, 0, 0, 0 };
byte myaddr = 0xAA;
unsigned msgCount = 0;
char buf[80];
String payLoad;
bool got_send = false;
int recipient;
byte sender;
bool got_received = false;
byte _to, _from;
String _payload;
bool onjob = false;


void sendMessage(byte to, byte from, String msg) {
  byte c[20];
  int packetlen;
  c[0] = to;
  c[1] = from;
  c[2] = msgCount;
  c[3] = msg.length();
  packetlen = msg.length() + 5;

  for (int i=0; i<msg.length(); i++) c[5+i] = msg.c_str()[i];
  byte chksum = 0;
  for (int i=0; i< packetlen; i++)
      if (i != 4) chksum += c[i];
  c[4] = chksum;
 
  LoRa.beginPacket();                   // start packet
  for (int i=0; i<packetlen; i++) LoRa.write(c[i]);
  LoRa.endPacket(); 
 
  //Serial.printf("Sent: %s, chksum=%d", msg.c_str(), chksum);
  //Serial.println();
  //for (int i=0; i<packetlen; i++) Serial.printf(" %3X", c[i]);
  //Serial.println();
  //for (int i=0; i<packetlen; i++) Serial.printf(" %3d", c[i]);
  //Serial.println();  
  
  msgCount++;                           // increment message ID
  LoRa.receive();
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  byte to = LoRa.read();          // recipient address
  byte from = LoRa.read();            // sender address
  byte seq = LoRa.read();     // incoming msg ID
  byte length = LoRa.read();
  byte chksum = LoRa.read();    // incoming msg length 
  String payload = "";

  byte csum = to + from + seq + length;
  while (LoRa.available()) {
    byte c = LoRa.read();
    payload += (char)c;
    csum += c;
  }
 
  if (to != myaddr) {
    LoRa.receive();
    return;
  }

  Serial.printf("\n%X<-%X (%dB) %s\n", to, from, payload.length(), payload.c_str()); 
  if (length - payload.length()) {
    Serial.printf("lth err: %d != %d\n", length, payload.length());
    LoRa.receive();
    return;
  }
  if (chksum != csum) {
    Serial.printf("chksum err: %d != %d\n", chksum, csum);
    LoRa.receive();
    return;
  }

  if (onjob) {
    Serial.println("Got new job while busy. skip");
  } else {
    //sprintf(buf, "%02X\n", rheader.from);
    //u8x8log.print(buf);
    _to = from;
    _from = to;
    _payload = payload;
    got_send = true;
    onjob = true;
  }
  LoRa.receive();
}

void clear_screen() {
    u8x8.clearDisplay();  
    u8x8.drawString(0, 1, "Modu Gate Ready");
}

void setup() {

  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);

  Serial.begin(115200);
  pinMode(LED,OUTPUT);

  u8x8.begin();
  u8x8.clearDisplay();  
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  clear_screen();

  Serial.printf("LoRa Receiver");


  if (!LoRa.begin(BAND)) {
    Serial.printf("Starting LoRa failed!");
    u8x8.drawString(0, 1, "Starting LoRa failed!");
    while (1);
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.printf("LoRa init succeeded."); 
}

void led(String cmd) {
  if (cmd == "on") digitalWrite(LED, HIGH);
  if (cmd == "off") digitalWrite(LED, LOW);
}

void gate_closed() {
  sendMessage(_to, _from, "gate closed");
  u8x8.drawString(0, 6, "gate closed"); 
  Serial.println("gate closed");
  led("off");
  timer.once(2, clear_screen);
  onjob = false;
}

void gate_opened() {
  sendMessage(_to, _from, "gate opened");
  u8x8.drawString(0, 5, "gate opened"); 
  Serial.println("gate opened");
  timer.once(5, gate_closed);
}

void gate_open() {
  sendMessage(_to, _from, "opening gate");
  u8x8.drawString(0, 4, "opening gate"); 
  Serial.println("opening gate");
  timer.once(4, gate_opened);
  led("on");
}

unsigned cnt = 0;

void loop() {
  if (got_send) {
    got_send = false;
    u8x8.clearDisplay();  
    u8x8.drawString(0, 2, "Got Job ");
    u8x8.drawString(0, 3, _payload.c_str());
    Serial.println("Got Job: "+ _payload);
    gate_open();           
  }
}
