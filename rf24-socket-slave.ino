/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* Updated: Dec 2014 by TMRh20
*/

#include <SPI.h>
#include "RF24.h"
#include "printf.h"
#include <avr/wdt.h>
//
// Physical connections 
//
#define HW_LED    6     // digital output
#define HW_RELAY1 7     // d output
#define HW_RELAY2 8     // digital output
#define HW_CSN    10    // icsp
#define HW_CE     9     // icsp
#define HW_TEMP  A5     // analog input
// 
// SW Logic and firmware definitions
// 
#define THIS_NODE_ID 123                // master is 0, unoR3 debugger is 1, promicro_arrosoir is 2, etc
#define DEFAULT_ACTIVATION 600          // 10h from now we activate (in case radio is down and can't program)
#define DEFAULT_DURATION 10             // max 10s of activation time by default
#define BUZZER_ACTIVATIN_INTERVAL 300;  // buzz every 5h when sth is wrong

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
RF24 radio(HW_CE,HW_CSN);

// Radio pipe addresses for the nodes to communicate.
// WARNING!! 3Node and 4Node are used by my testing sketches ping/pong
const uint8_t addresses[][5] = {
  "0Node", // master writes broadcasts here
  "1Node", // unor3 writes here
  "2Node", // unor3 reads here
  "3Node", // arrosoir reads here
  "4Node", // arrosoir writes here
  "5Node"};// not yet used by anybody


/**
 * exchange data via radio more efficiently with data structures.
 * we can exchange max 32 bytes of data per msg. 
 * schedules are reset every 24h (last for a day) so an INTEGER is
 * large enough to store the maximal value of a 24h-schedule.
 * temperature threshold is rarely used
 */
struct relayctl {
  unsigned long uptime = 0;                      // current running time of the machine (millis())  4 bytes  
  unsigned long sched1 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr1   4 bytes
  unsigned long sched2 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr2   4 bytes
  unsigned int  maxdur1 = DEFAULT_DURATION;      // max duration nbr1 is ON                         2 bytes
  unsigned int  maxdur2 = DEFAULT_DURATION;      // max duration nbr2 is ON                         2 bytes
  unsigned int  temp_thres = 0;                  // temperature at which the system is operational  4 bytes
  float         temp_now   = 20;                 // current temperature read on the sensor          4 bytes
  short         battery    =  0;                 // current temperature read on the sensor          2 bytes
  bool          state1 = false;                  // state of relay output 1                         1 byte
  bool          state2 = false;                  // "" 2                                            1 byte
  bool          waterlow = false;                // indicates whether water is low                  1 byte
  byte          nodeid = THIS_NODE_ID;           // nodeid is the identifier of the slave           1 byte
} myData;

void setup() 
{

  /* real setup starts */
  pinMode(HW_RELAY1, OUTPUT);
  pinMode(HW_RELAY2, OUTPUT);
  digitalWrite(HW_RELAY1, HIGH);
  digitalWrite(HW_RELAY2, HIGH);

#if defined(ARDUINO_AVR_LEONARDO) 
  /*a little something to tell we're alive*/
  for (int ii = 0; ii<= 5; ii++) 
  {  
    /*blinks the LEDS on the micro*/
    RXLED1;
    TXLED0; //TX LED is not tied to a normally controlled pin
    delay(500);              // wait for a second
    TXLED1;
    RXLED0;
    delay(500);              // wait for a second
  }
  TXLED0; 
  RXLED0;
#endif
  
  //
  // Print preamble
  //
  Serial.begin(115200);
  delay(500);
  Serial.println(F("RF24 Slave - power socket controller"));  
  delay(500);
  Serial.println(F("Warning! Always query the controller before attempting to program it!"));  
  delay(500);
  Serial.println(F("- - - - -"));  

  // using watchdog timer to signal this sketch is alive and well.
  // prevent getting hung for some reason
  wdt_enable(WDTO_1S); 
   
  radio.begin();
  radio.setCRCLength( RF24_CRC_16 ) ;
  radio.setRetries( 15, 5 ) ;
  radio.setAutoAck( true ) ;
  radio.setPALevel( RF24_PA_MAX ) ;
  radio.setDataRate( RF24_250KBPS ) ;
  radio.setChannel( 108 ) ;
  radio.enableDynamicPayloads(); //dont work with my modules :-/
  
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1,addresses[0]);
  radio.openReadingPipe(2,addresses[2]);

  // fun
  printf_begin();
  Serial.println(F("Radio setup:"));  
  radio.printDetails();
  Serial.println(F("- - - - -"));  
  
  // Start the radio listening for data
  radio.powerUp();
  radio.write( &myData, sizeof(myData) ); 
  radio.startListening();
}

void loop() 
{   
  wdt_reset();
  if( radio.available())
  {
    bool done = false;
    uint8_t len = 0;
    String s1;
    
    while (radio.available()) 
    {
      len = radio.getDynamicPayloadSize();
      if ( len == sizeof(relayctl) )
      {
        radio.read( &myData, len );
      }
      else
      {
        char* rx_data = NULL;
        rx_data = (char*)calloc(len+1, sizeof(char));
        if (rx_data == NULL)
        {
          Serial.println("Cannot allocate enough memory to read payload");
          break;
        }
        radio.read( rx_data, len );
      
        // Put a zero at the end for easy printing
        rx_data[len+1] = 0;
      
          // Spew it
        Serial.print(F("Got msg size="));
        Serial.print(len);
        Serial.print(F(" value="));
        Serial.println(rx_data);
      
        s1 = String(rx_data);
        free(rx_data);
        rx_data = NULL;
      }
    }
    
    myData.uptime = millis() / 60000;
    
    if (s1.indexOf("stop")>=0)
    {
      myData.state1 = false;
      myData.state2 = false;
      myData.sched1 = 0;
      myData.sched2 = 0;
      s1 = "stopped OK";
    }
    else 
    {
      Serial.print("Sending out status ");
      Serial.println(sizeof(myData));
      
      radio.stopListening();
      radio.write( &myData, sizeof(myData) ); 
      radio.startListening();
    }
  }




  
  // is it cold enough to turn justify heating the engine...
  if ( readTemperature() < myData.temp_thres )
  {
    if ( millis()/60000 >= myData.sched1  && myData.sched1 > 0  )
    {
      myData.state1 = true;
      
      radio.stopListening();
      radio.write( &myData, sizeof(myData) ); 
      radio.startListening();
    }
    if ( millis()/60000 >= myData.sched2  && myData.sched2 > 0  )
    {
      myData.state2 = true;
        
      radio.stopListening();
      radio.write( &myData, sizeof(myData) ); 
      radio.startListening();
    }
  }
    
  // switch relays off after max_duration, & sendout new status
  // WARNING: ===> calculate in seconds coz duration is in secs
  if ( myData.sched1 > 0 && millis()/1000 > (myData.sched1*60)+myData.maxdur1 )
  { 
    myData.state1 = false;
    //automatically schedule relay1 to tomorrow
    myData.sched1 += 1440; 
    
    radio.stopListening();
    radio.write( &myData, sizeof(myData) ); 
    radio.startListening();
  }
  if ( myData.sched2 > 0 && millis()/1000 > (myData.sched2*60)+myData.maxdur2 )
  { 
    myData.state2 = false;
    //automatically schedule relay2 to tomorrow
    myData.sched2 += 1400; 
    
    radio.stopListening();
    radio.write( &myData, sizeof(myData) ); 
    radio.startListening();
  }

  
  digitalWrite(HW_RELAY1, !myData.state1); // relays are npn-transistorized so have to reverse the logic
  digitalWrite(HW_RELAY2, !myData.state2); // of my program to de/activate each channel
  
  if (Serial.available())
  {
    Serial.readString();
    Serial.println(F("Radio setup:"));  
    radio.printDetails();
    Serial.println(F("- - - - -"));  
    bool goodSignal = radio.testRPD();
    Serial.println(goodSignal ? "Strong signal > 64dBm" : "Weak signal < 64dBm" );
    Serial.println(F("- - - - -"));  
    Serial.print("Plug 1: ");
    Serial.print(myData.sched1);
    Serial.print("min, during ");
    Serial.print(myData.maxdur1);
    Serial.print("s(currently ");
    Serial.print(myData.state1);
    Serial.print(")\nPlug 2: ");
    Serial.print(myData.sched2);
    Serial.print("min, during ");
    Serial.print(myData.maxdur2);
    Serial.print("s(currently ");
    Serial.print(myData.state2);
    Serial.print(")\nTemperature: ");
    Serial.print(myData.temp_now);
    Serial.print("/");
    Serial.print(myData.temp_thres);
    Serial.print("\nUptime: ");
    Serial.print(myData.uptime);
    Serial.print("min\nBattery:");
    Serial.print(myData.battery);
    Serial.print("V\nWaterLow:");
    Serial.print(myData.waterlow);
    Serial.println();
    
    Serial.println("RF24-BLOB-BEGIN");
    Serial.write((uint8_t *)&myData, sizeof(myData));
    Serial.println();
    
    radio.stopListening();
    radio.write( &myData, sizeof(myData) ); 
    radio.startListening();
    
  }
} // Loop




// Read analog pin to get value from temp sensor
int readTemperature()
{
  int value = analogRead(HW_TEMP);

  float millivolts = (value / 1024.0) * 5000;
  float celsius = millivolts / 10;  // sensor output is 10mV per degree Celsius
  /*Serial.print(celsius);
  Serial.println(" degrees Celsius, ");
  
  Serial.print( (celsius * 9)/ 5 + 32 );  //  converts celsius to fahrenheit
  Serial.print(" degrees Fahrenheit, ");
  
  Serial.print("A/D value = "); Serial.println(value); 
  */
  myData.temp_now = celsius;
  return celsius;
}




