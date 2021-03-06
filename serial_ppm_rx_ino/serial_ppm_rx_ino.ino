#include <TimerOne.h> 

#define CHANNELS 8

// Set these a little high since they are registering low on AQ
#define MIN_PULSE_TIME 1030   // 1000us
#define MAX_PULSE_TIME 1990   // 2000us
#define HALF_PULSE_TIME (MIN_PULSE_TIME + MAX_PULSE_TIME) / 2
#define SYNC_PULSE_TIME 3050  // 3000us

#define SERIAL_BAUD 38400

#define PIN_LED 13
#define PIN_PPM 9

#define RECEIVER_TIMEOUT 1500 // 1.5s
#define MIN_RECEIVER_VALUE 0
#define MAX_RECEIVER_VALUE 250

// For Aeroquad, this should disarm the quad when reception is lost
// TODO: Need to match AQ's channels
// YAXIS,ZAXIS,THROTTLE,XAXIS,AUX1,AUX2,0,0
const unsigned int defaultPulseWidths[CHANNELS] = {
  MIN_PULSE_TIME,      // Throttle
  HALF_PULSE_TIME,     // Roll
  HALF_PULSE_TIME,     // Pitch
  MIN_PULSE_TIME,      // Yaw
  
  MAX_PULSE_TIME,     // AUX1 (MODE in Aeroquad)
  MAX_PULSE_TIME,     // AUX2 (ALTITUDE in Aeroquad)
  MAX_PULSE_TIME,     // AUX3
  MAX_PULSE_TIME      // AUX4
};


// Inbound data is: [throttle, roll, pitch, yaw, aux1, aux2, aux3, aux4]
// AeroQuad expects: [XAXIS,YAXIS,THROTTLE,ZAXIS,MODE,AUX1,AUX2,AUX3]
// Make sure you define SERIAL_SUM_PPM_2 in AeroQuad!
const int channelMap[CHANNELS] = {1, 2, 0, 3, 4, 5, 6, 7};

unsigned int pulseWidths[CHANNELS];
byte buffer[CHANNELS];
int bytesReceived;
byte currentByte;
unsigned long lastReceived = 0;
boolean armed = false;

void setDefaultPulseWidths() {
  for (int i=0; i<CHANNELS; i++) {
    pulseWidths[i] = defaultPulseWidths[i];
  }
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PPM, OUTPUT);

  Serial.begin(38400);
  
  setDefaultPulseWidths();
  
  // Start timer with sync pulse
  Timer1.initialize(SYNC_PULSE_TIME);
  Timer1.attachInterrupt(isr_sendPulses);
  isr_sendPulses();
}

void loop() {
  handleSerial();
  checkArmed();
  checkLostReception();
} 

void handleSerial() {
  // Handle Serial Data
  if (Serial.available()) {
    lastReceived = millis();
    currentByte = Serial.read();

    if (currentByte == 254) {
      // Either packet is done, or we got corrupt data. Reset the packet
      bytesReceived = 0;
    } else {
      buffer[bytesReceived] = currentByte;
      bytesReceived++;
    }

    if (bytesReceived == CHANNELS) {
      bytesReceived = 0;
      armed = true;

      // Convert char (0-250) to pulse width (1000-2000)
      for (int i=0; i<CHANNELS; i++) {
        pulseWidths[i] = map(buffer[i], MIN_RECEIVER_VALUE, MAX_RECEIVER_VALUE, 
                                        MIN_PULSE_TIME, MAX_PULSE_TIME);
      }
    }
  }
}

void checkArmed() {
  if(!armed) {
    // Not Armed yet (no packets received) or lost reception, blink the LED
    // and set the dault positions
    setDefaultPulseWidths();
    
    digitalWrite(PIN_LED, HIGH);
    delay(300);
    
    digitalWrite(PIN_LED, LOW);
    delay(300);
  } else {
    digitalWrite(PIN_LED, HIGH);
  }
}

void checkLostReception() {
  // Check if we lost reception
  if(armed && lastReceived > 0 &&  millis() - lastReceived > RECEIVER_TIMEOUT) {
    armed = false;
    
    for(int i=0; i<5; i++) {
      digitalWrite(PIN_LED, HIGH);
      delay(100);
      digitalWrite(PIN_LED, LOW);
      delay(100);
    }
  }
}

// Sync pulse first
volatile int currentChannel = 0;

void isr_sendPulses() {
  digitalWrite(PIN_PPM, LOW);
  
  if (currentChannel == CHANNELS) {
    // After last channel
    Timer1.setPeriod(SYNC_PULSE_TIME);
    currentChannel = 0; // Will be 0 on next interrupt
  } else {
    Timer1.setPeriod(pulseWidths[channelMap[currentChannel]]);
    currentChannel++;
  }
  
  digitalWrite(PIN_PPM, HIGH);
}

