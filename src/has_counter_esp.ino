#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define SSID "Nokia 6.1 Plus"
#define PASS "yeyekhaha"
// #define SSID "ZaCK"
// #define PASS "2444666668888888000000"
// #define DEVICE_ID "7hx2vOBXJ"
#define DEVICE_ID "1otuf6bfM"
// #define SSID "rumahkucing"
// #define PASS "1sl4m4g4m4ku"
#define PIN_HOLD  D2
#define PIN_SETUP D5
#define PIN_STOP  D6

class MQTT{
  public:
  const char* server = "167.86.119.249";
  const char* username = "iotmqtt";
  const char* pass = "IOTku@303";
  const char* subTopic = "return/startjob";
  const char* pubTopic = "counter/mesin";
  const char* countTopic = "counter/mesin/jobsend";
  uint16_t port = 1883;
  void reconnect();
}mqtt;

struct Network{
  String json;
  String kirim;
  String ip;
}network;

struct Comm{
  String data[3];
  String cmdToNano;
  byte jumlahData = 1;
}comm;

struct Andon{
  String target;
  String nama;
  bool start = false;
  bool nanoReady = false;
}andon;

SoftwareSerial serial(D4, D3);
WiFiClient espClient;
PubSubClient client(espClient);

const byte BUTTON[3] = {PIN_HOLD, PIN_SETUP, PIN_STOP};

void getMQTT(char* topic, byte* payload, unsigned int length);
void connectWiFi();
void parsing(String dataString);
String ipToString(IPAddress ip);

void setup(){
  serial.begin(115200);
  Serial.begin(9600);
  pinMode(PIN_HOLD, INPUT);
  pinMode(PIN_SETUP, INPUT);
  pinMode(PIN_STOP, INPUT);
  connectWiFi();
  client.setServer(mqtt.server,mqtt.port);
  client.setCallback(getMQTT);
}

void loop(){
  String data;
  if(WiFi.status() == WL_CONNECTED){
    if(!client.connected()){
      mqtt.reconnect();
    }
    while(serial.available()){
      char s = serial.read();
      data += s;
    }
    if(data != ""){
      //  new
      parsing(data);
      if(comm.jumlahData != 1){
        Serial.print("\nRaw: ");
        Serial.println(data);
        Serial.print("Data 0: ");
        Serial.println(comm.data[0]);
        Serial.print("Data 1: ");
        Serial.println(comm.data[1]);
        Serial.print("Data 2: ");
        Serial.println(comm.data[2]);
        if(comm.data[0].length() == 6 && comm.data[0] != "jobsend" && comm.data[0] != "TAG"){
          if(comm.data[1] == "GET_IP"){
            andon.nanoReady = true;
          }
          network.kirim = "";
          network.json = "{\"MESIN_ID\":\"" + (String)DEVICE_ID + "\",\"RF_ID\":\"" + comm.data[0] + "\"}";
          Serial.print("JSON: ");
          Serial.println(network.json);
          StaticJsonDocument<256> doc;
          deserializeJson(doc, network.json);
          serializeJson(doc, network.kirim);
          client.publish(mqtt.pubTopic, network.kirim.c_str());
        }else if(comm.data[0] == "jobsend"){
          network.kirim = "";
          network.json = "{\"MESIN_ID\":\"" + (String)DEVICE_ID + "\",\"ACTUAL\":\"" + comm.data[1] + "\",\"action\":\"" + comm.data[2] + "\"}";
          Serial.print("JSON: ");
          Serial.println(network.json);
          StaticJsonDocument<256> doc;
          deserializeJson(doc, network.json);
          serializeJson(doc, network.kirim);
          client.publish(mqtt.countTopic, network.kirim.c_str());
        }else if(comm.data[0] == "TAG" /*&& comm.data[2] != ""*/){
          if(comm.data[1].length() == 6){
            serial.write("success");
            network.kirim = "";
            network.json = "{\"MESIN_ID\":\"" + (String)DEVICE_ID + "\",\"RF_ID\":\"" + comm.data[1] + "\"}";
            Serial.print("JSON: ");
            Serial.println(network.json);
            StaticJsonDocument<256> doc;
            deserializeJson(doc, network.json);
            serializeJson(doc, network.kirim);
            client.publish(mqtt.pubTopic, network.kirim.c_str());
            comm.data[2] = "";
          }
        }
      }
    }
    for(byte i = 0; i < 3; i++){
      if(digitalRead(BUTTON[i]) == LOW){
        while(digitalRead(BUTTON[i]) == LOW){

        }
        data = "button" + (String)i;
        Serial.println(data);
        serial.write(data.c_str());
        // while(!serial.available()){
        //   delay(10);
        // }
      }
    }
    client.loop();
  }else{
    connectWiFi();
  }  
}

void connectWiFi(){
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  network.ip = ipToString(WiFi.localIP());
  Serial.println("");
  Serial.println(network.ip);
}

String ipToString(IPAddress ipA){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ipA[i]) : String(ipA[i]);
  return s;
}

void getMQTT(char* topic, byte* payload, unsigned int length){
  String data = "";
  String raw;
  andon.target = "";
  andon.nama = "";

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  JsonObject obj = doc.as<JsonObject>();
  
  if(topic == topic){
    if(andon.nanoReady){
      serial.write(network.ip.c_str());
      andon.nanoReady = false;
      delay(1000);
    }
    andon.target = obj["t"].as<String>();
    andon.nama = obj["n"].as<String>();
    
    comm.cmdToNano = andon.target + "," + andon.nama;
    serial.write(comm.cmdToNano.c_str());
  }
  //debug
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("CMD: ");
  Serial.println(comm.cmdToNano.c_str());

  serializeJson(doc, raw);

  Serial.print("Raw: ");
  Serial.println(raw);
}

void MQTT::reconnect(){
  while (!client.connected()){
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if(client.connect(DEVICE_ID,mqtt.username,mqtt.pass)){
      Serial.println("connected");
      client.subscribe(mqtt.subTopic,1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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