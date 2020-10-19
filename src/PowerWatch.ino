/*
   Monitor cabin power
    C. Catlett Apr 2019
   2020-Oct  added MQTT to connect to remote Home Assistant
                                          */

#include <Particle.h>
#include <MQTT.h>
#include "secrets.h"

FuelGauge fuel;
#define casual         3600007  // Battery lasts 12-18h so update every ~1h (1)
#define watching        599999  // watch power at ~10 min (600k ms) intervals
#define closerLook      314159  // watch more closely; every ~5min (300k ms)
float lastPercent     = 0;
float fuelPercent     = 0;
bool  TimeToCheck     = TRUE;
bool  TimeToReport    = TRUE;
int   messageCount    = 0;
bool  PowerIsOn       = TRUE;

/*
 * MQTT parameters
 */
#define MQTT_KEEPALIVE 30 * 60              // 30 minutes but afaict it's ignored...
/*
 * When you configure Mosquitto Broker MQTT in HA you will set a
 * username and password for MQTT - plug these in here if you are not
 * using a secrets.h file. (and comment out the #include "sectets.h" line above)
 */
//const char *HA_USR = "your_ha_mqtt_usrname";
//const char *HA_PWD = "your_ha_mqtt_passwd";
//uncomment this line and fill in w.x.y.z if you are going by IP address,:
//  byte MY_SERVER[] = { 73, 246, 85, 17 };
// OR this one if you are doing hostname (filling in yours)
//  #define MY_SERVER "your.mqtt.broker.tld"
const char *CLIENT_NAME = "photon";
// Topics - these are what you watch for as triggers in HA automations
const char *TOPIC_A = "ha/cabin/powerLevel";
const char *TOPIC_B = "ha/cabin/powerOK";
const char *TOPIC_C = "ha/cabin/powerOUT";
// MQTT functions
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void timer_callback_send_mqqt_data();    
 // MQTT callbacks implementation (not used here but required)
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
     char p[length + 1];
     memcpy(p, payload, length);
     p[length] = 0; 
     Particle.publish("mqtt", p, 3600, PRIVATE);
 }

MQTT client(MY_SERVER, 1883, MQTT_KEEPALIVE, mqtt_callback);

bool DEBUG = FALSE;

Timer powerTimer(closerLook, checkPower);
Timer reportTimer(casual, reportPower);

void setup() {
    Time.zone (-5);
    Particle.syncTime();
    fuelPercent = fuel.getSoC();
    client.connect(CLIENT_NAME, HA_USR, HA_PWD);
    if (client.isConnected()) {
        Particle.publish("mqtt_startup", "Connected to HA", 3600, PRIVATE);
      } else {
        Particle.publish("mqtt_startup", "Failed to connect to HA - check IP address, username, passwd", 3600, PRIVATE);
    }
    if (DEBUG) Particle.publish("debug", "DEBUG=TRUE", PRIVATE);
    powerTimer.start();
    reportTimer.start();
}

void loop() {
    
    if (TimeToCheck) {
        if (DEBUG) Particle.publish("debug", "checking power", PRIVATE);
        TimeToCheck = FALSE;
        lastPercent = fuelPercent;
        fuelPercent = fuel.getSoC(); 

        if (fuelPercent < 85) {     // Below 85%? no- normal fluctuation or only brief power outage
            powerTimer.changePeriod(closerLook);
            if (DEBUG) Particle.publish("debug", "<85% - looking closer", PRIVATE);
            if (fuelPercent < lastPercent){  // going down... lost wall power send a warning
                Particle.publish("POWER", String::format("DIScharging (%.2f)",fuelPercent), PRIVATE);
                if (PowerIsOn) PowerIsOn = FALSE;
                tellHASS(TOPIC_A, String(fuelPercent));
                tellHASS(TOPIC_C, String(fuelPercent));
            } else {
                Particle.publish("POWER", String::format("charging (%.2f)",fuelPercent),PRIVATE);
                if (!PowerIsOn) PowerIsOn = TRUE;
                tellHASS(TOPIC_A, String(fuelPercent));
                tellHASS(TOPIC_B, String(fuelPercent));
            }
          } else {
            if (DEBUG) Particle.publish("debug", ">75% - stop closeLooker timer", PRIVATE);
            powerTimer.changePeriod(watching);
            tellHASS(TOPIC_A, String(fuelPercent));
        }
      }
    if (TimeToReport) {
      tellHASS(TOPIC_A, String(fuelPercent));
      TimeToReport = FALSE;
    }
} 
/************************************/
/***         FUNCTIONS       ***/
/************************************/

// Checking timer interrupt handler
void checkPower() {
  TimeToCheck = TRUE;
}

// Reporting timer interrupt handler
void reportPower() {
  TimeToReport = TRUE;
}

//
// put the mqtt stuff in one place since the error detect/correct
// due to oddly short connection timeouts (ignoring MQTT_KEEPALIVE afaict)
// require recovery code

void tellHASS (const char *ha_topic, String ha_payload) {

  messageCount++;

  if (DEBUG) Particle.publish("tellHASS msg#", String(messageCount), PRIVATE);
    
  client.connect(CLIENT_NAME, HA_USR, HA_PWD);
  client.publish(ha_topic, ha_payload);
  client.disconnect();
}
