#include "A6ESP32.h"

#define SERIAL1_RXPIN 33 
#define SERIAL1_TXPIN 4
#define UART_NUN      1

// Instantiate the library
A6ESP32 A6_GSM(UART_NUN);

void setup() {
    Serial.begin(115200);
    
    // If you need to changhe A6 module baud rate (default 115200)
    A6_GSM.setBaudRate(9600, SERIAL1_RXPIN, SERIAL1_TXPIN);
    // If functionality of A& module is mandatory you can use this
    // A6_GSM.blockUntilReady(115200, SERIAL1_RXPIN, SERIAL1_TXPIN);
    // Otherwise you can simple    
    A6_GSM.begin(9600, SERIAL1_RXPIN, SERIAL1_TXPIN);   
       
}



void loop() {
  
  // Send AT command from terminal ()
  String inputString = "";
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();    
    inputString += inChar;
    if (inChar == '\n') {
      A6_GSM.sendATString(inputString.c_str());
    }
  }
  
}
