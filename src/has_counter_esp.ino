#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

// #define SSID "ZaCK"
// #define PASS "2444666668888888000000"
#define SSID "rumahkucing"
#define PASS "1sl4m4g4m4ku"
#define PIN_HOLD  D2
#define PIN_SETUP D5
#define PIN_STOP  D6

SoftwareSerial serial(D4, D3);

String ip;

const byte BUTTON[3] = {PIN_HOLD, PIN_SETUP, PIN_STOP};

void connectWiFi();
String ipToString(IPAddress ip);

void setup(){
  serial.begin(115200);
  Serial.begin(9600);
  pinMode(PIN_HOLD, INPUT);
  pinMode(PIN_SETUP, INPUT);
  pinMode(PIN_STOP, INPUT);
  connectWiFi();
}

void loop(){
  String data;
  while(serial.available()){
    char s = serial.read();
    data += s;
  }
  if(data != ""){
    if(data == "GET_IP"){
      Serial.print("IP Sent: ");
      Serial.println(ip);
      serial.write(ip.c_str());
    }else{
      if(data.length() == 6){
        Serial.print("Data: ");
        Serial.println(data);
        serial.write("success");
      }
    }
  }
  for(byte i = 0; i < 3; i++){
    if(digitalRead(BUTTON[i]) == HIGH){
      data = "button" + (String)i;
      serial.write(data.c_str());
    }
  }
}

void connectWiFi(){
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  ip = ipToString(WiFi.localIP());
  Serial.println("");
  Serial.println(ip);
}

String ipToString(IPAddress ipA){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ipA[i]) : String(ipA[i]);
  return s;
}