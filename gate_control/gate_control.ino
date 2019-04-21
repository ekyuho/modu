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
#define PABOOST true
#define V2   1
#ifdef V2 //WIFI Kit series V1 not support Vext control
  #define Vext  21
#endif

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
byte myaddr = 0xA;
unsigned chksum_errors = 0;
String dbuf[7] = {"", "", "", "", "", "", "" };
String wait_gate = "";
byte wate_from;
String cmd = "";
bool got_cmd = false;
byte seq = 0;
String Sent;
unsigned long marker = 0, ack_marker = 0;
bool ack = true;
int send_retry = 0;

void clear_screen() {
  u8x8.clearDisplay();  
  u8x8.drawString(0, 1, "Modu ready");   
}

void send_cmd(String cmd) {
  if (!ack) {
    Serial.println("Previous command got no ack");
    ack = true;
  }
  ack = false;
  byte c[20], ci=0;
  LoRa.beginPacket();                   // start packet
  LoRa.write(0xAA);              // add destination address
  LoRa.write(myaddr);             // add sender address
  LoRa.write(seq);
  LoRa.write(cmd.length()&0xff);
  byte chksum = 0xAA + myaddr + seq + cmd.length()&0xff;
  c[ci++] = 0xAA;
  c[ci++] = myaddr;
  c[ci++] = seq;
  c[ci++] = cmd.length();
  ci++;
  for (int i=0; i<cmd.length(); i++) {
    chksum += cmd.c_str()[i];
    c[ci++] = cmd.c_str()[i];
  }
  c[4] = chksum;
  LoRa.write(chksum);
  LoRa.print(cmd);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  LoRa.receive();  
  Serial.printf("Sent: %s, chksum=%d", cmd.c_str(), chksum);
  Serial.println();
  //for (int i=0; i<ci; i++) Serial.printf(" %3X", c[i]);
  //Serial.println();
  //for (int i=0; i<ci; i++) Serial.printf(" %3d", c[i]);
  //Serial.println();  
  seq++;
  Sent = cmd;
  cmd = "";
}

void onReceive(int packetSize) {
  byte c[20], ci=0;  
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  byte to = LoRa.read();          // recipient address
  byte from = LoRa.read();            // sender address
  byte seq = LoRa.read();
  byte lth = LoRa.read();
  byte chksum = LoRa.read();    // incoming msg length  
  String payload = "";

  c[ci++] = to;
  c[ci++] = from;
  c[ci++] = seq;
  c[ci++] = lth;
  c[ci++] = chksum;
  byte csum = to + from + seq + lth;
  while (LoRa.available()) {
    byte x = LoRa.read();
    payload += (char)x;
    csum += x;
    c[ci++] = x;
  }

  Serial.printf("%X<-%X %d %s", to, from, payload.length(), payload.c_str());  
  Serial.println();
  if (to != myaddr) {
    LoRa.receive();
    return;
  }


  if (chksum != csum) {
    Serial.printf("chksum err: %d<%d %d %d != %d\n", to, from, seq, chksum, csum);
    Serial.println();
    for (int i=0; i<ci; i++) Serial.printf(" %3X", c[i]);
    Serial.println();
    for (int i=0; i<ci; i++) Serial.printf(" %3d", c[i]);
    Serial.println();      
    chksum_errors++;
    dbuf[0] = "chksum error "+ String(chksum_errors);
    LoRa.receive();
    return;
  }

  u8x8.drawString(0, 5, ("got "+ payload).c_str());
  Serial.println("Got: "+ payload);
  if (payload == "opening gate") {
    ack = true;
    Serial.println("set ack = true");
    u8x8.drawString(0, 4, "Got ack");
  }
  if (payload == "gate closed") {
    timer.once(3, clear_screen);
  }

  LoRa.receive();
}

void setup() {
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);

  Serial.begin(115200);
  pinMode(LED,OUTPUT);

  u8x8.begin(); 
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  clear_screen();

  if (!LoRa.begin(BAND)) {
    Serial.printf("Starting LoRa failed!");
    u8x8.drawString(0, 1, "Starting LoRa failed!");
    while (1);
  }
  LoRa.onReceive(onReceive);
  Serial.printf("LoRa init succeeded.\n"); 
  LoRa.receive();
  marker = millis() + 3000;
  net_init();
}

unsigned cnt = 0;

void loop() {
  static unsigned i = 1;
  if (millis() > marker) {
    marker = millis() + 3000;
    //got_cmd = true;
    //cmd = "open "+ String(i++);
  }
  if (ack_marker && millis() > ack_marker) {
    Serial.println("1 sec");
    if (ack) {
      Serial.println("got ack clear all");
      ack_marker = 0;
      cmd = "";
      send_retry = 0;
      return;
    }

    if (send_retry-- > 0) {
      Serial.printf("resending %d...  %s\n", send_retry, cmd.c_str());
      send_cmd(cmd);
      ack_marker = millis() + 1000;
    } else {
      Serial.printf("abort sending after retrying 5 times");
      ack_marker = 0;
      cmd = "";
    }
  }
  while (Serial.available() > 0) {
     char c = Serial.read();
     //Serial.printf(" %x ", c);
     if (' ' <= c && c <= '~') cmd += String(c);
     if (c == '\n') {
      got_cmd = true;
     }
     yield();
  }
  if (got_cmd) {
    Serial.printf("\nNew cmd=%s, len=%d\n", cmd.c_str(), cmd.length());
    u8x8.drawString(0, 2, "New Job");
    ack_marker = millis() + 1000;
    send_retry = 8;
    send_cmd(cmd); 
    u8x8.drawString(0, 3, cmd.c_str());
    got_cmd = false;
  }
  net_loop();
}
