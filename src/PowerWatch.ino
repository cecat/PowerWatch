/*
   Monitor cabin power
    C. Catlett Apr 2019
                                          */

#include <Particle.h>
#include <MQTT.h>
#include "secrets.h"

FuelGauge fuel;
bool DEBUG = FALSE;
                                // prime numbers are cool
#define casual        12000007  // Battery lasts 12-18h so check every ~20 min (1.2M ms)
#define closerLook      314159  // watch more closely; every ~5 min (300k ms)
#define debugFreq        30011  // in debug mode look every ~30 sec (30k ms)
int   interval        = casual;
float lastPercent     = 0;
float fuelPercent     = 0;
float lastReport      = 0;

/*
 * MQTT parameters
 */
#define MQTT_KEEPALIVE 30 * 60              // 30 minutes but afaict it's ignored...

/*
 * When you configure Mosquitto Broker MQTT in HA you will set a
 * username and password for MQTT - plug these in here if you are not
 * using a secrets.h file.
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
void timer_callback_send_mqqt_data();    // tbh I don't know what this is as it seems not used and spells mqtt wrong

 // MQTT callbacks implementation
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
     char p[length + 1];
     memcpy(p, payload, length);
     p[length] = 0; // was = NULL but that threw a compiler warning
     Particle.publish("mqtt", p, 3600, PRIVATE);
 }

MQTT client(MY_SERVER, 1883, MQTT_KEEPALIVE, mqtt_callback);

void setup() {
    Time.zone (-5);
    Particle.syncTime();
    fuelPercent = fuel.getSoC();
    client.connect(CLIENT_NAME, HA_USR, HA_PWD);
    if (client.isConnected()) {
        Particle.publish("mqtt_status", "Connected to HA", 3600, PRIVATE);
      } else {
        Particle.publish("mqtt_status", "Failed to connect to HA - check IP address, username, passwd", 3600, PRIVATE);
    }
    tellHASS(TOPIC_A, String(fuelPercent));
    if (DEBUG) {
        Particle.publish("debug", "DEBUG=TRUE", PRIVATE);
        interval = debugFreq;
    }
    lastReport = millis();
}

void loop() {
    
    if ((millis() - lastReport) > interval) {
        lastPercent = fuelPercent;
        fuelPercent = fuel.getSoC(); 
        lastReport = millis();
        if (fuelPercent < 75) {     // Below 75%? no- normal fluctuation
            interval = closerLook;  // yes-  start checking every 10 minutes
            if (fuelPercent < lastPercent){  // going down... lost wall power send a warning
                Particle.publish("POWER", String::format("DISCHARGING (charge level %.2f)",fuelPercent), PRIVATE);
                delay(500);
                tellHASS(TOPIC_C, String(fuelPercent));
             }
            if (fuelPercent > lastPercent) { // going up... wall power back
                Particle.publish("POWER", "CHARGING",PRIVATE);
                delay(500);
                tellHASS(TOPIC_B, String(fuelPercent));
                delay(500);
                tellHASS(TOPIC_A, String(fuelPercent));
                interval = casual;    // set back to hourly checking - out of danger
            }
        } else {
            interval = casual;   
            tellHASS(TOPIC_A, String(fuelPercent));
        }
    if (DEBUG) interval = debugFreq;
    } 
}
/************************************/
/***         FUNCTIONS       ***/
/************************************/


//
// put the mqtt stuff in one place since the error detect/correct
// due to oddly short connection timeouts (ignoring MQTT_KEEPALIVE afaict)
// require recovery code

void tellHASS (const char *ha_topic, String ha_payload) {

  delay(500);
  if (client.isConnected()) {
    client.publish(ha_topic, ha_payload);
  } else {
    if (DEBUG) Particle.publish("mqtt_status", "was NOT connected", PRIVATE);
    client.connect(CLIENT_NAME, HA_USR, HA_PWD);
    delay(500);
    client.publish(ha_topic, ha_payload);
  } // did it work?
  if (DEBUG) {
    if (client.isConnected()) {
      Particle.publish("mqtt_status", "connected", PRIVATE);
    } else {
      Particle.publish("mqtt_status", "still NOT connected", PRIVATE);
    }
  }

}
