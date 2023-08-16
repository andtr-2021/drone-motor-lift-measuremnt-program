/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-servo-motor-controlled-by-potentiometer
 */

#include <ESP32Servo.h>
#include <HX711_ADC.h>
#include <Arduino.h>
#include "ACS712.h"
#include <PID_v1.h>

/*#define PIN_POTENTIOMETER 36 // ESP32 pin GPIO36 (ADC0) onnected to potentiometer*/
#define PIN_ESC         26 // ESP32 pin GPIO26 connected to ESC servo motor
#define V_SENSOR_PIN  34 // voltage sensor pin
#define C_SENSOR_PIN  32 // current sensor pin 
#define LOADCELL_DT_PIN 16
#define LOADCELL_SCK_PIN 4

#define calibration_factor 430 // Depends on the load cell

Servo esc;  // create servo object to control a servo
ACS712 sensor(ACS712_05B, C_SENSOR_PIN); // set the 5A current sensor object 

// Float variables for voltage and current sensors
float adc_voltage = 0.0;
float voltage = 0.0;
float current = 0.0;
 
// Float variables for resistor values in divider (in ohms) in Voltage sensor // do not touch (it is fixed value)
// The voltage divider circuit is used to reduce the voltage from a higher source to a level that can be measured by the Arduino.
float R1 = 30000.0; // 30k ohm
float R2 = 7500.0; // 7.5k ohm
 
// Float for Reference Voltage
// float ref_voltage = 25.2; //voltage from the battery
// default reference value of the sensor is 5V but I can modify it to 3.3 when I connnect with it
float ref_voltage = 5;  // do not touch (it is fixed value)
// variable for analog voltage sensor reading
int adc_value = 0;  

// Loadcell setup
HX711_ADC scale(LOADCELL_DT_PIN, LOADCELL_SCK_PIN);

// check if data is ready to be read from the loadcell
volatile boolean newDataReady;

// variable to store the loadcell data
float loadcell_data; // measurement from the loadcell in gr

// serial configuration parameters
unsigned long curr_time = 0, prev_time = 0, dt = 50000; // time interval in us

// PID configuration parameters
double kp = 0.6, ki = 0.6, kd = 0, input = 0, output = 0, setpoint = 0;
PID myPID(&input, &output, &setpoint, kp, ki, kd, DIRECT);

// functions declarations
void dataReadyISR();
void SerialDataWrite();
float floatMap(float x, float in_min, float in_max, float out_min, float out_max);

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
   
  // voltage sensor setup
  pinMode(V_SENSOR_PIN, INPUT);
  
  // ESC & motor setup
  esc.attach(PIN_ESC); // (pin, min pulse width, max pulse width in microseconds)
  esc.writeMicroseconds(1000); // Initialize the ESC at minimum throttle (adjust if needed)

  // Current sensor setup
  int zero = sensor.calibrate();

  // Delay to allow the ESC to recognize the minimum throttle position
  delay(2000);

  while (!Serial);
  scale.begin();
  scale.start(2000, true);
  scale.setCalFactor(calibration_factor);
  pinMode(LOADCELL_DT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LOADCELL_DT_PIN), dataReadyISR, FALLING);

  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(50); // Set the sample time for the PID control
  myPID.SetOutputLimits(0, 180);

  esc.write(1); // send minimum throttle signal to the ESC
}

void loop() {

  if (newDataReady)
  {
    newDataReady = 0;
    loadcell_data = scale.getData();
  }

  curr_time = micros();
  if (curr_time - prev_time >= dt)
  {
    prev_time += dt;
    SerialDataWrite();
  }
  
  input = - loadcell_data; // Update the PID input
  myPID.Compute();       // Calculate new output
  esc.write(output);

  // Voltage Sensor calculation
  adc_value = analogRead(V_SENSOR_PIN);   // read the state of the the input pin:
  adc_voltage  = (adc_value * ref_voltage) / 4096.0;  // Determine voltage at ADC input // analog read resolution 10 bi(1025)? 12bit(4096)? 
  voltage = adc_voltage / (R2/(R1+R2)) ;    // Calculate voltage at divider input
   
  //Print results to Serial Monitor to 2 decimal places
  Serial.print(" Input Voltage = ");
  Serial.println(voltage, 2);

  // get the current from the sensor and display
  current = sensor.getCurrentDC();  // this current will set with 2 decimal points automatically from on the library
  Serial.print(" Current = ");
  Serial.println(current);

  // final variables
  // current 
  // in_voltage
  delay(1000);
}

void dataReadyISR()
{
  if (scale.update())
    newDataReady = 1;
}

void SerialDataWrite()
{
  Serial.print("Loadcell value: ");
  Serial.print(curr_time / 1000);
  Serial.print(",");
  Serial.print(loadcell_data);
  Serial.println("");
}
