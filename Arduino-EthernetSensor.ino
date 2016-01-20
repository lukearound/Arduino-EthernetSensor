// GitHub home of this Sketch: https://github.com/lukearound/Arduino-EthernetSensor
//
// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// 


#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DallasTemperature.h>
#include "Adafruit_BMP085.h"     // for pressure sensor
#include "DHT.h"

#define DHTPIN 2     // what digital pin we're connected to
#define DHTTYPE DHT11   // DHT 11
#define VER "1.0 - 11.01.2016"
#define TEMP_PIN 7               // OneWire Pin zu Sensoren (im Moment nur einer angeschlossen)

const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 1023;  // valid for Arduino UNO

unsigned long interval = 81000L;  // milli seconds
unsigned long updateCounter = 0L;
unsigned long previousMillis = 0L;
float temperature = -127;
float pressure = 0;
float humidity = 0;


// EEPROM address room
int apiKeyAddr          = 0;  // String : length,char_array [int, char, char, ...]
int thingSpeakActiveAddr = 20; // bool (1 byte?)
int intervalAddr        = 22;  // unsigned long  (4 byte)


// sensors ***************************************************************************
OneWire            ds(TEMP_PIN);  // DS18S20 Temperaturchip i/o an pin 10
DallasTemperature  sensors(&ds);
Adafruit_BMP085    bmp;  // pressure sensor BMP180 (GY-68)

//DeviceAddress Probe01 = { 0x28, 0xBE, 0xFA, 0x3B, 0x07, 0x00, 0x00, 0xB7 };  // DS18S20 #1
//DeviceAddress Probe02 = { 0x28, 0x9A, 0xF1, 0x3C, 0x07, 0x00, 0x00, 0x2B };  // DS18S20 #2
DeviceAddress Probe03 = { 0x28, 0x78, 0xB2, 0x3B, 0x07, 0x00, 0x00, 0x4D }; // DS18S20 #3

DHT dht(DHTPIN, DHTTYPE);   // humidity sensor


// Ethernet **************************************************************************
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xE1 };
IPAddress ip(192, 168, 1, 201);
IPAddress myDns(192,168,1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);


// telnet defaults to port 23  ****************************************************************
EthernetServer server(23);  // port 23 for TelNet
boolean alreadyConnected = false; // whether or not the telnet client was connected previously


// thingSpeak *********************************************************************************
bool   thingSpeakActive    = 1;  // 1 = upload sensor data to ThingSpeak channel. 0 = don't connect to ThingSpeak
char   thingSpeakAddress[] = "api.thingspeak.com";
String writeAPIKey         = "Read from EEPROM";   // https://thingspeak.com/channels/79359
bool   lastTSconnectSuccessful = 0;
long   failedConnectionCount = 0;
int    resetCounter = 0;   // after 5 failed attempts Ethernet will be restarted
EthernetClient ethernetClient;


// *********************************************************************************************
void setup() {
  // disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  // read states from EEPROM
  int len = 0;  // is used for string length in EEPROM. be aware that the code converts it to byte(len)
                // to use one byte in memory only! That limits the max value to 255 instead of real int.
  
  // use this code to initialize the EEPROM if there has not been written any persisitent data to it.
  //EEPROM.put(apiKeyAddr,byte(17));
  //write_StringEE(apiKeyAddr+1, String("1234567890123456"));
  //EEPROM.put(thingSpeakActiveAddr, thingSpeakActive);
  //EEPROM_writelong(intervalAddr, interval);
  

  
  len              = int(EEPROM.read(apiKeyAddr));
  writeAPIKey      = read_StringEE(apiKeyAddr+1, len);
  thingSpeakActive = EEPROM.read(thingSpeakActiveAddr);
  interval         = EEPROM_readlong(intervalAddr);
  
  // initialize the ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  delay(1000);  // allow some start-up time for ethernet shield
  
  // start listening for TelNet clients
  server.begin(); // TelNet Server

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("API Key: ");
  Serial.println(writeAPIKey);
  Serial.print("thingSpeakActive: ");
  Serial.println(thingSpeakActive);
  Serial.print("interval: ");
  Serial.println(interval);


  sensors.begin();    // start up temperature sensor
  sensors.setResolution(Probe03, 10);

  bmp.begin();   // start up pressure sensor
  dht.begin();   // start up humidity sensor

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
    humidity = dht.readHumidity();

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
      client.println("client connected");
      alreadyConnected = true;
    }

    // read the bytes incoming from the client:
    while(client.available() > 0) {
      char c = client.read();
      command.concat(c);  // append character to string
    }

    // interpret incoming data
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
      } else if(command.startsWith("failedConnectionCount")){
        client.println(String("failedConnectionCount = ") + failedConnectionCount); 
      } else if(command.startsWith("ip")){
        IPAddress address = Ethernet.localIP();
        String printStr = String(address[0]) + "." + String(address[1]) + "." + String(address[2]) + "." + String(address[3]);
        client.println(String("IP address = ") + printStr); 
      }
    }
    else if(command.startsWith("help")){
      client.println("get temperature\n\rget pressure\n\rget humidity\n\rget sensors\n\rgetmillis\n\rgetinterval\n\rgetcounter\n\rget ver\n\rset interval=1000");
    } else if(command.length() == 0){
      // do nothing;
    }
    else {
      client.println("error: command must start with 'set' or 'get'");
    }  
  }
}





void updateThingSpeak(String tsData)
{
  ethernetClient.stop(); // free socket
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

    //Serial.println(tsData);
    lastTSconnectSuccessful = 1;
    resetCounter = 0;
  }
  else
  {
    // connection failed
    lastTSconnectSuccessful = 0;
    failedConnectionCount++;
    resetCounter++;
    if(resetCounter>=5){
      restartEthernet();
      delay(1000);  // wait a moment to have ethernet shield start up
      resetCounter = 0;
    }
  }
  
  ethernetClient.stop();
}


void restartEthernet()
{
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
}



// write String to EEPROM. Use read_StringEE to read it
// Convention: The 
bool write_StringEE(int Addr, String input)
{
    char cbuff[input.length()+1];//Finds length of string to make a buffer
    input.toCharArray(cbuff,input.length()+1);//Converts String into character array
    return eeprom_write_string(Addr,cbuff);//Saves String
}


String read_StringEE(int Addr,int length)
{
  String stemp="";
  char cbuff[length];
  eeprom_read_string(Addr,cbuff,length);
  for(int i=0;i<length-1;i++)
  {
    stemp.concat(cbuff[i]);//combines characters into a String
    delay(100);
  }
  return stemp;
}


// Writes a string starting at the specified address.
// Returns true if the whole string is successfully written.
// Returns false if the address of one or more bytes fall outside the allowed range.
// If false is returned, nothing gets written to the eeprom.
boolean eeprom_write_string(int addr, const char* string) {
 
  int numBytes; // actual number of bytes to be written
 
  //write the string contents plus the string terminator byte (0x00)
  numBytes = strlen(string) + 1;
 
  return eeprom_write_bytes(addr, (const byte*)string, numBytes);
}


// Writes a sequence of bytes to eeprom starting at the specified address.
// Returns true if the whole array is successfully written.
// Returns false if the start or end addresses aren't between
// the minimum and maximum allowed values.
// When returning false, nothing gets written to eeprom.
boolean eeprom_write_bytes(int startAddr, const byte* array, int numBytes) {
  // counter
  int i;
 
  // both first byte and last byte addresses must fall within
  // the allowed range
  if (!eeprom_is_addr_ok(startAddr) || !eeprom_is_addr_ok(startAddr + numBytes)) {
    return false;
  }
 
  for (i = 0; i < numBytes; i++) {
    EEPROM.write(startAddr + i, array[i]);
  }
 
  return true;
}


// Reads a string starting from the specified address.
// Returns true if at least one byte (even only the string terminator one) is read.
// Returns false if the start address falls outside the allowed range or declare buffer size is zero.
//
// The reading might stop for several reasons:
// - no more space in the provided buffer
// - last eeprom address reached
// - string terminator byte (0x00) encountered.
boolean eeprom_read_string(int addr, char* buffer, int bufSize) {
  byte ch; // byte read from eeprom
  int bytesRead; // number of bytes read so far
 
  if (!eeprom_is_addr_ok(addr)) { // check start address
    return false;
  }
 
  if (bufSize == 0) { // how can we store bytes in an empty buffer ?
    return false;
  }
 
  // is there is room for the string terminator only, no reason to go further
  if (bufSize == 1) {
    buffer[0] = 0;
    return true;
  }
 
  bytesRead = 0; // initialize byte counter
  ch = EEPROM.read(addr + bytesRead); // read next byte from eeprom
  buffer[bytesRead] = ch; // store it into the user buffer
  bytesRead++; // increment byte counter
 
  // stop conditions:
  // - the character just read is the string terminator one (0x00)
  // - we have filled the user buffer
  // - we have reached the last eeprom address
  while ( (ch != 0x00) && (bytesRead < bufSize) && ((addr + bytesRead) <= EEPROM_MAX_ADDR) ) {
    // if no stop condition is met, read the next byte from eeprom
    ch = EEPROM.read(addr + bytesRead);
    buffer[bytesRead] = ch; // store it into the user buffer
    bytesRead++; // increment byte counter
  }
 
  // make sure the user buffer has a string terminator, (0x00) as its last byte
  if ((ch != 0x00) && (bytesRead >= 1)) {
    buffer[bytesRead - 1] = 0;
  }
 
  return true;
}

// Returns true if the address is between the
// minimum and maximum allowed values, false otherwise.
//
// This function is used by the other, higher-level functions
// to prevent bugs and runtime errors due to invalid addresses.
boolean eeprom_is_addr_ok(int addr) {
  return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}





 // read double word from EEPROM, give starting address
 unsigned long EEPROM_readlong(int address) {
 //use word read function for reading upper part
 unsigned long dword = EEPROM_readint(address);
 //shift read word up
 dword = dword << 16;
 // read lower word from EEPROM and OR it into double word
 dword = dword | EEPROM_readint(address+2);
 return dword;
}

//write word to EEPROM
 void EEPROM_writeint(int address, int value) {
 EEPROM.write(address,highByte(value));
 EEPROM.write(address+1 ,lowByte(value));
}
 
 //write long integer into EEPROM
 void EEPROM_writelong(int address, unsigned long value) {
 //truncate upper part and write lower part into EEPROM
 EEPROM_writeint(address+2, word(value));
 //shift upper part down
 value = value >> 16;
 //truncate and write
 EEPROM_writeint(address, word(value));
}

unsigned int EEPROM_readint(int address) {
 unsigned int word = word(EEPROM.read(address), EEPROM.read(address+1));
 return word;
}







