# PowerWatch

This code uses MQTT with no QoS which works
fine within a local area network but is not so reliable traversing cellular Internet
connections with low signal strength (I am testing with a cellular signal strength
of 20-25%).

A Particle.io Electron-based power monitor for a remote home. Using an Electron
with a LiPo battery and plugged into the wall, checks battery level regularly.
Reports battery level and whether the battery is discharging (power must be out).

It uses MQTT to send these notices and warnings to Home Assistant, where you can set
up automations based on the topics as well as monitor power stability over time (if
that's an issue for you).

To use this code you will need to set either the hostname or IP address of your HA system,
which for this code we assume is running the Mosquito MQTT broker.
Also, when setting up the Mosquitto MQTT integration in HA you will have created
an MQTT username and password - those will also need to be set.

The best way to do this is to create a file "secrets.h" that looks like this if you are
using a hostname:

```
const char *HA_USR = "your mqtt username";
const char *HA_PWD = "your mqtt passwd";
// and use one of the following two lines:
char MY_SERVER[] = "your.server.hostname";
// or using your IP address w.x.y.z
byte MY_SERVER[] = { w, x, y, z };
```

Alternatively, you can comment out the "#include secrets.h" line and fill this information in
following the instructions in the code.

You can take the .ino file from the src directory and paste it into the Particle.io web IDE,
make the above customizations (adding a secrets.h file if you go that route),
then add the libraries (see project.properties for
the libraries. (note if you are reading this a long time from the last update and
having trouble it could be that something has changed in the libraries, so you
can check the version number in the project.properties file as well).
