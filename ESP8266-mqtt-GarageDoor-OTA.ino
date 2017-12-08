#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <PubSubClient.h>         //MQTT
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>


////**********START CUSTOM PARAMS******************//

//Define parameters for the http firmware update
const char* host = "GarageESP";
const char* update_path = "/WebFirmwareUpgrade";
const char* update_username = "admin";
const char* update_password = "YourPassWordHere";

/*
 * not sure if this is working - was original code and couldn't get the relay to turn on
 
//Define the pins
#define RELAY_PIN 5
#define DOOR_PIN 4
*/
 
//Define the pins
const int RELAY_PIN = 5;
const int DOOR_PIN = 4;

//Define MQTT Params. If you don't need to 
#define mqtt_server "10.29.10.20"
#define door_topic "garage/door"
#define button_topic "garage/button"
const char* mqtt_user = "mqtt_user"; 
const char* mqtt_pass = "mqtt_pass";

const char* ssid = "HESKIS-TRAVEL";
const char* password = "(*)@DAniEL";

//************END CUSTOM PARAMS********************//
//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

//Setup the web server for http OTA updates. 
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient espClient;

//Initialize MQTT
PubSubClient client(espClient);

//Setup Variables
String switch1;
String strTopic;
String strPayload;
char* door_state = "UNDEFINED";
char* last_state = "";

//Wifi Manager will try to connect to the saved AP. If that fails, it will start up as an AP
//which you can connect to and setup the wifi
WiFiManager wifiManager;
long lastMsg = 0;

void setup() {
  //Set Relay(output) and Door(input) pins
  pinMode(5, OUTPUT);
  digitalWrite (5, LOW);
  pinMode(4, INPUT);

  Serial.begin(115200);

  //Set the wifi config portal to only show for 3 minutes, then continue.
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect(host);

  //sets up the mqtt server, and sets callback() as the function that gets called
  //when a subscribed topic has data
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); //callback is the function that gets called for a topic sub

  //setup http firmware update page.
  MDNS.begin(host);
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and your password\n", host, update_path, update_username);
}

void loop() {
  //If MQTT client can't connect to broker, then reconnect
  if (!client.connected()) {
    reconnect();
  }
  checkDoorState();
  client.loop(); //the mqtt function that processes MQTT messages
  httpServer.handleClient(); //handles requests for the firmware update page
}

void callback(char* topic, byte* payload, unsigned int length) {
  //if the 'garage/button' topic has a payload "OPEN", then 'click' the relay
  payload[length] = '\0';
  strTopic = String((char*)topic);
  if (strTopic == button_topic)
  {
    switch1 = String((char*)payload);
    if (switch1 == "OPEN")
    {
      //'click' the relay
      Serial.println("ON");
      digitalWrite(5, HIGH);
      delay(6000);
      digitalWrite(5, LOW);
    }
  }
}

void checkDoorState() {
  //Checks if the door state has changed, and MQTT pub the change
  last_state = door_state; //get previous state of door
  if (digitalRead(4) == 0) // get new state of door
    door_state = "OPENED";
  else if (digitalRead(4) == 1)
    door_state = "CLOSED"; 

  if (last_state != door_state) { // if the state has changed then publish the change
    client.publish(door_topic, door_state);
    Serial.println(door_state);
  }
  //pub every minute, regardless of a change.
  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    client.publish(door_topic, door_state);
  }
}

void reconnect() {
  //Reconnect to Wifi and to MQTT. If Wifi is already connected, then autoconnect doesn't do anything.
  wifiManager.autoConnect(host);
  Serial.print("Attempting MQTT connection...");
  if (client.connect(host, mqtt_user, mqtt_pass)) {
    Serial.println("connected");
    client.subscribe("garage/#");
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}
