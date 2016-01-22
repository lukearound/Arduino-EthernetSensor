# Arduino-EthernetSensor
Arduino Sketch for an etherent shield equipped Arduino Uno with temperature, humidity and pressure sensor. The Sketch reads the sensor data in defined intervals and uploads the values into a channel at www.thingspeak.com. The Arduino is accessable via Telenet. Most of the runtime variable can be viewed or changed through simple Telnet commands (list of commands below).

Sensors used:
- BMP180 (air pressure)
- DHT11 (humidity)
- DS18B20 (temperature)

#### The Telnet server runs on Port 23. This is the list of commands it can parse:
- **get ver** | returns software version of current sketch
- **get interval** | returns interval in [msec]
- **get temperature** | returns latest temperture reading in [°C]
- **get pressure** | returns latest air pressure reading in [mBar]
- **get humidity** | returns latest relative humidity [%]
- **get sensors** | returns list of installed sensors
- **get millis** | returns time since start-up in [msec]. This value will roll over after 2^32 msecs
- **get counter** | returns how many intervals have been cycled since start-up. his value will roll over after 2^32 cycles
- **get thingSpeakActive** | returns 0 or 1 and indicates if sensor data is uploaded to the ThingSpeak channel currently
- **get lastTSconnect** | returns 0 or 1 and indicates if the latest ethernet connection to api.thingspeak.com was successful
- **get failedConnectionCount** | returns the number of failed attempts to connect to api.thingspeak.com. This value rolls over after 2^32 counts
- **get ip** | returns current IP of ethernet shield (rather useless since you are connected via Telnet)
- **set interval = 60** | sets new interval timer. Must be specified as int and in seconds
- **set APIKey = ABCDEFGHIJKLMNOP** | sets new authentication key for ThingSpeak channel. ThingSpeak API keys consist of 16 characters
- **set thingSpeakActive = 1** | sets upload to ThingSpeak channel active=1 or inactive=0. 
- **restartEthernet** | restarts the Ethernet client with the same IP address. This will interrupt the telnet connection.
 
Some states are saved to the EEPROM and will be loaded at start-up time. These variables will be set to the previous value after restart:
- APIKey
- interval
- thingSpeakActive

