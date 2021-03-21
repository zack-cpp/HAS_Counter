#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>
#include <rdm6300.h>
#include "RTClib.h"

#define RDM6300_RX_PIN 9
#define READ_LED_PIN 13
#define CLK 2
#define DIO 3
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
  bool change = false;
  bool tap = false;
}tag;

struct Time{
  byte millisecond;
  byte prevSecond;
}waktu;

struct Barang{
  unsigned int total = 20;
  unsigned int terhitung = 0;
  unsigned long prevStamp;
  unsigned long currentStamp;
  unsigned int cycleTime = 0;
  bool adjustState = false;
  bool resetState = false;
}barang;

const uint8_t SEG_DONE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
	};

bool countState = false;
bool countPrevState = false;
bool stateCounter = false;

void initialize();
void waitForWiFi();
void readUART();
void showMenu(byte jam, byte menit, byte detik);
unsigned int calculateCycle();
unsigned int calculateMillisRTC(byte detik);

bool readRFID();

void setup(){
	Serial.begin(115200);
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
	rdm6300.begin(RDM6300_RX_PIN);
  waitForWiFi();
}

void loop(){
  DateTime now = rtc.now();
  String data;
  if(readRFID()){
    Serial.write(tag.card.c_str());
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
  if(barang.terhitung == barang.total){
    display.setSegments(SEG_DONE);
    display2.setSegments(SEG_DONE);
    if(!barang.resetState){
      tag.tap = true;
      barang.resetState = true;
    }
  }else{
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
  lcd.clear();

  lcd.setCursor(3,1);
  lcd.print("Connecting to");
  lcd.setCursor(8,2);
  lcd.print("WiFi");
  Serial.write("GET_IP");
  while(!Serial.available()){
    Serial.write("GET_IP");
    delay(10);
  }
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
  lcd.print(tag.card);
  lcd.setCursor(0,2);
  lcd.print("Stat  : ");
  if(stateCounter){
    if(tag.tap){
      lcd.print("Complete");
    }else{
      lcd.print("Run     ");
    }
  }else{
    lcd.print("Stop");
  }
  lcd.setCursor(0,3);
  menu = "Cycle : " + (String)barang.cycleTime + " ms";
  for(byte i = 0; i < menu.length(); i++){
    lcd.print(menu[i]);
  }
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
  if(data == "success"){
    if(tag.tap){
      digitalWrite(READ_LED_PIN, HIGH);
    }else{
      digitalWrite(READ_LED_PIN, LOW);
    }
  }
}

unsigned int calculateCycle(){
  unsigned int hasil;
  if(!tag.tap){
    if(countState != countPrevState){
      countPrevState = countState;    
      if(countState){
        barang.terhitung++;
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