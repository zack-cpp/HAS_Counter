#include <FS.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <NTPClient.h>

#define DEVICE_ID "1otuf6bfM"
#define AP_NAME "HAS Counter"
#define AP_PASS "12345678"

class MQTT{
  public:
  const char* server = "167.86.119.249";
  // const char* server = "45.77.46.252";
  const char* username = "iotmqtt";
  const char* pass = "IOTku@303";
  const char* subTopic = "return/startjob";
  const char* pubTopic = "counter/mesin";
  const char* countTopic = "counter/mesin/jobsend";
  const char* wifiTopic = "config/wifi";
  String serverTemp;
  char serverUI[40];
  uint16_t port = 1883;
  void reconnect();
}mqtt;

struct Network{
  String json;
  String kirim;
  String ip;
  bool saveConfigState = false;
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

struct Waktu{
  byte jam;
  byte menit;
  byte detik;
}waktu;

SoftwareSerial serial(D4, D3);
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200);

void getMQTT(String topic, byte* payload, unsigned int length);
void connectWiFi();
void parsing(String dataString);
void callback(WiFiManager *myWiFiManager);
void saveConfigCalback();
void getTimeNTP();
void writeFile(const char* path);
String ipToString(IPAddress ip);
String readFromFile(const char* path, const char* data);

void setup(){
  WiFi.mode(WIFI_STA);
  serial.begin(9600);
  Serial.begin(115200);
  Serial.println("Mounting FS...");
  if(!SPIFFS.begin()){
    Serial.println("Failed to mount SPIFFS");
  }
  WiFiManagerParameter customParam("server", "MQTT Server", mqtt.serverUI, 40);
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setAPCallback(callback);
  wm.addParameter(&customParam);
  bool res = wm.autoConnect(AP_NAME, AP_PASS);
  if(res){
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    comm.cmdToNano = "CONFIG_DONE\n";
    serial.write(comm.cmdToNano.c_str());
    Serial.println(comm.cmdToNano);
    network.ip = ipToString(WiFi.localIP());
    if(network.saveConfigState){
      strcpy(mqtt.serverUI, customParam.getValue());
      writeFile("/config.json", mqtt.serverUI);
    }
    if(SPIFFS.exists("/config.json")){
      mqtt.serverTemp = readFromFile("/config.json", "mqtt_server");
      network.saveConfigState = false;
    }
  }
  // connectWiFi();
  client.setServer(mqtt.serverTemp.c_str(), mqtt.port);
  client.setCallback(getMQTT);
}

void loop(){
  String data;
  if(!client.connected()){
    mqtt.reconnect();
  }
  while(serial.available()){
    data = serial.readStringUntil('\n');
  }
  if(data != ""){
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
      if(comm.data[0] == "GET_IP" && comm.data[0] != "jobsend" && comm.data[0] != "TAG"){
        if(comm.data[1].length() == 6){
          andon.nanoReady = true;
        }
        Serial.print("IP: ");
        Serial.println(network.ip);
        network.kirim = "";
        network.json = "{\"MESIN_ID\":\"" + (String)DEVICE_ID + "\",\"RF_ID\":\"" + comm.data[1] + "\"}";
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
      }else if(comm.data[0] == "TAG"){
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
    }else{
      if(data == "WIFI_CHECK"){
        if(WiFi.status() == WL_CONNECTED){
          delay(1000);
          serial.write("WL_CONNECTED\n");
          Serial.println("WL_CONNECTED");
        }else{
          serial.write("WL_DISCONNECTED\n");
          Serial.println("WL_DISCONNECTED");
        }
      }else if(data == "TIME_GET"){
        getTimeNTP();
        comm.cmdToNano = "TIME," + (String)waktu.jam + "," + (String)waktu.menit + "," + (String)waktu.detik + "\n";
        serial.write(comm.cmdToNano.c_str());
      }
    }
  }
  client.loop();
}

String readFromFile(const char* path, const char* data){
  String value;
  File file = SPIFFS.open(path, "r");
  if(file){
    StaticJsonDocument<250> doc;
    deserializeJson(doc,file);
    String temp = doc[data].as<String>();
    value = temp;
  }
  file.close();
  return value;
}

void writeFile(const char* path, const char* data){
  delay(2000);
  File file = SPIFFS.open(path, "w");
  StaticJsonDocument<250> doc;
  doc["mqtt_server"] = data;
  if(serializeJson(doc, file) == 0){
    // Serial.println("Failed to write file!");
  }
  else{
    // Serial.println("Success to write file");
  }
  file.close();
}

String ipToString(IPAddress ipA){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ipA[i]) : String(ipA[i]);
  return s;
}

void getMQTT(String topic, byte* payload, unsigned int length){
  String data;
  //String raw;
  andon.target = "";
  andon.nama = "";

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  JsonObject obj = doc.as<JsonObject>();
  
  if(topic == mqtt.subTopic){
    if(andon.nanoReady){
      network.ip += "\n";
      serial.write(network.ip.c_str());
      andon.nanoReady = false;
      delay(1000);
    }
    andon.target = obj["t"].as<String>();
    andon.nama = obj["n"].as<String>();
    
    comm.cmdToNano = andon.target + "," + andon.nama + "\n";
    serial.write(comm.cmdToNano.c_str());
  }else if(topic == mqtt.wifiTopic){
    data = obj["MESIN_ID"].as<String>();
    if(data == DEVICE_ID){
      WiFiManagerParameter customParam("server", "MQTT Server", mqtt.serverUI, 40);
      WiFiManager wm;
      wm.setDebugOutput(false);
      wm.setAPCallback(callback);
      wm.addParameter(&customParam);
      WiFi.mode(WIFI_AP);
      if(wm.startConfigPortal(AP_NAME, AP_PASS)){
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        comm.cmdToNano = "CONFIG_DONE\n";
        network.ip = ipToString(WiFi.localIP());
        Serial.println(comm.cmdToNano);
        serial.write(comm.cmdToNano.c_str());
        if(network.saveConfigState){
          strcpy(mqtt.serverUI, customParam.getValue());
          writeFile("/config.json", mqtt.serverUI);
        }
        if(SPIFFS.exists("/config.json")){
          mqtt.serverTemp = readFromFile("/config.json", "mqtt_server");
          network.saveConfigState = false;
        }
        client.setServer(mqtt.serverTemp.c_str(), mqtt.port);
      }
    }
  }
  //debug
  // Serial.print("Topic: ");
  // Serial.println(topic);
  // Serial.print("CMD: ");
  // Serial.println(comm.cmdToNano.c_str());

  // serializeJson(doc, raw);

  // Serial.print("Raw: ");
  // Serial.println(raw);
}

void MQTT::reconnect(){
  while (!client.connected()){
    delay(1000);
    if(WiFi.status() != WL_CONNECTED){
      WiFiManagerParameter customParam("server", "MQTT Server", mqtt.serverUI, 40);
      WiFiManager wm;
      wm.setDebugOutput(false);
      wm.setAPCallback(callback);
      wm.addParameter(&customParam);
      bool res = wm.autoConnect(AP_NAME, AP_PASS);
      if(res){
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        comm.cmdToNano = "CONFIG_DONE";
        network.ip = ipToString(WiFi.localIP());
        Serial.println(comm.cmdToNano);
        serial.write(comm.cmdToNano.c_str());
        if(network.saveConfigState){
          strcpy(mqtt.serverUI, customParam.getValue());
          writeFile("/config.json", mqtt.serverUI);
        }
        if(SPIFFS.exists("/config.json")){
          mqtt.serverTemp = readFromFile("/config.json", "mqtt_server");
          network.saveConfigState = false;
        }
        client.setServer(mqtt.serverTemp.c_str(), mqtt.port);
      }
    }
    Serial.print("Attempting MQTT connection...");
    if(client.connect(DEVICE_ID,mqtt.username,mqtt.pass)){
      Serial.println("connected");
      client.subscribe(mqtt.subTopic,1);
      client.subscribe(mqtt.wifiTopic,1);
      comm.cmdToNano = "CONFIG_DONE";
      serial.write(comm.cmdToNano.c_str());
    } else {
      comm.cmdToNano = "SERVER_CONFIG\n";
      serial.write(comm.cmdToNano.c_str());
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

void callback(WiFiManager *myWiFiManager){
  network.saveConfigState = true;
  comm.cmdToNano = "AP_CONFIG\n";
  serial.write(comm.cmdToNano.c_str());
  Serial.println("Entering AP Mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println(comm.cmdToNano);
}

void getTimeNTP(){
  timeClient.begin();
  timeClient.update();
  waktu.jam = timeClient.getHours();
  waktu.menit = timeClient.getMinutes();
  waktu.detik = timeClient.getSeconds();
  timeClient.end();
}
