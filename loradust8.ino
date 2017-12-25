
// to be built with the arduino ide, tested to work with arduino 1.8.5
#include <lmic.h> // install this from github at https://github.com/matthijskooijman/arduino-lmic from the zip file into your libraries
#include <hal/hal.h>
#include <Adafruit_SleepyDog.h> // you have to install the https://github.com/adafruit/Adafruit_ASFcore from the zip file into your libraries
#include <DHT.h> // install the adafruit dht and adafruit sensor library from the arduino library manager
// to do - write frame counter to eeprom
// to check - maybe feather enable pin should be driven low to save power - but then does the m0 wake up again?

///////////////////////////////////////////// Change below for your node /////////////////////////////////////
// LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t NWKSKEY[16] =  { 0xF8, 0xAA, 0xA8, 0x67, 0x5B, 0xEC, 0x16, 0x83, 0x51, 0xA1, 0x1B, 0x21, 0xDF, 0xC1, 0x32, 0xF9 } ;
// LoRaWAN AppSKey, application session key
static const u1_t PROGMEM APPSKEY[16] = { 0xB3, 0x31, 0xB8, 0xEF, 0x20, 0x48, 0x7C, 0xED, 0x92, 0x03, 0xE6, 0x70, 0x69, 0xD9, 0x99, 0x3A };
// LoRaWAN end-device address (DevAddr)
static const u4_t DEVADDR = 0x26011FBF;
static const byte MYNODEID = 'M';
///////////////////////////////////////////// Change above for your node /////////////////////////////////////


#define DHTPIN A4     

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

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
const unsigned TX_INTERVAL = 1;      // seconds transmit cycle plus ... 
const unsigned SLEEP_TIME = 60*4+30;  // seconds sleep time plus ...
const unsigned MEASURE_TIME = 29;     // seconds measuring time should lead to ... 
                                      // 5 minutes total cycle time

const int PWM100PIN = A3;
const int PWM025PIN = A2;
const int DUSTPOWERPIN = A5;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 8,  
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3,6,11}
    ,
};

unsigned long next_measurement_done;

void onEvent (ev_t ev) {
    print_time();
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
              print_time();
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
            
            print_time();
            Serial.println("going to sleep now ... ");
            // lmic library sleeps automatically after transmission has been completed
            for(int i= 0; i < SLEEP_TIME / 16; i++) {
              Watchdog.sleep(16000); // maximum seems to be 16 seconds
              Serial.print('.');
            }
            Serial.println("... woke up again");
            digitalWrite(DUSTPOWERPIN, HIGH);
            next_measurement_done = millis() + MEASURE_TIME*1000;
            
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

    
    print_time();
    Serial.println("measuring");

    int dust02p5 = -1; // -1 arriving on the server means that no measurement worked
    int dust10p0 = -1;
    while(millis() < next_measurement_done) {
      int newdust02p5 = pulseIn(PWM025PIN, HIGH,1100*1000)/1000 -2;
      int newdust10p0 = pulseIn(PWM100PIN, HIGH,1100*1000)/1000 -2;
      print_time();
      Serial.print(newdust02p5);
      Serial.print(' ');
      Serial.println(newdust10p0);
      if(newdust02p5 >= 0) dust02p5 = newdust02p5;
      if(newdust10p0 >= 0) dust10p0 = newdust10p0;
    }

    print_time();
    Serial.println("dust sensor going to sleep");
    digitalWrite(DUSTPOWERPIN, LOW);
    
    float measuredvbat = analogRead(A7);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    float voltage = measuredvbat;
    print_time();
    Serial.print("VBat: " );
    Serial.println(voltage);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    // check if returns are valid, if they are NaN (not a number) then something went wrong!
    if (isnan(t)) t = 123; // 123 means error, this temperature should not occur 
    if (isnan(h)) h = 123; // 123 measn error, this humidity should not occur
    print_time();
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C");

    mydata[0]=voltage*32;
    mydata[1]=(int)(10*dust02p5) / 256;
    mydata[2]=(int)(10*dust02p5) % 256;
    mydata[3]=(int)(10*dust10p0) / 256;
    mydata[4]=(int)(10*dust10p0) % 256;
    mydata[5]=MYNODEID;
    mydata[6]=t;
    mydata[7]=h;
    // Prepare upstream data transmission at the next possible time.

    LMIC_setTxData2(1, mydata,8, 1);  // 1 to trigger that ACK is sent
    print_time();
    Serial.println(F("Packet queued"));
  }
  // Next TX is scheduled after TX_COMPLETE event.
}

void print_time() {
  Serial.print(millis());
  Serial.print(' ');
}

void setup(){
  unsigned long waittime = millis() + 5000;
  while (! Serial && millis() < waittime)
    ; // do nothing
  Serial.begin(115200);
  
  print_time();
  Serial.println(F("starting"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial1.begin(9600, SERIAL_8N1 );
 
  #ifdef VCC_ENABLE
   For Pinoccio Scout boards
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);
  delay(1000);
  #endif

  pinMode(PWM100PIN, INPUT);
  pinMode(PWM025PIN, INPUT);
  pinMode(DUSTPOWERPIN, OUTPUT);
  digitalWrite(DUSTPOWERPIN, HIGH);
  next_measurement_done = millis() + MEASURE_TIME*1000;

  dht.begin();
  
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

    // Start job sending
    do_send(&sendjob);
}
  
void loop(){
  os_runloop_once();

  // how is the control loop in this example?
  // os_run_loop_once will call the following 2 functions above:
  //   do_send is triggered based on the duty cycle defined in TX_INTERVAL, as sleeping stops the internal timer the sleep time + TX_INTERVAL is the true duty cycle
  //   onEvent is triggered and the key event is EV_TXCOMPLETE which closes the sending cycle, schedules the next do_send and then sleeps    
}

