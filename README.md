# A6ESP32
ESP32 Hardware Serial A6 GSM module


An ESP32/Arduino library for communicating with the AI-Thinker A6 GSM module.
It will probably also work with other GSM modules that use the AT command set,
like the SIM900.

## Details

This library is an adaptation of [A6lib](https://github.com/skorokithakis/A6lib). 
Please feel free to issue pull requests to improve it.

## Usage

With ESP32 you can redefine RX and TX pin according to your needs.
The [examples](https://github.com/cotestatnt/ESP32GSM/examples) assumes the TX pin is connected to GPIO4 and the RX to GPIO33. 
A6ESP32 can power-cycle the module if you connect a MOSFET to a pin and control the A6's power supply with it.

The A6's PWR pin should be permanently connected to Vcc (if you think that's wrong or know a better way, please open an issue).

~~~
// Start and place a call.
A6ESP32.dial("1234567890");
delay(8000);

A6ESP32.hangUp();
delay(8000);

A6ESP32.redial();
delay(8000);

// Send a message.
A6ESP32.sendSMS("+1234567890", "Hello there!");

// Get an SMS message from memory.
SMSmessage sms = A6ESP32.readSMS(3);

// Delete an SMS message.
A6ESP32.deleteSMS(3);

callInfo cinfo = A6ESP32.checkCallStatus();
// This will be the calling number, "1234567890".
cinfo.number;
~~~

This library doesn't currently include any code to connect to the internet, but
a PR adding that would be very welcome.
