/*
   Monitor cabin power
    C. Catlett Apr 2019
   2020-Oct  added MQTT to connect to remote Home Assistant
                                          */

#include <Particle.h>
#include <MQTT.h>
#include "secrets.h"
//#include "powercode.h"

FuelGauge fuel;
#define reportingPeriod 3600007  // Battery lasts 12-18h so update every ~1h (1)
#define watching         599999  // watch power at ~10 min (600k ms) intervals
#define closerLook       314159  // watch more closely; every ~5min (300k ms)
#define LINE_PWR              1  // we're plugged in (5 means we are on battery)
float lastPercent     = 0;
float fuelPercent     = 0;
bool  TimeToCheck     = TRUE;
bool  TimeToReport    = TRUE;
int   messageCount    = 0;
bool  PowerIsOn       = TRUE;
int   powerSource     = 0;

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
Timer reportTimer(reportingPeriod, reportPower);

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
    if (DEBUG) Particle.publish("DEBUG", "DEBUG=TRUE", PRIVATE);
    Particle.publish("POWER", String(fuelPercent), PRIVATE);
    powerTimer.start();
    reportTimer.start();
}

void loop() {

    if (TimeToCheck) {
        if (DEBUG) Particle.publish("DEBUG", "checking power", PRIVATE);
        TimeToCheck = FALSE;
        lastPercent = fuelPercent;
        fuelPercent = fuel.getSoC(); 
        // powerSource: 5=wall wart; 2=USB host; 1=battery
        powerSource = System.powerSource();
        if (DEBUG) Particle.publish("DEBUG-Pwr Src", String(powerSource), PRIVATE);
        if (powerSource == LINE_PWR) {
            if (!PowerIsOn) {
              tellHASS(TOPIC_B, String(powerSource));
              Particle.publish("POWER-start ON", String(powerSource), PRIVATE);
            }
            PowerIsOn = TRUE;
            powerTimer.changePeriod(watching);
        } else {
          if (PowerIsOn) {
            Particle.publish("POWER-start OFF", String(powerSource), PRIVATE);
            tellHASS(TOPIC_C, String(powerSource));
          }
            PowerIsOn = FALSE;
            powerTimer.changePeriod(closerLook);
        }
    }

    if (TimeToReport) {
      Particle.publish("POWER", String(fuelPercent), PRIVATE);
      tellHASS(TOPIC_A, String(fuelPercent));
      if (PowerIsOn) {
        tellHASS(TOPIC_B, String(fuelPercent));
        reportTimer.changePeriod(reportingPeriod);
      } else {
        tellHASS(TOPIC_C, String(fuelPercent));
        reportTimer.changePeriod(watching);
      }
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

  if (DEBUG) Particle.publish("DEBUG. TellHASS msg#", String(messageCount), PRIVATE);
    
  client.connect(CLIENT_NAME, HA_USR, HA_PWD);
  client.publish(ha_topic, ha_payload);
  client.disconnect();
}
