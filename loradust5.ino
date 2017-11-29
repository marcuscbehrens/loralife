#include <lmic.h>
#include <hal/hal.h>

///////////////////////////////////////////// Change below for your node /////////////////////////////////////
// LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t NWKSKEY[16] =  { ... } ;
// LoRaWAN AppSKey, application session key
static const u1_t PROGMEM APPSKEY[16] = { .... };
// LoRaWAN end-device address (DevAddr)
static const u4_t DEVADDR = 0x...;
static const byte MYNODEID = '...';
///////////////////////////////////////////// Change above for your node /////////////////////////////////////

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

uint8_t mydata[] = "12345678901234";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 8,  
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3,6,11}
    ,
};

float pm25;
float pm10;
float voltage;
unsigned long nextmeasurement;

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            digitalWrite(LED_BUILTIN, HIGH);
            delay(50);
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK) {
              Serial.println(F("Received ack"));
              digitalWrite(LED_BUILTIN, LOW);
            }
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void do_send(osjob_t* j){
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // fill in current values of measurements
    unsigned long mdone = millis()+5000;
    wake_dust();
    Serial.println("measuring...");
    while(millis() < mdone)
      checkSDS011ForMessage();
    Serial.println("sleeping...");
    sleep_dust();
    
    float measuredvbat = analogRead(A7);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    voltage = measuredvbat;

    Serial.print("VBat: " );
    Serial.println(voltage);

    mydata[0]=voltage*32;
    mydata[1]=(int)(10*pm25) / 256;
    mydata[2]=(int)(10*pm25) % 256;
    mydata[3]=(int)(10*pm10) / 256;
    mydata[4]=(int)(10*pm10) % 256;
    mydata[5]=MYNODEID;
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, mydata,6, 1);  // 1 to trigger that ACK is sent
    Serial.println(F("Packet queued"));
  }
  // Next TX is scheduled after TX_COMPLETE event.
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
}

void wake_dust() {
  Serial1.write(0x01);
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

void setup(){
  unsigned long waittime = millis() + 5000;
  while (! Serial && millis() < waittime)
    ; // do nothing
  Serial.begin(115200);
  Serial.println(F("Starting"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial1.begin(9600, SERIAL_8N1 );
 
    #ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif

    #if defined(CFG_eu868)
    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.
    // NA-US channels 0-71 are configured automatically
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    // TTN defines an additional channel at 869.525Mhz using SF9 for class B
    // devices' ping slots. LMIC does not have an easy way to define set this
    // frequency and support for class B is spotty and untested, so this
    // frequency is not configured here.
    #elif defined(CFG_us915)
    // NA-US channels 0-71 are configured automatically
    // but only one group of 8 should (a subband) should be active
    // TTN recommends the second sub band, 1 in a zero based count.
    // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
    LMIC_selectSubBand(1);
    #endif

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF7,14);

    // Start job sending after 60 seconds
    do_send(&sendjob);
}
  
void loop(){
  os_runloop_once();

    // For example, here is the Feather with RFM9x 900MHz radio set up for +20dBm power, transmitting a data payload of 20 bytes. Transmits take about 130mA for 70ms
    // The ~13mA quiescent current is the current draw for listening (~2mA) plus ~11mA for the microcontroller. This can be reduce to amost nothing with proper sleep modes and not putting the module in active listen mode!
    // You can put the module into sleep mode by calling radio.sleep(); which will save you about 2mA
}

