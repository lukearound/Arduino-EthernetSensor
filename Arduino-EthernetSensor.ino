// GitHub home of this Sketch: https://github.com/lukearound/Arduino-EthernetSensor
//
// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// 


//#include <SPI.h>      // uncomment for Serial Monitoring
//#include <Wire.h>
#include <Ethernet.h>
#include <DallasTemperature.h>
#include <Adafruit_BMP085.h>     // for pressure sensor
#include "DHT.h"

#define DHTPIN 2     // what digital pin we're connected to
#define DHTTYPE DHT11   // DHT 11
#define TEMP_PIN 7               // OneWire Pin zu Sensoren (im Moment nur einer angeschlossen)
#define RESENDS 3                // how many times each message is repeated
#define RESEND_INTERVAL 1.5      // [sec] how long to wait between resend attempts 
#define SEND_INTERVAL 300        // [sec] how long to wait in between transmissions
#define MESS_MAX_LEN 63          // maximum length of 433MHz message
#define TEMP_PIN 7               // OneWire Pin to sensors (custom shield)
#define SS_TX 4                  // serial TX pin for HC-12 433MHz transmitter. Connect to HC-12 RX!!
#define SS_RX 5                  // serial RX pin for HC-12 433MHz transmitter. Connect to HC-12 TX!!
#define SS_SET 6                 // HC-12 set pin. Pull LOW to configure HC-12
#define LED_PIN 13               // pin of on board LED


//String sqlTable;
//String param_temp = "t=";
//String param_mid = "mid=";
//String param_loc = "loc=";
//String emptyString = "";
String Svalue = "";
String Sdb_table = "sensordata_dachboden";

char server[] = "mainpi";


// sensors ***************************************************************************
OneWire            onewire(TEMP_PIN);  // DS18S20 Temperaturchip i/o an pin 10
DallasTemperature  ds(&onewire);
Adafruit_BMP085    bmp;  // pressure sensor BMP180 (GY-68)

//DeviceAddress 1 = { 0x28, 0xBE, 0xFA, 0x3B, 0x07, 0x00, 0x00, 0xB7 };  // DS18S20 #1
//DeviceAddress 2 = { 0x28, 0x9A, 0xF1, 0x3C, 0x07, 0x00, 0x00, 0x2B };  // DS18S20 #2
//DeviceAddress 3 = { 0x28, 0x78, 0xB2, 0x3B, 0x07, 0x00, 0x00, 0x4D };  // DS18S20 #3
DeviceAddress tprobe1 = { 0x28, 0xAB, 0x70, 0x29, 0x07, 0x00, 0x00, 0x4E };    // DS18S20 #4

DHT dht(DHTPIN, DHTTYPE);   // humidity sensor


// Ethernet **************************************************************************
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xA5 };   // MUST BE DIFFERENT FOR EACH SENSOR!
IPAddress ip(192, 168, 1, 208);    // MUST BE DIFFERENT FOR EACH SENSOR! Just for backup if DHCP doesn't work.

//IPAddress myDns(192,168,1, 1);
//IPAddress gateway(192, 168, 1, 1);
//IPAddress subnet(255, 255, 255, 0);


EthernetClient ethernet_client;


// *********************************************************************************************
void setup() {
  // disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(LED_PIN, OUTPUT);    // prepare on board led to signal activity

  // initialize the ethernet device
  if (Ethernet.begin(mac) == 0) {
    //Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    delay(500);
    Ethernet.begin(mac, ip);
  }
  delay(1000);  // allow some start-up time for ethernet shield

  
  ds.begin();    // start up temperature sensor
  ds.setResolution(tprobe1, 11);  // 11: 0.125°C

  bmp.begin();   // start up pressure sensor
  dht.begin();   // start up humidity sensor

  delay(1000); // initial start-up time to settle hardware etc
}




// handling the timing
unsigned long previousTime = millis()/1000 - SEND_INTERVAL;
unsigned long currentTime = 0;
int           nomsgcount = 0;




void loop() {

  float t1 = 0;
  float pressure = 0;
  float humidity = 0;

  int sensor_success = 0;
  unsigned long crc;

  currentTime = millis()/1000;

  if(currentTime - previousTime > SEND_INTERVAL)
  {
    previousTime = currentTime;
    digitalWrite(LED_PIN, HIGH);   // switch on LED
    
    ds.requestTemperatures();
    t1 = ds.getTempC(tprobe1);
    pressure = 0.01*bmp.readPressure();  // convert to mBar
    humidity = dht.readHumidity();

    // check if temperature makes sense and transmit if it does 
    if(t1 > -50)
    {
      Svalue = String(t1);  
      String param_str = "";

      param_str.concat("tab=");
      param_str.concat(Sdb_table);
      param_str.concat("&temp=");
      param_str.concat(Svalue);

      delay(500);
      boolean suc = httpRequest(param_str);
    }

    // check if humidity makes sense and transmit if it does 
    if(humidity > 0)
    {
      Svalue = String(humidity);  
      String param_str = "";

      param_str.concat("tab=");
      param_str.concat(Sdb_table);
      param_str.concat("&hum=");
      param_str.concat(Svalue);

      delay(500);
      boolean suc = httpRequest(param_str);
    }

    // check if pressure makes sense and transmit if it does 
    if(pressure > 0)
    {
      Svalue = String(pressure);  
      String param_str = "";

      param_str.concat("tab=");
      param_str.concat(Sdb_table);
      param_str.concat("&press=");
      param_str.concat(Svalue);

      delay(500);
      boolean suc = httpRequest(param_str);
    }
  }
}





boolean httpRequest(String &parameters)
{
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  ethernet_client.stop();
  delay(10);

  // if there's a successful connection:
  if(ethernet_client.connect(server, 80)) 
  {
    // send the HTTP GET request:
    // http://mainpi/cgi-bin/sqlentry.py?tab=sensordata_brunnen&temp=20
     
    ethernet_client.print("GET /cgi-bin/sqlentry.py?");
    ethernet_client.print(parameters.c_str());
    ethernet_client.println(" HTTP/1.1");
    ethernet_client.println("Host: mainpi");
    ethernet_client.println("User-Agent: Arduino");
    ethernet_client.println("Accept: text/html");
    ethernet_client.println("Connection: close");
    ethernet_client.println();

    Serial.print("GET /cgi-bin/sqlentry.py?");
    Serial.print(parameters.c_str());
    Serial.println(" HTTP/1.1");

    return true;
  } 
  else 
  {
    // if you couldn't make a connection:
    return false;
  }
}



















//












