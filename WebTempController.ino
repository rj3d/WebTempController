/*
  Web Temperature Controller
 
 A webserver adapted from the Arduino webserver example.
 Currently displays the temperature from a OneWire temperature sensor.
 Will eventually take a set temperature from a web form and control
 two relays to turn on/off heating and cooling devices.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * OneWire temperature sensor on digital pin 2
 
 
 */

#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TextFinder.h>


// OneWire/DallasTemp setup stuff
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(88);

#define COOL_PIN 8
#define HEAT_PIN 9
struct ControllerData {
  float set_temp;
  float sensitivity;
  unsigned long min_interval; //Minimum cycle interval in miliseconds
  unsigned long last_heat;
  boolean heating;
  unsigned long last_cool;
  boolean cooling;
};

ControllerData cd;

void initControllerData(){
  cd.set_temp = 68.0;
  cd.sensitivity = 1.0;
  cd.min_interval = 60000;
  cd.last_heat = 0;
  cd.heating = false;
  cd.last_cool = 0;
  cd.cooling = false;
}

void updateControllerData(EthernetClient client){
  TextFinder finder(client);
    if(finder.find("set_temp=")){
      cd.set_temp = finder.getFloat();
    }
};

void checkCorrectOverflow(unsigned long time, unsigned long &last){
  unsigned long delta = time - last;
  if(delta<0){
    last = time;
  }
}

void turnOnHeating(){
  unsigned long time = millis();
  turnOffCooling();
  checkCorrectOverflow(time, cd.last_heat);
  if(time - cd.last_heat > cd.min_interval){
    digitalWrite(HEAT_PIN, HIGH);
    cd.heating = true;
  }
};      

void turnOffHeating(){
  if(cd.heating){
    digitalWrite(HEAT_PIN, LOW);
    cd.heating = false;
    cd.last_heat = millis();
  }
};

void turnOnCooling(){
  unsigned long time = millis();
  turnOffHeating();
  checkCorrectOverflow(time, cd.last_cool);
  if(time - cd.last_cool > cd.min_interval){
    digitalWrite(COOL_PIN, HIGH);
    cd.cooling = true;
  }
};      

void turnOffCooling(){
  if(cd.cooling){
    digitalWrite(COOL_PIN, LOW);
    cd.cooling = false;
    cd.last_cool = millis();
  }
};

void updateRelays(){
     sensors.requestTemperatures();
     float cur_temp = sensors.getTempFByIndex(0);
     float delta = cur_temp - cd.set_temp;
     //If the temp is out of range
     if (abs(delta) > cd.sensitivity){
       if(delta > 0){
         turnOnCooling();
       }
       else if(delta < 0){
         turnOnHeating();
       }
     }
     //If the temp is in range
     else {
       turnOffHeating();
       turnOffCooling();
     }
};

void outputHeader(EthernetClient client){
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
};

void outputData(EthernetClient client){
     sensors.requestTemperatures();
     float temp = sensors.getTempFByIndex(0);
     client.print("Current temperature is ");
     client.print(temp);
     client.println(" F <br/>");
     client.print("Set temperature is "); 
     client.print(cd.set_temp);
     client.println(" F <br/> <br/>");
}

void outputRelayStatus(EthernetClient client){
  if(cd.heating){
    client.print("Currently heating <br/><br/>");
  }
  if(cd.cooling){
    client.print("Currently cooling <br/><br/>");
  }
  if(!(cd.cooling || cd.heating)){
    client.print("No temp control active <br/><br/>");
  }  
};

void outputForm(EthernetClient client){
    client.println("<FORM ACTION=\"http://192.168.1.177:88\" METHOD=\"post\">");
    client.println("Temp Set: <INPUT TYPE=\"TEXT\" NAME=\"set_temp\" VALUE=\"\" SIZE=\"4\" MAXLENGTH=\"5\"><BR>");
    client.println("<INPUT TYPE=\"SUBMIT\" NAME=\"submit\" VALUE=\"Submit\">");
    client.println("</FORM>");
    client.println("<BR>");
}

void outputFooter(EthernetClient client){
  client.println("</html>");
}

void setup() {
  //Start the serial port
  Serial.begin(9600);
  //tart the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  // Start up the temp sensors
  sensors.begin();
  //Initialize the ControllerData
  initControllerData();  
  //Initialize relay pins as outputs
  pinMode(COOL_PIN, OUTPUT);
  pinMode(HEAT_PIN, OUTPUT);
}

void loop() {
  // set the relay statuses
  updateRelays();
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        
        if (c == '\n' && currentLineIsBlank) {
          // Use POST data to update ControllerData and update the relays accordingly.
          updateControllerData(client);
          updateRelays();
          //Output HTML to client
          outputHeader(client);
          outputData(client);
          outputRelayStatus(client);  
          outputForm(client); 
          outputFooter(client);       
          break;
        }
        
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
}


