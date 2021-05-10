#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>
#include <rdm6300.h>
#include "RTClib.h"

#define RDM6300_RX_PIN 9
#define READ_LED_PIN 13
#define CLK 3
#define DIO 2
#define CLK2  4
#define DIO2  5
#define IR  8
#define BUZZER    A0
#define PIN_HOLD  A1
#define PIN_SETUP A2
#define PIN_STOP  A3
#define LED_HOLD  10
#define LED_SETUP 11
#define LED_STOP  12

Rdm6300 rdm6300;
LiquidCrystal_I2C lcd(0x27, 20, 4);
TM1637Display display(CLK, DIO);
TM1637Display display2(CLK2, DIO2);
RTC_DS3231 rtc;

struct Tag{
  String card;
  String prevCard;
  String nama;
  bool tapState = false;
  bool tap = false;
}tag;

struct Time{
  byte millisecond;
  byte prevSecond;
  unsigned long RFdelay;
  unsigned long buzzTime;
}waktu;

struct Barang{
  unsigned int total = 0;
  unsigned int terhitung = 0;
  unsigned long prevStamp;
  unsigned long currentStamp;
  unsigned int cycleTime = 0;
  bool adjustState = false;
  bool resetState = false;
}barang;

struct Comm{
  String data[2];
  String cmdToEsp;
  byte jumlahData = 0;
  byte action = 4;
  bool sendActionState = false;
  bool sendActionState2 = false;
}comm;

struct States{
  bool holdState = false;
  bool setupState = false;
  bool stopState = false;
  bool prevState[3] = {false, false, false};
  bool buzzState = false;
  const byte BUTTON[3] = {PIN_HOLD, PIN_SETUP, PIN_STOP};
  const byte LED[3] = {LED_HOLD, LED_SETUP, LED_STOP};
}bs;

uint8_t blank[] = { 0x00, 0x00, 0x00, 0x00 };

const uint8_t SEG_DONE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
	};

bool countState = false;
bool countPrevState = false;
bool stateCounter = false;
bool first = true;

void initialize();
void waitForWiFi();
void readUART();
void parsing(String data);
void showMenu(byte jam, byte menit, byte detik);
unsigned int calculateCycle();
unsigned int calculateMillisRTC(byte detik);

bool readRFID();
void(* resetFunc) (void) = 0;

void setup(){
	Serial.begin(115200);
  rdm6300.begin(RDM6300_RX_PIN);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.init();
  lcd.backlight();
  display.setBrightness(0x0f);
  display2.setBrightness(0x0f);
  display2.showNumberDec(barang.total, false);
  display.showNumberDec(barang.terhitung, false);

	pinMode(READ_LED_PIN, OUTPUT);
  pinMode(IR, INPUT);
  pinMode(bs.BUTTON[0], INPUT_PULLUP);
  pinMode(bs.BUTTON[1], INPUT_PULLUP);
  pinMode(bs.BUTTON[2], INPUT_PULLUP);
  pinMode(bs.LED[0], INPUT);
  pinMode(bs.LED[1], INPUT);
  pinMode(bs.LED[2], INPUT);
  pinMode(BUZZER, OUTPUT);
	digitalWrite(READ_LED_PIN, LOW);
  waitForWiFi();
  tag.tap = false;
}

void loop(){
  DateTime now = rtc.now();
  String data;
  if(millis() - waktu.RFdelay > 6000){
    tag.tapState = true;
  }else{
    tag.tapState = false;
  }
  if(readRFID()){
    if(tag.tapState){
      waktu.buzzTime = millis();
      bs.buzzState = true;
      comm.cmdToEsp = "TAG," + tag.card;
      Serial.write(comm.cmdToEsp.c_str());
    }
  }
  if(bs.buzzState){
    if(millis() - waktu.buzzTime < 100){
      digitalWrite(BUZZER, HIGH);
    }else{
      bs.buzzState = false;
      digitalWrite(BUZZER, LOW);
    }
  }
  if(first){
    tag.tap = false;
    first = false;
  }
  //  read incoming serial data
  readUART();
  //  read proximity
  if(digitalRead(IR) == LOW){
    countState = true;
  }else if(digitalRead(IR) == HIGH){
    countState = false;
  }

  for(byte i = 0; i < 3; i++){
    if(digitalRead(bs.BUTTON[i]) == LOW){
      // pinMode(bs.LED[i], OUTPUT);
      // digitalWrite(bs.LED[i], LOW);
      // bs.prevState[i] = true;
      // comm.sendActionState2 = true;
      if(!tag.tap){
        if(i == 0){
          if(!bs.setupState && !bs.stopState){
            pinMode(bs.LED[i], OUTPUT);
            digitalWrite(bs.LED[i], LOW);
            bs.prevState[i] = true;
            comm.sendActionState2 = true;

            bs.holdState = true;
            stateCounter = false;
            comm.action = 2;
          }
        }else if(i == 1){
          if(!bs.holdState && !bs.stopState){
            pinMode(bs.LED[i], OUTPUT);
            digitalWrite(bs.LED[i], LOW);
            bs.prevState[i] = true;
            comm.sendActionState2 = true;

            bs.setupState = true;
            stateCounter = false;
            comm.action = 3;
          }
        }else if(i == 2){
          if(!bs.holdState && !bs.setupState){
            pinMode(bs.LED[i], OUTPUT);
            digitalWrite(bs.LED[i], LOW);
            bs.prevState[i] = true;
            comm.sendActionState2 = true;
            
            bs.stopState = true;
            stateCounter = false;
            comm.action = 4;
          }
        }
      }
    }else{
      pinMode(bs.LED[i], INPUT);
      if(i == 0){
        bs.holdState = false;
        comm.action = 2;
      }else if(i == 1){
        bs.setupState = false;
        comm.action = 3;
      }else if(i == 2){
        bs.stopState = false;
        comm.action = 4;
      }
      if(!bs.holdState && !bs.setupState && !bs.stopState){
        if(bs.prevState[i]){
          comm.sendActionState2 = true;
          bs.prevState[i] = false;
        }else{
          comm.sendActionState2 = false;
        }
        stateCounter = true;
        comm.sendActionState = true;
      }
    }
    if(comm.sendActionState){
      if(comm.sendActionState2){
        comm.cmdToEsp = "jobsend," + (String)barang.terhitung + "," + (String)comm.action;
        Serial.write(comm.cmdToEsp.c_str());
        comm.sendActionState = false;
      }
    }
  }

  //  if status complete
  if(tag.tap){
    //  reset cycle to 0
    barang.cycleTime = 0;
  }else{
    barang.cycleTime = calculateCycle();
    if(barang.terhitung == 0){
      barang.cycleTime = 0;
    }
  }
  // waktu.millisecond = calculateMillisRTC(now.second());
  if(tag.tap){
    if(!barang.resetState){
      tag.tap = true;
      barang.resetState = true;
      display.setBrightness(7, false);
      display2.setBrightness(7, false);
      display.setSegments(blank);
      display2.setSegments(blank);
    }
  }else{
    display.setBrightness(0x0f, true);
    display2.setBrightness(0x0f, true);
    display2.showNumberDec(barang.total, false);
    display.showNumberDec(barang.terhitung, false);
    barang.resetState = false;
  }
  //  RFID interrupt handler
  if(tag.tap){
    if(barang.adjustState){
      barang.total = barang.total - barang.terhitung;
      barang.terhitung = 0;
      barang.adjustState = false;
    }else{
      barang.adjustState = true;
    }
  }else{
    if(barang.total == barang.terhitung){
      barang.total = 20;
      barang.terhitung = 0;
    }
  }
  //  display to LCD
  showMenu(now.hour(), now.minute(), now.second());
}

bool readRFID(){
  if (rdm6300.update()){
    waktu.RFdelay = millis();
    tag.card = String(rdm6300.get_tag_id(), HEX);
    if(tag.card != tag.prevCard){
      tag.prevCard = tag.card;
      tag.tap = false;
    }else{
      tag.tap = !tag.tap; 
    }
    return true;
  }else{
    return false;
  }
}

void waitForWiFi(){
  unsigned long waitRFID;
  String data;
  lcd.setCursor(6,1);
  lcd.print("Tap RFID");
  while(!readRFID()){
    while(Serial.available()){
      char s = Serial.read();
      data += s;
    }
    if(data != ""){
      if(data == "AP_CONFIG"){
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Please Configure");
        lcd.setCursor(8,1);
        lcd.print("WiFi");
        lcd.setCursor(4,2);
        lcd.print("192.168.4.1");
        data = "";
      }else if(data == "CONFIG_DONE"){
        lcd.clear();
        lcd.setCursor(6,1);
        lcd.print("Tap RFID");
        data = "";
      }
    }else{
      
    }
  }
  while(readRFID()){

  }
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  lcd.clear();

  lcd.setCursor(3,1);
  lcd.print("Checking RFID");
  lcd.setCursor(8,2);
  lcd.print("....");
  // data = tag.card + ",GET_IP";
  data = "GET_IP," + tag.card;
  Serial.write(data.c_str());
  waitRFID = millis();
  while(!Serial.available()){
    if(millis() - waitRFID > 10000){
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("RFID Not Recognized");
      lcd.setCursor(2,2);
      lcd.print("Restart Counter");
      while(true){

      }
    }
    delay(10);
  }
  data = "";
  while(Serial.available()){
    char s = Serial.read();
    data += s;
  }
  if(data != ""){
    stateCounter = true;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("IP: ");
    lcd.print(data);
    delay(3000);
    lcd.clear();
  }
}

void showMenu(byte jam, byte menit, byte detik){
  String menu;
  lcd.setCursor(0,0);
  lcd.print("Time  : ");
  if(jam < 10){
    lcd.print("0");
  }
  lcd.print(jam);
  lcd.print(":");
  if(menit < 10){
    lcd.print("0");
  }
  lcd.print(menit);
  lcd.print(":");
  if(detik < 10){
    lcd.print("0");
  }
  lcd.print(detik);
  lcd.setCursor(0,1);
  lcd.print("Name  : ");
  lcd.print(tag.nama);
  lcd.setCursor(0,2);
  lcd.print("Stat  : ");
  if(stateCounter){
    if(tag.tap){
      lcd.print("Complete");
      }else{
      lcd.print("Run     ");
    }
  }else{
    if(bs.holdState && !bs.setupState && !bs.stopState){
      lcd.print("Hold");
    }else if(!bs.holdState && bs.setupState && !bs.stopState){
      lcd.print("Setup");
    }else if(!bs.holdState && !bs.setupState && bs.stopState){
      lcd.print("Stop");
    }
  }
  lcd.setCursor(0,3);
  menu = (String)barang.cycleTime + " ms";
  lcd.print(menu);
  for(byte i = menu.length(); i < 20; i++){
    lcd.print(" ");
  }
}

void readUART(){
  String data;
  String data2;
  while(Serial.available()){
    char s = Serial.read();
    data += s;
  }
  if(data != ""){
    parsing(data);
    if(comm.jumlahData == 2){
      barang.total = comm.data[0].toInt();
      tag.nama = comm.data[1];
    }
  }
  if(data == "success"){
    if(tag.tap){
      digitalWrite(READ_LED_PIN, HIGH);
    }else{
      digitalWrite(READ_LED_PIN, LOW);
    }
  }else if(data == "AP_CONFIG"){
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Please Configure");
    lcd.setCursor(8,1);
    lcd.print("WiFi");
    lcd.setCursor(4,2);
    lcd.print("192.168.4.1");
    while(true){
      while(Serial.available()){
        char s = Serial.read();
        data2 += s;
      }
      if(data2 != ""){
        if(data2 == "CONFIG_DONE"){
          break;
        }
      }
    }
  }else if(data == "SERVER_CONFIG"){
    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("Connecting to");
    lcd.setCursor(7,1);
    lcd.print("Server");
    while(true){
      while(Serial.available()){
        char s = Serial.read();
        data2 += s;
      }
      if(data2 != ""){
        if(data2 == "CONFIG_DONE"){
          break;
        }
      }
    }
  }
}

unsigned int calculateCycle(){
  unsigned int hasil;
  if(!tag.tap){
    if(countState != countPrevState){
      countPrevState = countState;    
      if(countState){
        if(stateCounter){
          barang.terhitung++;
          comm.action = 1;
          comm.cmdToEsp = "jobsend," + (String)barang.terhitung + "," + (String)comm.action;
          Serial.write(comm.cmdToEsp.c_str());
          if(barang.terhitung != 1){
            barang.prevStamp = barang.currentStamp;
            barang.currentStamp = millis();
            hasil = barang.currentStamp - barang.prevStamp;
          }else{
            barang.currentStamp = millis();
            return 0;
          }
        }
      }
    }
  }
  return hasil;
}

unsigned int calculateMillisRTC(byte detik){
  unsigned long prev;
  unsigned long current;
  unsigned long hasil;
  if(detik != waktu.prevSecond){
    waktu.prevSecond = detik;
  }else if(detik == waktu.prevSecond){
    prev = current;
    current = millis();
    hasil = current - prev;
  }
  return hasil;
}

void parsing(String dataString){
  byte i = 0, dataNow = 0;
  comm.jumlahData = 1;
  comm.data[dataNow] = "";
  for(i = 0; i < dataString.length(); i++){
    if(dataString[i] == ','){
      comm.jumlahData++;
      dataNow++;
      comm.data[dataNow] = "";
    }else{
      comm.data[dataNow] += dataString[i];
    }
  }
}