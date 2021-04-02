#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>
#include <rdm6300.h>
#include "RTClib.h"
#include <EEPROM.h>

#define RDM6300_RX_PIN 9
#define READ_LED_PIN 13
#define CLK 3
#define DIO 2
#define CLK2  4
#define DIO2  5
#define IR  8

Rdm6300 rdm6300;
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
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
}comm;

struct States{
  bool holdState = false;
  bool setupState = false;
  bool stopState = false;
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

  lcd.begin(20,4);
  lcd.backlight();
  display.setBrightness(0x0f);
  display2.setBrightness(0x0f);
  display2.showNumberDec(barang.total, false);
  display.showNumberDec(barang.terhitung, false);

	pinMode(READ_LED_PIN, OUTPUT);
  pinMode(IR, INPUT);
	digitalWrite(READ_LED_PIN, LOW);
  waitForWiFi();
  tag.tap = false;
}

void loop(){
  DateTime now = rtc.now();
  String data;
  if(millis() - waktu.RFdelay > 1000){
    tag.tapState = true;
  }else{
    tag.tapState = false;
  }
  if(readRFID()){
    if(tag.tapState){
      comm.cmdToEsp = "TAG," + tag.card;
      Serial.write(comm.cmdToEsp.c_str());
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
  // if(barang.terhitung == barang.total){
    if(!barang.resetState){
      tag.tap = true;
      barang.resetState = true;
      // delay(1000);
      // display.setSegments(SEG_DONE);
      // display2.setSegments(SEG_DONE);
      // delay(2000);
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
  String data;
  lcd.setCursor(6,1);
  lcd.print("Tap RFID");
  while(!readRFID()){

  }
  while(readRFID()){

  }
  lcd.clear();

  lcd.setCursor(3,1);
  lcd.print("Checking RFID");
  // lcd.print("Connecting to");
  lcd.setCursor(8,2);
  lcd.print("....");
  // lcd.print("WiFi");
  //new
  data = tag.card + ",GET_IP";
  Serial.write(data.c_str());
  while(!Serial.available()){
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
      comm.action = 4;
      }else{
      lcd.print("Run     ");
      comm.action = 1;
    }
  }else{
    if(bs.holdState && !bs.setupState && !bs.stopState){
      lcd.print("Hold");
      comm.action = 2;
    }else if(!bs.holdState && bs.setupState && !bs.stopState){
      lcd.print("Setup");
      comm.action = 3;
    }else if(!bs.holdState && !bs.setupState && bs.stopState){
      lcd.print("Stop");
      comm.action = 4;
    }
    // lcd.print("Stop");
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
  while(Serial.available()){
    char s = Serial.read();
    data += s;
  }
  if(data != ""){
    parsing(data);
    if(comm.jumlahData == 2){
      barang.total = comm.data[0].toInt();
      EEPROM.write(0, barang.total);
      tag.nama = comm.data[1];
    }
  }
  if(data == "success"){
    if(tag.tap){
      digitalWrite(READ_LED_PIN, HIGH);
    }else{
      digitalWrite(READ_LED_PIN, LOW);
    }
  }else if(data == "button0"){
    // bs.holdState = !bs.holdState;
    if(!tag.tap){
      if(!bs.setupState && !bs.stopState){
        bs.holdState = !bs.holdState;
        stateCounter = !stateCounter;
      }
    }
    Serial.write("yess");
  }else if(data == "button1"){
    // bs.setupState = !bs.setupState;
    if(!tag.tap){
      if(!bs.holdState && !bs.stopState){
        bs.setupState = !bs.setupState;
        stateCounter = !stateCounter;
      }
    }
    Serial.write("yess");
  }else if(data == "button2"){
    // bs.stopState = !bs.stopState;
    if(!tag.tap){
      if(!bs.holdState && !bs.setupState){
        bs.stopState = !bs.stopState;
        stateCounter = !stateCounter;
      }
    }
    Serial.write("yess");
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