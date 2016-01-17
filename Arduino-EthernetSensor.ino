
// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// EOC is not used, it signifies an end of conv


#include <SPI.h>
#include <Ethernet.h>
#include <DallasTemperature.h>
#include <Adafruit_BMP085.h>     // for pressure sensor


#define VER "1.0 - 11.01.2016"
#define TEMP_PIN 7               // OneWire Pin zu Sensoren (im Moment nur einer angeschlossen)

unsigned long interval = 5000L;  // milli seconds
unsigned long updateCounter = 0L;
unsigned long previousMillis = 0L;
float temperature = -127;
float pressure = 0;
float humidity = 0;


// sensors ***************************************************************************
OneWire            ds(TEMP_PIN);  // DS18S20 Temperaturchip i/o an pin 10
DallasTemperature  sensors(&ds);
Adafruit_BMP085    bmp;  // pressure sensor BMP180 (GY-68)

//DeviceAddress Probe01 = { 0x28, 0xBE, 0xFA, 0x3B, 0x07, 0x00, 0x00, 0xB7 };  // DS18S20 #1
//DeviceAddress Probe02 = { 0x28, 0x9A, 0xF1, 0x3C, 0x07, 0x00, 0x00, 0x2B };  // DS18S20 #2
DeviceAddress Probe03 = { 0x28, 0x78, 0xB2, 0x3B, 0x07, 0x00, 0x00, 0x4D }; // DS18S20 #3

byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 78);
IPAddress myDns(192,168,1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);


// telnet defaults to port 23  ****************************************************************
EthernetServer server(23);
boolean alreadyConnected = false; // whether or not the client was connected previously


// thingSpeak *********************************************************************************
bool   thingSpeakActive    = 1;  // 1 = upload sensor data to ThingSpeak channel. 0 = don't connect to ThingSpeak
char   thingSpeakAddress[] = "api.thingspeak.com";
String writeAPIKey         = "2Q3VXKLFOAVBNTZA";
bool   lastTSconnectSuccessful = 0;

void setup() {
  // initialize the ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  // start listening for clients
  server.begin();
  // Open serial communications and wait for port to open:
  //Serial.begin(9600);
  //while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  //}

  Serial.print("Chat server address:");
  Serial.println(Ethernet.localIP());

  sensors.begin();    // start up temperature sensor
  sensors.setResolution(Probe03, 10);

  bmp.begin();   // start up pressure sensor

  delay(1000); // initial start-up time to settle hardware etc
  
}

void loop() {

  // timer function
  if ((unsigned long)(millis() - previousMillis) >= interval){
    previousMillis = millis();
    updateCounter++;
    
    sensors.requestTemperatures();
    
    temperature = sensors.getTempC(Probe03);
    pressure    = 0.01*bmp.readPressure();  // convert to mBar

    if(thingSpeakActive)
    {
      //updateThingSpeak("field1="+tempStr1+"&field2="+tempStr2);
      String tsStr = String("field1=") + temperature + String("&field2=") + pressure + String("&field3=") + humidity;
      updateThingSpeak(tsStr);
    }
  }

    
  // check if there is a Telnet connection and parse commands
  parseTelnetCommand();
}









// parse telnet commands if any available
bool parseTelnetCommand()
{
  String command;
  EthernetClient client = server.available();

  // when the client sends the first byte, say hello:
  if (client) {
    if (!alreadyConnected) {
      // clear out the input buffer:
      client.flush();
      Serial.println("client connected");
      client.println("client connected");
      alreadyConnected = true;
    }

    while(client.available() > 0) {
      // read the bytes incoming from the client:
      char c = client.read();
      command.concat(c);  // append character to string
    }
    
    command.replace(" ", "");  // clean string from spaces
    
    if(command.startsWith("set")){
      command.remove(0,3); // remove order type to allow variables named set or get
      
      // set through possible commands
      if(command.startsWith("interval=")){
        command.remove(0,9); // remove command
        int val = command.toInt();  // read following statement as integer
        if(val!=0){
          // set global interval to new value
          interval = val;
          client.println(String("new interval = ") + String(val) + String("msec"));
        }
      } 
    } 
    else if(command.startsWith("get")){
      command.remove(0,3); // remove order type to allow variables named set or get

      if(command.startsWith("ver")){
        client.println(String("version: ") + VER);
      } else if(command.startsWith("temperature")){
        client.println(String("temperature = ") + temperature + String("°C"));
      } else if(command.startsWith("pressure")){
        client.println(String("pressure = ") + pressure  + String("mBar"));
      } else if(command.startsWith("humidity")){
        client.println(String("humidity = ") + humidity);
      } else if(command.startsWith("sensors")){
        client.println(String("temperture\n\rhumidity\n\rpressure"));
      } else if(command.startsWith("millis")){
        client.println(String("millis() = ") + millis() + String("msec"));
      } else if(command.startsWith("interval")){
        client.println(String("interval = ") + interval + String("msec"));
      } else if(command.startsWith("counter")){
        client.println(String("updates since start = ") + updateCounter);
      } else if(command.startsWith("thingSpeakActive")){
        client.println(String("thingSpeakActive = ") + thingSpeakActive);
      } else if(command.startsWith("lastTSconnect")){
        client.println(String("lastTSconnectSuccessful = ") + lastTSconnectSuccessful); 
      }
    }
    else if(command.startsWith("help")){
      client.println("get temperature\n\rget pressure\n\rget humidity\n\rget sensors\n\rgetmillis\n\rgetinterval\n\rgetcounter\n\rget ver\n\rset interval=1000");
    }
    else {
      client.println("error: command must start with 'set' or 'get'");
    }  
  }
}





void updateThingSpeak(String tsData)
{
  EthernetClient ethernetClient;
  
  if (ethernetClient.connect(thingSpeakAddress, 80))
  {         
    ethernetClient.print("POST /update HTTP/1.1\n");
    ethernetClient.print("Host: api.thingspeak.com\n");
    ethernetClient.print("Connection: close\n");
    ethernetClient.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    ethernetClient.print("Content-Type: application/x-www-form-urlencoded\n");
    ethernetClient.print("Content-Length: ");
    ethernetClient.print(tsData.length());
    ethernetClient.print("\n\n");
    ethernetClient.print(tsData);

    Serial.println(tsData);
    lastTSconnectSuccessful = 1;
  }
  else
  {
    // connection failed
    lastTSconnectSuccessful = 0;
  }
  
  ethernetClient.stop();
}






















