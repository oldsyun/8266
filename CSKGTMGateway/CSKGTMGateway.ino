#include <FS.h>
#include <ModbusMaster.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>  
#include <Ticker.h>

#define parameters_size 20
#define mqtt_topic_max_size 100
#define Gateway_Name ""
#define maxMQTTretry 10
#define will_Topic  "/state"
#define will_QoS 0
#define will_Retain true
#define will_Message "offline"
#define Gateway_AnnouncementMsg "online"
#define toMQTT  "/data"

#if defined(ESP8266) && !defined(D5)
#define D5 (14)
#define D6 (12)
#define TRIGGERPIN (5) //D1
#define LED (2)
#endif
#define BAUD_RATE 9600

char mqtt_user[parameters_size] = "your_username"; // not compulsory only if your broker needs authentication
char mqtt_pass[parameters_size] = "your_password"; // not compulsory only if your broker needs authentication
char mqtt_server[parameters_size] = "193.112.184.216";
char mqtt_port[6] = "1883";
char mqtt_topic[mqtt_topic_max_size] = "/gateway/";
char gateway_name[parameters_size * 2] = Gateway_Name;
bool shouldSaveConfig = false;
bool connectedOnce = false;
int failure_number = 0;
unsigned long lastMQTTReconnectAttempt = 0;
unsigned long lastNTWKReconnectAttempt = 0;
uint8_t MAC_array[6];

ModbusMaster node;
SoftwareSerial swSer1;
WiFiClient espClient;
PubSubClient client(espClient);
Ticker tickerRead;
Ticker tickerPub;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup_wifimanager(bool reset_settings )
{
   if (reset_settings)
    {
      WiFi.disconnect(true);
      Serial.println("Formatting requested, result:");
      Serial.println(SPIFFS.format());
      ESP.reset();
      }
  else{
    WiFi.mode(WIFI_STA);
    Serial.println("mounting FS...");
    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) 
      {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);
          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success())
          {
            Serial.println("\nparsed json");
            if (json.containsKey("mqtt_server"))
              strcpy(mqtt_server, json["mqtt_server"]);
            if (json.containsKey("mqtt_port"))
              strcpy(mqtt_port, json["mqtt_port"]);
            if (json.containsKey("mqtt_user"))
              strcpy(mqtt_user, json["mqtt_user"]);
            if (json.containsKey("mqtt_pass"))
              strcpy(mqtt_pass, json["mqtt_pass"]);
            if (json.containsKey("mqtt_topic"))
              strcpy(mqtt_topic, json["mqtt_topic"]);
            if (json.containsKey("gateway_name"))
              strcpy(gateway_name, json["gateway_name"]);
          } 
          else 
          {
            Serial.println("failed to load json config");
          }
          configFile.close();
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }
    //end read
    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, parameters_size);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, parameters_size);
    WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, parameters_size);
    WiFiManagerParameter custom_mqtt_topic("topic", "mqtt base topic", mqtt_topic, mqtt_topic_max_size);
    WiFiManagerParameter custom_gateway_name("name", "gateway name", gateway_name, parameters_size * 2);
  
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
  
    //wifiManager.setConnectTimeout(3);
    //Set timeout before going to portal
   // wifiManager.setConfigPortalTimeout(30);
    
    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
  
    //set static ip
    //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_gateway_name);
    wifiManager.addParameter(&custom_mqtt_topic);
  
    if (!wifiManager.autoConnect("CSKGTMGateway", "j10j10j10")) 
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  
    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
    strcpy(mqtt_topic, custom_mqtt_topic.getValue());
    strcpy(gateway_name, custom_gateway_name.getValue());
  
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["mqtt_user"] = mqtt_user;
      json["mqtt_pass"] = mqtt_pass;
      json["mqtt_topic"] = mqtt_topic;
      json["gateway_name"] = gateway_name;
  
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }
  }
}

void setup_parameters()
{
  strcat(mqtt_topic, gateway_name);
}

void callback(char* topic, byte* payload, unsigned int length)
 {

}

void reconnect()
 {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
      // Attempt to connect
    String willTopic = String(mqtt_topic) + String(will_Topic);
    if (client.connect(gateway_name, mqtt_user, mqtt_pass, (char *)willTopic.c_str(), will_QoS, will_Retain, will_Message))
    {   
      Serial.println(F("Connected to broker"));
      failure_number = 0;
      // Once connected, publish an announcement...
      client.publish((char *)willTopic.c_str(), Gateway_AnnouncementMsg, will_Retain);
      //mRead();
    }
    else
    {
      failure_number++; // we count the failure
      if (failure_number > maxMQTTretry )
      {
        ESP.reset();
      }
      Serial.println(F("failed, rc="));
      Serial.println(client.state());
      delay(5000);
    }  
   }
}

void setup() {
  pinMode(2, OUTPUT);  
  pinMode(15,INPUT);
  Serial.begin(115200);
  swSer1.begin(9600, SWSERIAL_8N1, D5, D6, false, 256);
  node.begin(1, swSer1);
  WiFi.macAddress(MAC_array);
  sprintf(gateway_name, "%s%02X%02X%02X%02X%02X%02X", gateway_name,MAC_array[0],MAC_array[1],MAC_array[2], MAC_array[3],MAC_array[4],MAC_array[5]);
  setup_wifimanager(false);
  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.localIP().toString());
  long port;
  port = strtol(mqtt_port, NULL, 10);
  setup_parameters();
  client.setServer(mqtt_server, port);
  client.setCallback(callback);
  lastMQTTReconnectAttempt = 0;
  lastNTWKReconnectAttempt = 0;
  tickerRead.attach(2, flash);
  tickerPub.attach(60,MqttToPub); //60s 一次发送
}

void MqttToPub()
{
  if (connectedOnce)
  {
    mRead();
    }
  else
  {
    Serial.println("wifi Not Ready yet");
    }
}

void mRead()
{
  uint8_t result = node.readHoldingRegisters(0xA0, 51);
  if (result== node.ku8MBSuccess)
  {
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(37);
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& device = root.createNestedArray("device");
    JsonObject& device_0 = device.createNestedObject();
    device_0["dev_name"] = gateway_name;
    device_0["comm_s"] = "1";
    JsonObject& device_0_variable = device_0.createNestedObject("variable");
    device_0_variable["V1"] =  node.getResponseBuffer(0)/100.0f;
    device_0_variable["V2"] = node.getResponseBuffer(1)/100.0f;
    device_0_variable["V3"] = node.getResponseBuffer(2)/100.0f;
    device_0_variable["I1"] = node.getResponseBuffer(3)/100.0f;
    device_0_variable["I2"] = node.getResponseBuffer(4)/100.0f;
    device_0_variable["I3"] = node.getResponseBuffer(5)/100.0f;
    device_0_variable["I6"] = node.getResponseBuffer(6)/100.0f;
    device_0_variable["I7"] = node.getResponseBuffer(7)/100.0f;
    device_0_variable["I8"] = node.getResponseBuffer(8)/100.0f;
    device_0_variable["I9"] = node.getResponseBuffer(9)/100.0f;
    device_0_variable["I10"] = node.getResponseBuffer(10)/100.0f;
    device_0_variable["I11"] = node.getResponseBuffer(11)/100.0f;
    device_0_variable["I12"] = node.getResponseBuffer(12)/100.0f;
    device_0_variable["P1"] = node.getResponseBuffer(13);
    device_0_variable["P2"] = node.getResponseBuffer(14);
    device_0_variable["P3"] = node.getResponseBuffer(15);
    device_0_variable["P4"] = node.getResponseBuffer(16);
    device_0_variable["P5"] = node.getResponseBuffer(17);
    device_0_variable["P6"] = node.getResponseBuffer(18);
    device_0_variable["P7"] = node.getResponseBuffer(19);
    device_0_variable["P8"] = node.getResponseBuffer(20);
    device_0_variable["P9"] = node.getResponseBuffer(21);
    device_0_variable["P10"] = node.getResponseBuffer(22);
    device_0_variable["P11"] = node.getResponseBuffer(23);
    device_0_variable["P12"] = node.getResponseBuffer(24);
    device_0_variable["EP1"] = (node.getResponseBuffer(28)<< 16 + node.getResponseBuffer(29) )/100.0f;
    device_0_variable["EP2"] = (node.getResponseBuffer(30)<< 16 + node.getResponseBuffer(31) )/100.0f;
    device_0_variable["EP3"] = (node.getResponseBuffer(32)<< 16 + node.getResponseBuffer(33) )/100.0f;
    device_0_variable["EP4"] = (node.getResponseBuffer(34)<< 16 + node.getResponseBuffer(35) )/100.0f;
    device_0_variable["EP5"] = (node.getResponseBuffer(36)<< 16 + node.getResponseBuffer(37) )/100.0f;
    device_0_variable["EP6"] = (node.getResponseBuffer(38)<< 16 + node.getResponseBuffer(39) )/100.0f;
    device_0_variable["EP7"] = (node.getResponseBuffer(40)<< 16 + node.getResponseBuffer(41) )/100.0f;
    device_0_variable["EP8"] = (node.getResponseBuffer(42)<< 16 + node.getResponseBuffer(43) )/100.0f;
    device_0_variable["EP9"] = (node.getResponseBuffer(44)<< 16 + node.getResponseBuffer(45) )/100.0f;
    device_0_variable["EP10"] = (node.getResponseBuffer(46)<< 16 + node.getResponseBuffer(47) )/100.0f;
    device_0_variable["EP11"] = (node.getResponseBuffer(48)<< 16 + node.getResponseBuffer(49) )/100.0f;
    device_0_variable["EP12"] = (node.getResponseBuffer(50)<< 16 + node.getResponseBuffer(51) )/100.0f;
    String topic = String(mqtt_topic) + String(toMQTT);
    char JSONmessageBuffer[1300];
    root.printTo(JSONmessageBuffer,sizeof(JSONmessageBuffer));
    if(client.publish((char *)topic.c_str(),JSONmessageBuffer))
    {
      Serial.println( topic);
      root.printTo(Serial);
      Serial.println(" ");}
    }
    else 
    {
        Serial.println( "ttl modbus read error!");
    }
}
  
void flash()
{  
  if (connectedOnce){
    static boolean output = HIGH;
    digitalWrite(LED, output);
    output = !output;
    }
  else{
    digitalWrite(LED, LOW);
    }
}

void checkButton()
{ 
  if (digitalRead(TRIGGERPIN) == LOW)
  {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(TRIGGERPIN) == LOW)
    {
      Serial.println(F("Trigger button Pressed"));
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(6000); // reset delay hold
      if (digitalRead(TRIGGERPIN) == LOW)
      {
        Serial.println(F("Button Held"));
        Serial.println(F("Erasing ESP Config, restarting"));
        setup_wifimanager(true);
      }
    }
  }
}

void loop() {
  unsigned long now = millis();
  checkButton();
  if (WiFi.status() == WL_CONNECTED)
  {
    lastNTWKReconnectAttempt = 0;
    if (client.connected())
    {
      // MQTT loop
      connectedOnce = true;
      lastMQTTReconnectAttempt = 0;
      client.loop();    
    }
    else{
      connectedOnce=false;
      if (now - lastMQTTReconnectAttempt > 5000)
      {
        lastMQTTReconnectAttempt = now;
        reconnect();
      }
    }
  }
}
