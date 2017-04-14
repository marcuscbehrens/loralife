// based on https://github.com/ricki-z/SDS011
// based on https://github.com/crystaldust/sds018
// Author: marcus@4xb.de
// Date: 2017-04-14Z

// hookup from SDS018 to feather - TX to RX, RX to TX, 5V to BAT, GND to GND, that's it

// use sleep once all of your debugging is through
//#include "Adafruit_SleepyDog.h"

void setup() {
  // Serial0 for console log, Serial1 for conmmunication with the device.
  // This is tested adafruit feather 32u4 with lora modem which provides more than one serial port.
  // For uno, drop the console log and use Serial0 as the device conmmunication channel.
  while (!Serial); // only needed on feathers
  Serial.begin( 115200 );
  Serial.println("loradust1 started");
  Serial1.begin( 9600, SERIAL_8N1 );
  wakeup_dust(); // sensor might have been turned off
  pinMode(13, OUTPUT);
}

void loop() {
  digitalWrite(13, HIGH);
  delay(1000); measure_dust();
  delay(1000); measure_dust();
  delay(1000); measure_dust();
  delay(1000); measure_dust();
  delay(1000); measure_dust();
  delay(1000); measure_dust();
  measure_bat();
  digitalWrite(13, LOW);

  sleep_dust();
  delay(20000);
  // use sleep once all of your debugging is through
  //radio.sleep();
  //Watchdog.sleep(20000);
  wakeup_dust();
  Serial.println("loradust1 woken up");
}

void measure_bat() {
  float measuredvbat = analogRead(A9);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat); 
}

void measure_dust() {
  uint8_t mData = 0;
  uint8_t mPkt[10] = {0};
  uint8_t mCheck = 0;
  while( Serial1.available() > 0 ) {
    for( int i=0; i<10; ++i ) {
      mPkt[i] = Serial1.read();
      //Serial.println( mPkt[i], HEX );
    }
    if( 0xC0 == mPkt[1] ) {
      // Read dust density.
      // Check
      uint8_t sum = 0;
      for( int i=2; i<=7; ++i ) {
        sum += mPkt[i];
      }
      if( sum == mPkt[8] ) {
        uint8_t pm25Low   = mPkt[2];
        uint8_t pm25High  = mPkt[3];
        uint8_t pm10Low   = mPkt[4];
        uint8_t pm10High  = mPkt[5];
  
        float pm25 = ( ( pm25High * 256.0 ) + pm25Low ) / 10.0;
        float pm10 = ( ( pm10High * 256.0 ) + pm10Low ) / 10.0;
        
        Serial.print( "PM2.5: " );
        Serial.print( pm25 );
        Serial.print( "\nPM10 :" );
        Serial.print( pm10 );
        Serial.println();
      }
    }
    
    Serial1.flush();
  }
}

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
  for (uint8_t i = 0; i < 19; i++) {
    Serial1.write(SLEEPCMD[i]);
  }
  Serial1.flush();
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void wakeup_dust() {
  Serial1.write(0x01);
  Serial1.flush();
}


