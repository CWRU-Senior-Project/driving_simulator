#include <Wire.h>

struct Calibration {
  int brake_min;
  int brake_max;
  int gas_min;
  int gas_max;
  double rads_per_tick;
};

union PiDouble {
  byte b[8];
  double d;
};

#define ENCODER_IS_REVERSED 1

#define ESTOP_PIN 25
#define KEYSWITCH_PIN 42

#define DRIVE2_PIN 51
#define DRIVE_PIN 49
#define NEUTRAL_PIN 47
#define REVERSE_PIN 45
#define PARK_PIN 43

#define DRIVE2_GEAR 4
#define DRIVE_GEAR 3
#define NEUTRAL_GEAR 2
#define REVERSE_GEAR 1
#define PARK_GEAR 0
#define UNKNOWN_GEAR -1

#define GAS_PIN 0
#define BRAKE_PIN 1

#define ENCODER_A 52
#define ENCODER_B 50

#define SLAVE_ADDRESS 0x04

bool keyswitch_on = false;
int gearshift_state = UNKNOWN_GEAR;

float gas = 0;
float brake = 0;

struct Calibration cal;

volatile bool encoder_b_state;
volatile long encoder_ticks = 0;

volatile int received_byte_count = 0;
volatile PiDouble desired_wheel_angle;
volatile PiDouble desired_wheel_force;
volatile PiDouble desired_vibration;

void setup() {
  // set calibration values
  cal.brake_min = 177;
  cal.brake_max = 960;
  cal.gas_min = 417;
  cal.gas_max = 764;
  cal.rads_per_tick = 0.00770942982476;
  
  desired_wheel_angle.d = 0;
  desired_wheel_force.d = 0;
  desired_vibration.d = 0;
  
  Serial.begin(115200);
  
  // estop setup
  pinMode(ESTOP_PIN, INPUT);
  digitalWrite(ESTOP_PIN, HIGH);
  attachInterrupt(ESTOP_PIN, activateEstop, HIGH);

  // keyswitch setup
  pinMode(KEYSWITCH_PIN, INPUT);
  keyswitch_on = digitalRead(KEYSWITCH_PIN);
  
  // Gear shift setup
  pinMode(DRIVE2_PIN, INPUT);
  pinMode(DRIVE_PIN, INPUT);
  pinMode(NEUTRAL_PIN, INPUT);
  pinMode(REVERSE_PIN, INPUT);
  pinMode(PARK_PIN, INPUT);
  
  gearshift_state = getGear(gearshift_state);
  
  // gas and brake setup
  gas = getGas();
  brake = getBrake();
  
  // Enocder setup
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  attachInterrupt(ENCODER_A, handleEncoderChange, RISING);
  
  Wire.begin(SLAVE_ADDRESS);
  
  Wire.onReceive(receiveData);
}

void loop() {
  Serial.print("Wheel angle: ");
  Serial.println(desired_wheel_angle.d);
  Serial.print("Wheel force: ");
  Serial.println(desired_wheel_force.d);
  Serial.print("vibration: ");
  Serial.println(desired_vibration.d);
  delay(500);
  /*Serial.print("Keyswitch: ");
  keyswitch_on = digitalRead(KEYSWITCH_PIN);
  Serial.println(keyswitch_on);
  
  Serial.print("Gear: ");
  gearshift_state = getGear(gearshift_state);
  switch(gearshift_state) {
    case UNKNOWN_GEAR:
      Serial.println("Unknown");
      break;
    case PARK_GEAR:
      Serial.println("Park");
      break;
    case REVERSE_GEAR:
      Serial.println("Reverse");
      break;
    case NEUTRAL_GEAR:
      Serial.println("Neutral");
      break;
    case DRIVE_GEAR:
      Serial.println("Drive");
      break;
    case DRIVE2_GEAR:
      Serial.println("Drive 2");
      break;
  }
  
  Serial.print("Gas: ");
  gas = getGas();
  Serial.println(gas);
  Serial.print("Brake: ");
  brake = getBrake();
  Serial.println(brake);
  
  Serial.print("Encoder ticks: ");
  Serial.println((encoder_ticks*cal.rads_per_tick)/(2*PI));
  
  delay(500);*/
}

void activateEstop() {
  bool estop_active = true;
  do {
    Serial.println("Estop");
    estop_active = digitalRead(ESTOP_PIN);
    delayMicroseconds(500000);
  } while(estop_active);
  received_byte_count = 0;
}

int getGear(int last_state) {
   if(digitalRead(DRIVE2_PIN))
     return DRIVE2_GEAR;
   else if(digitalRead(DRIVE_PIN))
     return DRIVE_GEAR;
   else if(digitalRead(NEUTRAL_PIN))
     return NEUTRAL_GEAR;
   else if(digitalRead(PARK_PIN))
     return PARK_GEAR;
   else if(digitalRead(REVERSE_PIN))
   {
     if(last_state == NEUTRAL_GEAR || last_state == PARK_GEAR)
       return REVERSE_GEAR;
     return last_state;
   }
   
   return last_state;
}

float getGas() {
   float val = (float)(analogRead(GAS_PIN) - (float)cal.gas_min)/((float)(cal.gas_max - cal.gas_min)); 
   return max(min(val, 1), 0);
}

float getBrake() {
  float val = (analogRead(BRAKE_PIN) - (float)cal.brake_min)/((float)(cal.brake_max - cal.brake_min));
   return max(min(val, 1), 0);
}

void handleEncoderChange() {
    encoder_b_state = digitalRead(ENCODER_B);
    // and adjust counter + if A leads B
    #ifdef ENCODER_IS_REVERSED
      encoder_ticks -= encoder_b_state ? -1 : +1;
    #else
      encoder_ticks += encoder_b_state ? -1 : +1;
    #endif
}

void receiveData(int byteCount) {
  if(byteCount < 25)
  {
    while(Wire.available())
      Wire.read();
  }
  while(Wire.available())
  {
    received_byte_count = received_byte_count % 24;
    if(received_byte_count == 0)
      Wire.read(); // skip the first value
      
    switch(received_byte_count / 8)
    {
     case 0:
      desired_wheel_angle.b[received_byte_count % 8] = Wire.read();
      break;
     case 1:
      desired_wheel_force.b[received_byte_count % 8] = Wire.read();
      break;
     case 2:
      desired_vibration.b[received_byte_count % 8] = Wire.read();
      break;
    }
    
    received_byte_count++;
  }

}
