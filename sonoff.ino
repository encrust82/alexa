#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

#define D0              16        //WAKE  =>  16
#define D1              5         //IOS   =>  5
#define D2              4         //      =>  4
#define D3              0         //      =>  0
#define D4              2         //      =>  2
#define D5              14        //CLK   =>  14
#define D6              12        //MISO  =>  12 
#define D7              13        //MOSI  =>  13
#define D8              15        //CS    =>  15
#define D9              3         //RX    =>  3

#define RELAY_PIN       D6
#define TOGGLE_PIN      D3
#define LED_PIN         D7

#define MQTT_VERSION    MQTT_VERSION_3_1_1
const PROGMEM uint16_t  MQTT_SERVER_PORT          = 1883;
const PROGMEM char*     MQTT_CLIENT_ID            = "sonoff";
const PROGMEM char*     MQTT_USER                 = "MQTT_USER";
const PROGMEM char*     MQTT_PASSWORD             = "MQTT_PASSWORD";
const char*             MQTT_LIGHT_STATE_TOPIC    = "room/sonoff/status";
const char*             MQTT_LIGHT_COMMAND_TOPIC  = "room/sonoff/switch";
const char*             LIGHT_ON                  = "ON";
const char*             LIGHT_OFF                 = "OFF";
boolean                 m_light_state             = false;
boolean                 fDebug                    = true;
long                    lastMsg                   = 0;
long unsigned int       pause                     = 5000;
int                     chk;
int                     TOGGLE_COUNT              = 0;

WiFiClient espClient;
PubSubClient client(espClient);
IPAddress MQTT_SERVER_IP;

void ISR_Reset(){
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.reset();  
}

void setup() {
  ConfigHardware();
  if ( fDebug ) {
    Serial.println();
    Serial.println("Ver: 1.3");
  }
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  if ( fDebug == true ) {
    wifiManager.setDebugOutput(true);
  }else{
    wifiManager.setDebugOutput(false);
  }
  //exit after config instead of connecting
  //wifiManager.setBreakAfterConfig(true);
  //reset settings - for testing
  //wifiManager.resetSettings();

  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" with password "password"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ThingMaBob")) {
    //Debugger "failed to connect, we should reset as see if it connects",NULL;
    delay(3000);
    ESP.reset();
  }
  //if you get here you have connected to the WiFi
  // init the MQTT connection
  WiFi.hostByName("assistant.ussharps.co.uk", MQTT_SERVER_IP);
  if ( fDebug ) {
    Serial.print("MQTT_SERVER_IP: ");
    Serial.println(MQTT_SERVER_IP);
    client.setServer(MQTT_SERVER_IP , MQTT_SERVER_PORT);
    client.setCallback(callback);
  }
}

void loop() {
  int mqttState = client.state();
  if (mqttState != 0){
    Serial.print("State: ");
    Serial.println(mqttState);
  }
  if (!client.connected()) {
    reconnect();
  }
  if (digitalRead(TOGGLE_PIN) == LOW) {
      m_light_state = !m_light_state;
      setLightState();
  }
  delay(250);
}

void ConfigHardware(){
  
  if ( fDebug ) {
    Serial.begin(115200);
    Serial.println();
    Serial.print("Service started");
    Serial.println();
  }
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TOGGLE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if ( fDebug ) {Serial.print("INFO: Attempting MQTT connection...");}
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      if ( fDebug ) {Serial.println("INFO: connected");}
      // Once connected, publish an announcement...
      publishLightState();
      // ... and resubscribe
      client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
    } else {
      if ( fDebug ) {
        Serial.print("ERROR: failed, rc=");
        Serial.print(client.state());
        Serial.println("DEBUG: try again in 5 seconds");
      }
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
// function called to publish the state of the light (on/off)
void publishLightState() {
  if (m_light_state) {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
  } else {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
  }
  client.loop();
}

// function called to turn on/off the light
void setLightState() {
  if (m_light_state) {
    digitalWrite(RELAY_PIN, HIGH);
    if ( fDebug ) {Serial.println("INFO: Turn light on...");}
    digitalWrite(LED_PIN,LOW);
  } else {
    digitalWrite(RELAY_PIN, LOW);
    if ( fDebug ) {Serial.println("INFO: Turn light off...");}
    digitalWrite(LED_PIN,HIGH);
  }
  publishLightState();
  client.loop();
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  if ( fDebug ) {
    Serial.println("Callback");
    Serial.print("Payload: ");
  }
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  if ( fDebug ) {Serial.println(payload);}
  // handle message topic
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      if (m_light_state != true) {
        m_light_state = true;
        setLightState();
      }
    } else if (payload.equals(String(LIGHT_OFF))) {
      if (m_light_state != false) {
        m_light_state = false;
        setLightState();
      }
    }
    publishLightState();  
  }
}

