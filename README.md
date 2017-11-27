# How to measure fine dust via LoRaWan based on adafruit feather m0 lora boards and sds011/sds018 sensors

## How to send data from adafruit feather m0 lora 900 boards to the things network

These instructions and the source code are associated with https://www.meetup.com/Internet-of-Things-IoT-LoRaWan-Infrastruktur-4-RheinNeckar/ and have been created in the econtext of the iot hackathon at the DAI at https://dai-heidelberg.de/de/veranstaltungen/iot-hackathon-18425/.

They should work for both the 32u4 and the m0 based feather lora boards and have been tested in Europe with the m0 at 868MHZ with full gateways (reference to gateway building instructions to be included here). 

### Get your computer ready to program the adafruit feather m0 lora 900 board

1. buy or find an adafruit feather m0 lora 900, a fitting lipo battery and an usb cable
2. follow the instructions on the adafruit site at https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module?view=all to ...
3. solder the header pins,
4. solder an antenna,
4. install the arduino ide,
5. install both the Arduino SAMD boards and the Adafruit SAMD boards in the board manager of the Arduino IDE.
6. test your installation by loading the AnalogReadSerial example to your feather.

With the M0 its not the same as arduino for uploading code so get used to the new process.

### Install the LMIC library and wire up the device appropriately

The LMIC library implements the LoRaWan protocol.

6. follow the steps in the instructions at https://startiot.telenor.com/learning/getting-started-with-adafruit-feather-m0-lora/ from the step "Preparing the Device" to ...
7. the step "Edit the Transceiver config.h" - this last step might not be needed but check the values anyway. The wiring is visible in the image in this repository as 2 orange wires.
8. in the arduino ide us the example called ttn-abp provided in the lmic library and do the following modifications to the file:
10. insert LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); after the LMIC_reset(); in the setup function,
11. change the pin settings as follow at the start of the application:
```
const lmic_pinmap lmic_pins = {
  .nss = 8,  
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 4,
  .dio = {3,6,11},
};
```

Now the LMIC library is installed so it works for the M0 and the example program matches the default wiring. For the 32u4 the library might need some other modifications.

### Get your feather sending "hello world!" to the things network

12. sign up for an account at the things network at https://account.thethingsnetwork.org/register 
13. create an application in this account and create a device in it
13. change the device to use the ABP activation method (we had a lot of difficulties with the other method)
14. copy the network session key and the app session key as "c-style" and with "msb" into the code you got from the previous step. These keys look like this "{ 0xF8, 0xAA, 0xA8, 0x67, 0x5B, 0xEC, 0x16, 0x83, 0x51, 0xA1, 0x1D, 0x23, 0xEF, 0xB1, 0x12, 0xF9 }" and msb means that the most significant byte comes first.
15. copy the device address as hexadecimal string to your code. it looks like this: "26033FBF".
16. upload the code to your feather. if there is a LoRaWan gateway nearby it will receive "hello world!" from your device every 60 seconds and will forward it to your application.
17. check the console in the things network for your received data.

### Troubleshooting why your data does not show up

There can be many reasons for failure and this is just a short list of the basic trouble shooting tips.
- ideally have a node that works with the given gateway next to yours
- if you are to far away then the gateway might not be reached - in line of sight multiple kilometers could be ok - so coming closer to the gateway might be a good way to avoid this problem
- coming closer then 3 meters line of sight might be a problem - put something in between you and the gateway (e.g. a wall)
- if the owner of the gateway gives you access to it you can see sometimes your device registering with the gateway but the data not showing up in the application - but we have seen situations where the data was not showing up in the console but it has been forwarded to an google drive sheets via ifttt
- if the device shows up on the gateway but the data does not come through reseting the frame counter might help

### Adding a fine dust sensor like SDS011 or SDS018

1. buy a finedust sensor as required for your application. all of the known ones work with 5v and not with 3.3v so you also need to
2. buy a buck/boost converter to 5v and to
3. buy a levelshifter
4. wire them all up as on the included picture especially connecting TX and RX with RX and TX through the level shifter and providing the 5V as the VCC to the dust sensor.
5. use the loradust3.ino example in this repository to read the data from the sensor and submit it to the loradust network 

### Parse the values in the things network to json

This step makes it much easier to read the received values and to forward them to other services. With the above fine dust sensor you can use below routine in the "Payload Formats" section of your application in the the things network to create a beautiful json from the bytes received:
```
function Decoder(bytes, port) {
  var voltage = bytes[0]*8*3.3/1024;
  var dust025 = (256*bytes[1] + bytes[2])/10;
  var dust100 = (256*bytes[3] + bytes[4])/10;
  return {
    voltage: voltage,
    dust025: dust025,
    dust100: dust100
  };
}
```

### Use ifttt integration to forward your data to google sheets

This was quite easy to setup following the instructions at the things network and at ifttt.
