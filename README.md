# Instructions on how to assemble a Sensornode for fine dust measuring with the things networl via LoRaWan

These instructions and the source code are associated with https://www.meetup.com/Internet-of-Things-IoT-LoRaWan-Infrastruktur-4-RheinNeckar/ and with the iot hackathon at the DAI at https://dai-heidelberg.de/de/veranstaltungen/iot-hackathon-18425/.


1. buy or find an adafruit feather m0 lora 900, a fitting lipo battery and an usb cable
2. follow the instructions on the adafruit site at https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module?view=all to ...
3. solder the header pins,
4. solder an antenna,
4. install the arduino ide,
5. install both the Arduino SAMD boards and the Adafruit SAMD boards in the board manager of the Arduino IDE.
6. follow the steps in the instructions at https://startiot.telenor.com/learning/getting-started-with-adafruit-feather-m0-lora/ from the step Preparing the Device ...
7. to the step "Edit the Transceiver config.h" - this last step should not be needed but check the values anyway.
8. in the arduino ide us the example called ttn-abp provided in the lmic library and ...
9. do the following modifications to the file:
10. insert LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); after the LMIC_reset(); in the setup function,
11. change the pin settings as follow at the start of the applicvation r
const lmic_pinmap lmic_pins = {
    .nss = 8,  
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3,6,11},
};
12.sign up for an account at the things network at: http://thethingsnetwork.org
13. create an application in it and create a device in it - make sure to use ABP activation method
14. copy the device eui, the app eui as c-style msb into your code. it looks like this: msb
{ 0x00, 0x34, 0x03, 0xCA, 0x18, 0xC1, 0x33, 0xBE }
15.copy the device address as hexadecimal string to your code.
16. now you should be able to upload your code and your devicde to get into the network.
