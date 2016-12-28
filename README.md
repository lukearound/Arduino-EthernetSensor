# Arduino-EthernetSensor
Arduino Sketch for an etherent shield equipped Arduino Uno with temperature, humidity and pressure sensor. The Sketch reads the sensor data in defined intervals and uploads the values into a SQL database via python script located at the server hosting the database and a webserver.
Sensors used:

- BMP180 (air pressure)
- DHT11 (humidity)
- DS18B20 (temperature)
