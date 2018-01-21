/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* pin09-brown
* pin10-orange
* pin11-green
* pin12-yellow
* pin13-black
*/

#include <SPI.h>
#include "RF24.h"
#include "printf.h"
//
// Physical connections 
//
#define HW_RELAY1 7 // d output
#define HW_RELAY2 8 // digital output
#define HW_TEMP A5  // analog input
#define HW_LED  6   // digital output
#define HW_CSN 9    // icsp
#define HW_CE 10    // icsp
#define DEFAULT_ACTIVATION 1440 // 10h from now we activate (in case radio is down and can't program)

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/

RF24 radio(9,10);
const uint8_t addresses[][6] = {"1Node","2Node"};


short tempThreshold = -12;                  // default temperature at which we might want to start the engine block heater
unsigned long schedule1 = 0,schedule2 = 0;  // hold the amount of time in seconds until activation
bool relay1 = false, relay2 = false;        // activation state of relay number 2
const unsigned long max_duration = 6600;    // turn off after 1h of activation


void setup() 
{
  delay(1000);
  /* real setup starts */
  pinMode(HW_RELAY1, OUTPUT);
  pinMode(HW_RELAY2, OUTPUT);
  /*a little something to tell we're alive*/
  for (int ii = 0; ii<= 5; ii++) 
  {  
    digitalWrite(HW_RELAY1, relay1);
    digitalWrite(HW_RELAY2, relay2);
    delay(250);
    relay2 = !relay2;
    relay1 = !relay1;
  }
  delay(1000);
  digitalWrite(HW_RELAY1, LOW);
  digitalWrite(HW_RELAY2, LOW);
  delay(1000);
  digitalWrite(HW_RELAY1, HIGH);
  digitalWrite(HW_RELAY2, HIGH);
  
  Serial.begin(115200);
  Serial.println(F("RF24 Slave - power socket controller -"));
  
  radio.begin();
  radio.setCRCLength( RF24_CRC_16 ) ;
  radio.setRetries( 15, 5 ) ;
  radio.setAutoAck( true ) ;
  radio.setPALevel( RF24_PA_MAX ) ;
  radio.setDataRate( RF24_250KBPS ) ;
  radio.setChannel( 108 ) ;
  radio.enableDynamicPayloads(); //dont work with RPi :-/
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1,addresses[0]);

  /*delay(1000);
  delay(1000);
  delay(1000);*/
  // fun
  printf_begin();
  radio.printDetails();
  
  // Start the radio listening for data
  radio.startListening();
  
  schedule1 = DEFAULT_ACTIVATION;
  schedule2 = DEFAULT_ACTIVATION;
}

void loop() 
{   
  if( radio.available())
  {
    //Serial.println("Radio is available");
    // Dump the payloads until we've gotten everything
    bool done = false;
    uint8_t len = 0;
    String s1;
    while (radio.available()) 
    {
      
      // Fetch the payload, and see if this was the last one.
      len = radio.getDynamicPayloadSize();
      char* rx_data = NULL;
      rx_data = (char*)calloc(len+1, sizeof(char));
      if (rx_data == NULL)
      {
        //Serial.println("Cannot allocate enough memory to read payload");
        break;
      }
      radio.read( rx_data, len );
      
        // Put a zero at the end for easy printing
      rx_data[len+1] = 0;
      
        // Spew it
      /*Serial.print(F("Got msg size="));
      Serial.print(len);
      Serial.print(F(" value="));
      Serial.println(rx_data);*/
      
      s1 = String(rx_data);
      free(rx_data);
    }

    if (s1.indexOf("activate")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      int i1 = i1 = sched.substring(0).toInt();
      if (s1.indexOf("activate1")>=0)
      {
        schedule1 = i1 + (millis()/60000);
      }
      else if (s1.indexOf("activate2")>=0)
      {
        schedule2 = i1 + (millis()/60000);
      }
      /*Serial.print("Starting1 in ");
      Serial.print(schedule1);
      Serial.print(" Starting2 in ");
      Serial.print(schedule2);
      Serial.print(" +  ");
      Serial.println(millis()/1000);*/
    }
    else if (s1.indexOf("stop")>=0)
    {
      relay1 = false;
      relay2 = false;
      digitalWrite(HW_RELAY1, HIGH);
      digitalWrite(HW_RELAY2, HIGH);
      schedule1 = 0;
      schedule2 = 0;
      s1 = "stopped";
      //Serial.println("Stopped");
    }
    else if (s1.indexOf("status")>=0) 
    {
      char buffer[6];  //buffer used to format a line (+1 is for trailing 0)
      sprintf(buffer,"%dC",readTemperature());   
      s1 = "Up:" + String(millis()/60000) + 
        String(" ") + String((relay1?"ON":"OFF")) + 
        String(" ") + String((relay2?"ON":"OFF")) + 
        String(" ") + String(schedule1)  + 
        String(" ") + String(schedule2)  + 
        String(" ") + String(buffer) + String("C");
      /*Serial.print("Status ");
      Serial.println(s1);*/
    }
    else
      s1 = "You fail.";

    // convert the response string into a char[]
    len = s1.length();
    char rt_data[len];
    s1.toCharArray(rt_data, len);
    
    // First, stop listening so we can talk
    radio.stopListening();
    // Send the final one back.
    radio.write( rt_data, len );
    Serial.println("Sent response: "+s1);

    // Now, resume listening so we catch the next packets.
    radio.startListening();
  }




  
  // is it cold enough to turn justify heating the engine...
  if ( readTemperature() < tempThreshold )
  {
    if ( millis()/60000 > schedule1  && schedule1 > 0 && relay1 == false )
    {
      digitalWrite(HW_RELAY1, LOW);
      relay1 = true;
    }
    if ( millis()/60000 > schedule2  && schedule2 > 0 && relay2 == false )
    {
      digitalWrite(HW_RELAY2, LOW);
      relay2 = true;
    }
  }
    
  // switch relays off after max_duration
  if ( schedule1 > 0 && (millis()/60000) > (unsigned long)schedule1+max_duration )
  { 
    //automatically schedule relay1 to tomorrow
    schedule1 += (unsigned long)24*(unsigned long)3660; 
    digitalWrite(HW_RELAY1, HIGH);
    relay1 = false;
  }
  if ( schedule2 > 0 && (millis()/60000) > (unsigned long)schedule2+max_duration )
  { 
    //automatically schedule relay2 to tomorrow
    schedule2 += (unsigned long)24*(unsigned long)3600; 
    digitalWrite(HW_RELAY2, HIGH);
    relay2 = false;
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
  return celsius;
}



