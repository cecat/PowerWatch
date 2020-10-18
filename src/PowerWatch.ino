/*
   Monitor cabin power
    C. Catlett Apr 2019
   2020-Oct  added MQTT to connect to remote Home Assistant
                                          */

#include <Particle.h>
#include <MQTT.h>
#include "secrets.h"

FuelGauge fuel;
                                // prime numbers are cool
#define casual         1200007  // Battery lasts 12-18h so check every ~20 min (1.2M ms)
#define closerLook      314159  // watch more closely; every ~5 min (300k ms)
float lastPercent     = 0;
float fuelPercent     = 0;
bool  TimeToGo        = TRUE;
int   messageCount    = 0;

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
 // MQTT callbacks implementation
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
     char p[length + 1];
     memcpy(p, payload, length);
     p[length] = 0; 
     Particle.publish("mqtt", p, 3600, PRIVATE);
 }

MQTT client(MY_SERVER, 1883, MQTT_KEEPALIVE, mqtt_callback);

//bool DEBUG = TRUE;
//      Timer powerTimer(30000, checkPower);
//      Timer closeLooker(10000, checkPower);
bool DEBUG = FALSE;
      Timer powerTimer(casual, checkPower);
      Timer closeLooker(closerLook, checkPower);

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
    tellHASS(TOPIC_A, String(fuelPercent));
    if (DEBUG) Particle.publish("debug", "DEBUG=TRUE", PRIVATE);
    powerTimer.start();
}

void loop() {
    
    if (TimeToGo) {
        if (DEBUG) Particle.publish("debug", "checking power", PRIVATE);
        TimeToGo = FALSE;
        lastPercent = fuelPercent;
        fuelPercent = fuel.getSoC(); 
        if (fuelPercent < 75) {     // Below 75%? no- normal fluctuation
            closeLooker.start();
            if (DEBUG) Particle.publish("debug", "<75 - looking closer", PRIVATE);
            if (fuelPercent < lastPercent){  // going down... lost wall power send a warning
                Particle.publish("POWER", String::format("DIScharging (%.2f)",fuelPercent), PRIVATE);
                delay(500);
                tellHASS(TOPIC_C, String(fuelPercent));
             } else {
                Particle.publish("POWER", String::format("charging (%.2f)",fuelPercent),PRIVATE);
                delay(500);
                tellHASS(TOPIC_B, String(fuelPercent));
                delay(500);
                tellHASS(TOPIC_A, String(fuelPercent));
                closeLooker.stop();
            }
        } else {
            if (DEBUG) Particle.publish("debug", ">75% - stop closeLooker timer", PRIVATE);
            closeLooker.stop();
            tellHASS(TOPIC_A, String(fuelPercent));
        }
    } 
}
/************************************/
/***         FUNCTIONS       ***/
/************************************/

// Timer interrupt handler
void checkPower() {
  TimeToGo = TRUE;
  // tellHASS(TOPIC_A, String(fuelPercent));
}



//
// put the mqtt stuff in one place since the error detect/correct
// due to oddly short connection timeouts (ignoring MQTT_KEEPALIVE afaict)
// require recovery code

void tellHASS (const char *ha_topic, String ha_payload) {
  if (DEBUG) Particle.publish("tellHASS msg#", String(messageCount), PRIVATE);
  messageCount++;
  delay(500);
  if (client.isConnected()) {
    client.publish(ha_topic, ha_payload);
  } else {
    if (DEBUG) Particle.publish("debug-tellHASS", "was NOT connected", PRIVATE);
    client.connect(CLIENT_NAME, HA_USR, HA_PWD);
    delay(500);
    client.publish(ha_topic, ha_payload);
  } // did it work?
  if (DEBUG) {
    if (client.isConnected()) {
      Particle.publish("debug-tellHASS", "re-connected", PRIVATE);
    } else {
      Particle.publish("debug-tellHASS", "still NOT connected", PRIVATE);
    }
  }

}
