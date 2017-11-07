#include "A6ESP32.h"
#include <Arduino.h>

/////////////////////////////////////////////
// Public methods.

#define RETRY 1


A6ESP32::A6ESP32(const byte uart) {
	A6conn = new HardwareSerial(uart);
}


A6ESP32::~A6ESP32() {
    delete A6conn;
}


// Block until the module is ready.
byte A6ESP32::blockUntilReady(long baudRate, byte rxPin, byte txPin) {

    byte response = A6_NOTOK;
    while (A6_OK != response) {
        response = begin(baudRate, rxPin, txPin);
        // This means the modem has failed to initialize and we need to reboot it.
        if (A6_FAILURE == response) {
            return A6_FAILURE;
        }
        delay(1000);
        logln("Waiting for module to be ready...");
    }
    return A6_OK;
}

// Initialize the hardware serial connection 
byte A6ESP32::begin(long baudRate, byte rxPin, byte txPin) {
    
    A6conn->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    if (A6command("\rAT", "OK", "+CME", 2000, 2, NULL, false) != A6_OK) {                   
       return A6_NOTOK;
    }
    
    // Factory reset.
    A6command("AT&F", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);

    // Echo off.
    A6command("ATE0", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);

    // Switch audio to headset.
    enableSpeaker(0);

    // Set caller ID on.
    A6command("AT+CLIP=1", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);

    // Set SMS to text mode.
    A6command("AT+CMGF=1", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);

    // Turn SMS indicators off.
    A6command("AT+CNMI=1,0", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);

    // Set SMS storage to the GSM modem.
    if (A6_OK != A6command("AT+CPMS=ME,ME,ME", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false))
        // This may sometimes fail, in which case the modem needs to be
        // rebooted.
    {
        return A6_FAILURE;
    }

    // Set SMS character set.
    setSMScharset("UCS2");

    return A6_OK;
}


// Reboot the module by setting the specified pin HIGH, then LOW. The pin should
// be connected to a P-MOSFET, not the A6's POWER pin.
void A6ESP32::powerCycle(int pin) {
    logln("Power-cycling module...");
    powerOff(pin);
    delay(2000);
    powerOn(pin);
    // Give the module some time to settle.
    logln("Done, waiting for the module to initialize...");
    delay(20000);
    logln("Done.");
}


// Turn the modem power completely off.
void A6ESP32::powerOff(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}


// Turn the modem power on.
void A6ESP32::powerOn(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}


// Dial a number.
void A6ESP32::dial(String number) {
    char buffer[50];
    logln("Dialing number...");
    sprintf(buffer, "ATD%s;", number.c_str());
    A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Redial the last number.
void A6ESP32::redial() {
    logln("Redialing last number...");
    A6command("AT+DLST", "OK", "CONNECT", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Answer a call.
void A6ESP32::answer() {
    A6command("ATA", "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Hang up the phone.
void A6ESP32::hangUp() {
    A6command("ATH", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL, false);
}


// Check whether there is an active call.
callInfo A6ESP32::checkCallStatus() {
    char number[50];
    String response = "";
    long respStart = 0;
    callInfo cinfo;

    // Issue the command and wait for the response.
    A6command("AT+CLCC", "OK", "+CLCC", A6_CMD_TIMEOUT, RETRY, &response, false);

    // Parse the response if it contains a valid +CLCC.
    respStart = response.indexOf("+CLCC");
    if (respStart >= 0) {
        sscanf(response.substring(respStart).c_str(), "+CLCC: %d,%d,%d,%d,%d,\"%s\",%d", 
		(int*)&cinfo.index, 
		(int*)&cinfo.direction, 
		(int*)&cinfo.state, 
		(int*)&cinfo.mode, 
		(int*)&cinfo.multiparty, 
		number, 
		(int*)&cinfo.type);
        cinfo.number = String(number);
    }

    int comma_index = cinfo.number.indexOf('"');
    if (comma_index != -1) {
        logln("Extra comma found.");
        cinfo.number = cinfo.number.substring(0, comma_index);
    }

    return cinfo;
}


// Get the strength of the GSM signal.
int A6ESP32::getSignalStrength() {
    String response = "";
    long respStart = 0;
    int strength, error  = 0;

    // Issue the command and wait for the response.
    A6command("AT+CSQ", "OK", "+CSQ", A6_CMD_TIMEOUT, RETRY, &response, false);

    respStart = response.indexOf("+CSQ");
    if (respStart < 0) {
        return 0;
    }

    sscanf(response.substring(respStart).c_str(), "+CSQ: %d,%d", &strength, &error);

    // Bring value range 0..31 to 0..100%, don't mind rounding..
    strength = (strength * 100) / 31;
    return strength;
}


// Send an SMS.
byte A6ESP32::sendSMS(String number, String text) {
    char ctrlZ[2] = { 0x1a, 0x00 };
    char buffer[100];

    if (text.length() > 159) {
        // We can't send messages longer than 160 characters.
        return A6_NOTOK;
    }

    log("Sending SMS to ");
    log(number);
    logln("...");

    sprintf(buffer, "AT+CMGS=\"%s\"", number.c_str());
    A6command(buffer, ">", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
    delay(100);
    A6conn->println(text.c_str());
    A6conn->println(ctrlZ);
    A6conn->println();

    return A6_OK;
}


// Retrieve the number and locations of unread SMS messages.
int A6ESP32::getUnreadSMSLocs(int* buf, int maxItems) {
    return getSMSLocsOfType(buf, maxItems, "REC UNREAD");
}

// Retrieve the number and locations of all SMS messages.
int A6ESP32::getSMSLocs(int* buf, int maxItems) {
    return getSMSLocsOfType(buf, maxItems, "ALL");
}

// Retrieve the number and locations of all SMS messages.
int A6ESP32::getSMSLocsOfType(int* buf, int maxItems, String type) {
    String seqStart = "+CMGL: ";
    String response = "";

    String command = "AT+CMGL=\"";
    command += type;
    command += "\"";

    // Issue the command and wait for the response.
    A6command(command.c_str(), "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT, RETRY, &response, false);

    int seqStartLen = seqStart.length();
    int responseLen = response.length();
    int index, occurrences = 0;

    // Start looking for the +CMGL string.
    for (int i = 0; i < (responseLen - seqStartLen); i++) {
        // If we found a response and it's less than occurrences, add it.
        if (response.substring(i, i + seqStartLen) == seqStart && occurrences < maxItems) {
            // Parse the position out of the reply.
            sscanf(response.substring(i, i + 12).c_str(), "+CMGL: %u,%*s", &index);
            buf[occurrences] = index;
            occurrences++;
        }
    }
    return occurrences;
}

// Return the SMS at index.
SMSmessage A6ESP32::readSMS(int index) {
    String response = "";
    char buffer[30];

    // Issue the command and wait for the response.
    sprintf(buffer, "AT+CMGR=%d", index);
    A6command(buffer, "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT, RETRY, &response, false);
   
    char number[50];
    char date[50];
    char type[10];
    int respStart = 0;
    SMSmessage sms = (const struct SMSmessage) { "", "", "" };

    // Parse the response if it contains a valid +CLCC.
    respStart = response.indexOf("+CMGR");
    if (respStart >= 0) {
        // Parse the message header.
        sscanf(response.substring(respStart).c_str(), "+CMGR: \"REC %s\",\"%s\",,\"%s\"\r\n", type, number, date);
        sms.number = String(number);
        sms.date = String(date);
        // The rest is the message, extract it.
        sms.message = response.substring(strlen(type) + strlen(number) + strlen(date) + 24, response.length() - 8);
    }
    return sms;
}

// Delete the SMS at index.
byte A6ESP32::deleteSMS(int index) {
    char buffer[20];
    sprintf(buffer, "AT+CMGD=%d", index);
    return A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Set the SMS charset.
byte A6ESP32::setSMScharset(String charset) {
    char buffer[30];
    sprintf(buffer, "AT+CSCS=\"%s\"", charset.c_str());
    return A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Set the volume for the speaker. level should be a number between 5 and
// 8 inclusive.
void A6ESP32::setVol(byte level) {
    char buffer[30];

    // level should be between 5 and 8.
    level = min(max(level, 5), 8);
    sprintf(buffer, "AT+CLVL=%d", level);
    A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}


// Enable the speaker, rather than the headphones. Pass 0 to route audio through
// headphones, 1 through speaker.
void A6ESP32::enableSpeaker(byte enable) {
    char buffer[30];

    // enable should be between 0 and 1.
    enable = min(max(enable, 0), 1);
    sprintf(buffer, "AT+SNFS=%d", enable);
    A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, false);
}



byte A6ESP32::sendATString(const char *command){
  byte returnValue = A6_NOTOK;
  returnValue = A6command(command, "OK", "yy", A6_CMD_TIMEOUT, RETRY, NULL, true);
  return returnValue;
}



/////////////////////////////////////////////
// Private methods.

// Autodetect the module connection rate.

long A6ESP32::detectRate(byte rxPin, byte txPin) {
    unsigned long rate = 0;
    unsigned long rates[] = {9600, 19200, 57600, 115200};

    // Try to autodetect the rate.
    logln("Autodetecting connection rate...");
    for (int i = 0; i < countof(rates); i++) {
        rate = rates[i];

        A6conn->begin(rate, SERIAL_8N1, rxPin, txPin);
        log("Trying rate " + String(rate) + "...");

        delay(100);
        if (A6command("\rAT", "OK", "+CME", 2000, RETRY +1, NULL, false) == A6_OK) {
            return rate;
        }
    }

    logln("Couldn't detect the rate.");
    return A6_NOTOK;
}


// Set the A6 baud rate.
char A6ESP32::setBaudRate(long baudRate, byte rxPin, byte txPin) {
    int rate = 0;

    rate = detectRate(rxPin, txPin);
    if (rate == A6_NOTOK) {
        return A6_NOTOK;
    }

    // The rate is already the desired rate, return.
    //if (rate == baudRate) return OK;

    logln("Setting baud rate on the module...");

    // Change the rate to the requested.
    char buffer[30];
    sprintf(buffer, "AT+IPR=%lu", baudRate);
    A6command(buffer, "OK", "+IPR=", A6_CMD_TIMEOUT, RETRY +2, NULL, false);

    logln("Switching to the new rate... ");
    // Begin the connection again at the requested rate.
    A6conn->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    logln("Rate set.");

    return A6_OK;
}



// Read some data from the A6 in a non-blocking manner.
String A6ESP32::read() {
    String reply = "";
    if (A6conn->available()) {
        reply = A6conn->readString();
    }

    // XXX: Replace NULs with \xff so we can match on them.
    for (int x = 0; x < reply.length(); x++) {
        if (reply.charAt(x) == 0) {
            reply.setCharAt(x, 255);
        }
    }
    return reply;
}



// Issue a command.
byte A6ESP32::A6command(const char *command, const char *resp1, const char *resp2, int timeout, int repetitions, String *response, bool log ) {
	byte returnValue = A6_NOTOK;
	byte count = 0;
	while (count < repetitions && returnValue != A6_OK) {
		logln("Command: " + String(command));
		// Force serial log (form command sent manually)
		if (log) {
			Serial.println("Command: " + String(command));
		}

		A6conn->write(command);
		A6conn->write('\r');

		if (A6waitFor(resp1, resp2, timeout, response, log) == A6_OK) 
		  returnValue = A6_OK;    
		else 
		  returnValue = A6_NOTOK;
	  
		count++;
	}
	return returnValue;
}


// Wait for responses.
byte A6ESP32::A6waitFor(const char *resp1, const char *resp2, int timeout, String *response, bool log) {
    unsigned long entry = millis();  
	String reply = "";
	byte retVal = 99;
	do {
		reply += read();
		yield();
	} while (((reply.indexOf(resp1) + reply.indexOf(resp2)) == -2) && ((millis() - entry) < timeout));

	if (reply != "") {
		String myString = reply;
		myString.trim();
		logln("Reply in " + String(millis() - entry) + " ms: " + myString);
		if (log) {
			Serial.println("Reply in " + String(millis() - entry) + " ms: " + myString);
		}
	}

	if (response != NULL) {
		*response = reply;
	}

	if ((millis() - entry) >= timeout) {
		retVal = A6_TIMEOUT;
		log("Timed out.");
	}
	else {
		if (reply.indexOf(resp1) + reply.indexOf(resp2) > -2) 			
			retVal = A6_OK;
		
		else 			
			retVal = A6_NOTOK;
		
	}
	return retVal;
}
