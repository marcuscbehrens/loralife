// based on https://github.com/ricki-z/SDS011
// based on https://github.com/crystaldust/sds018
// Author: marcus@4xb.de
// Date: 2017-04-14Z

static const byte SLEEPCMD[19] = {
  0xAA, // head
  0xB4, // command id
  0x06, // data byte 1
  0x01, // data byte 2 (set mode)
  0x00, // data byte 3 (sleep)
  0x00, // data byte 4
  0x00, // data byte 5
  0x00, // data byte 6
  0x00, // data byte 7
  0x00, // data byte 8
  0x00, // data byte 9
  0x00, // data byte 10
  0x00, // data byte 11
  0x00, // data byte 12
  0x00, // data byte 13
  0xFF, // data byte 14 (device id byte 1)
  0xFF, // data byte 15 (device id byte 2)
  0x05, // checksum
  0xAB  // tail
};


void sleep_dust() {
  //Serial1.flush();
  //while (Serial1.available() > 0) {
  //  Serial1.read();
  //}
  for (int i = 0; i < 19; i++)
    Serial1.write(SLEEPCMD[i]);
  //Serial1.flush();
  //while (Serial1.available() > 0) {
  //  Serial1.read();
  //}
}

void wake_dust() {
  Serial1.write(0x01);
  //Serial1.flush();
  //while (Serial1.available() > 0) {
  //  Serial1.read();
  //}
}


int bindex = 0;
uint8_t mbuffer[10] = {0};
uint8_t last = 0;
float dust02p5;
float dust10p0;

void checkSDS011ForMessage() {
  if(Serial1.available()) {
    uint8_t next = Serial1.read();
    if(next == 0xAA && last==0xAB) // if this combination occurs in real life we might be off for one measurement
      bindex = 0;
    else if(bindex >= 10)
      bindex = 0;
    mbuffer[bindex]=next;
    last = next;
    Serial.print(next, HEX);
    Serial.print(" ");
    bindex++;
    if(bindex == 10) {
      uint8_t sum = 0;
      for( int i=2; i<=7; ++i ) {
        sum += mbuffer[i];
      }
      if(next != 0xAB) Serial.println("warning: mbuffer full but AB for end of message");
      else if(mbuffer[1] != 0xC0) Serial.println("warning: not a C0 message");
      else if(sum != mbuffer[8]) Serial.println("warning: wrong checksum");
      else {
        dust02p5 = ( ( mbuffer[3] * 256.0 ) + mbuffer[2] ) / 10.0;
        dust10p0 = ( ( mbuffer[5] * 256.0 ) + mbuffer[4] ) / 10.0;
        Serial.print( "Dust ppm 2.5: " );
        Serial.print( dust02p5 );
        Serial.print( " 10: " );
        Serial.println( dust10p0 );
      }
    }
  }
}

unsigned long nextsleep;
unsigned long nextwake;
boolean awake;

void setup() {
  // Serial0 for console log, Serial1 for conmmunication with the device.
  // This is tested adafruit feather 32u4 with lora modem which provides more than one serial port.
  // For uno, drop the console log and use Serial0 as the device conmmunication channel.
  while (!Serial); // only needed on feathers
  Serial.begin( 115200 );
  Serial.println("loradust1 started");
  Serial1.begin( 9600, SERIAL_8N1 );
  wake_dust(); // sensor might have been turned off
  awake = true;
  //Serial1.flush();
  nextsleep = millis() + 60000;
  nextwake = nextsleep + 20000;
}

void loop() {
  checkSDS011ForMessage();

  unsigned long rightnow = millis();
  if(awake && rightnow > nextsleep) {
    sleep_dust();
    awake = false;
    Serial.println("sds011 put to sleep");
  }
  if(! awake && rightnow > nextwake) {
    nextsleep = rightnow + 10000;
    nextwake = nextsleep + 20000;
    wake_dust();
    awake = true;
    Serial.println("sds011 woken up");
  }

  //radio.sleep();
  //Watchdog.sleep(20000);
  //wakeup_dust();
  //Serial.println("loradust1 woken up");
}
