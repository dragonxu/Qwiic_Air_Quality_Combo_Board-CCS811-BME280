/*
  SparkFun combination CCS811 tVOC sensor combined with BME280 humidity, pressure, temp sensor
  By: Nathan Seidle
  SparkFun Electronics
  Date: April 3rd, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This is a fast tester to verify both ICs are working.

  Hardware Connections:
  Attach a Qwiic Shield to your RedBoard, Photon, or ESP32.
  Plug the Qwiic Air Quality Combo board into one of the connectors
  Serial.print it out at 9600 baud to serial monitor.

*/

#include <Wire.h>
#include "SparkFunBME280.h" //Library for BME280 from library manager or https://github.com/sparkfun/SparkFun_BME280_Arduino_Library

BME280 pressureSensor;

#define CCS811_ADDR 0x5B //7-bit unshifted default I2C Address

//Register addresses
#define CSS811_STATUS 0x00
#define CSS811_MEAS_MODE 0x01
#define CSS811_ALG_RESULT_DATA 0x02
#define CSS811_RAW_DATA 0x03
#define CSS811_ENV_DATA 0x05
#define CSS811_NTC 0x06
#define CSS811_THRESHOLDS 0x10
#define CSS811_BASELINE 0x11
#define CSS811_HW_ID 0x20
#define CSS811_HW_VERSION 0x21
#define CSS811_FW_BOOT_VERSION 0x23
#define CSS811_FW_APP_VERSION 0x24
#define CSS811_ERROR_ID 0xE0
#define CSS811_APP_START 0xF4
#define CSS811_SW_RESET 0xFF

//These are the air quality values obtained from the sensor
unsigned int tVOC = 0;
unsigned int CO2 = 0;

void setup()
{
  Serial.begin(9600);
  Serial.println("CCS811+BME280 Read Example");

  Wire.begin();

  CO2 = 65535; //Set error condition
}

void loop()
{
  if (CO2 == 65535 || CO2 == 65534)
  {
    //No sensor detected, re-try setup
    if (configureCCS811() == false) //Turn on CCS811 sensor
      Serial.println("Problem with CCS811.");
    else
    {
      Serial.println("CCS811 GOOD");
      CO2 = 65534;
    }

    //Setup the BME280 for basic readings
    pressureSensor.settings.commInterface = I2C_MODE;
    pressureSensor.settings.I2CAddress = 0x77;
    pressureSensor.settings.runMode = 3; //  3, Normal mode
    pressureSensor.settings.tStandby = 0; //  0, 0.5ms
    pressureSensor.settings.filter = 0; //  0, filter off
    pressureSensor.settings.tempOverSample = 1;
    pressureSensor.settings.pressOverSample = 1;
    pressureSensor.settings.humidOverSample = 1;

    delay(10); //Give BME280 time to come on
    //Calling .begin() causes the settings to be loaded
    byte id = pressureSensor.begin(); //Returns ID of 0x60 if successful
    if (id != 0x60)
      Serial.println("Problem with BME280.");
    else
    {
      Serial.println("BME280 GOOD");
      if(CO2 == 65534) CO2 = 0; //All good! Clear error condition
    }

    delay(50);

  }
  else if (dataAvailable()) //Check to see if CCS811 has new data. It is the slowest sensor
  {
    readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables

    float temperatureC = pressureSensor.readTempC();
    float pressure = pressureSensor.readFloatPressure();
    float humidity = pressureSensor.readFloatHumidity();

    Serial.print("CO2[");
    Serial.print(CO2);
    Serial.print("] tVOC[");
    Serial.print(tVOC);
    Serial.print("] humidity[");
    Serial.print(humidity, 0);
    Serial.print("] pressure[");
    Serial.print(pressure, 0);
    Serial.print("] temperatureC[");
    Serial.print(temperatureC, 1);
    Serial.print("] millis[");
    Serial.print(millis());
    Serial.print("]");
    Serial.println();
  }
  else if (checkForError())
  {
    printError();
  }

  delay(100); //Wait for next reading
}

//Updates the total voltatile organic compounds (TVOC) in parts per billion (PPB)
//and the CO2 value
//Returns nothing
void readAlgorithmResults()
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ALG_RESULT_DATA);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 4); //Get four bytes

  byte co2MSB = Wire.read();
  byte co2LSB = Wire.read();
  byte tvocMSB = Wire.read();
  byte tvocLSB = Wire.read();

  CO2 = ((unsigned int)co2MSB << 8) | co2LSB;
  tVOC = ((unsigned int)tvocMSB << 8) | tvocLSB;
}

//Turns on the sensor and configures it with default settings
boolean configureCCS811()
{
  //Verify the hardware ID is what we expect
  byte hwID = readRegister(0x20); //Hardware ID should be 0x81
  if (hwID != 0x81)
  {
    Serial.print("Hardware ID wrong: 0x");
    Serial.println(hwID, HEX);
    return (false);
  }

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at Startup");
    printError();
    return (false);
  }

  //Tell App to Start
  if (appValid() == false)
  {
    Serial.println("Error: App not valid.");
    return (false);
  }

  //Write to this register to start app
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_APP_START);
  Wire.endTransmission();

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at AppStart");
    printError();
    return (false);
  }

  //Set Drive Mode
  setDriveMode(1); //Read every second

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at setDriveMode");
    printError();
    return (false);
  }

  return (true); //All set!
}

//Checks to see if error bit is set
boolean checkForError()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 0);
}

//Displays the type of error
//Calling this causes reading the contents of the ERROR register
//This should clear the ERROR_ID register
void printError()
{
  byte error = readRegister(CSS811_ERROR_ID);

  Serial.print("Error: ");
  if (error & 1 << 5) Serial.print("HeaterSupply ");
  if (error & 1 << 4) Serial.print("HeaterFault ");
  if (error & 1 << 3) Serial.print("MaxResistance ");
  if (error & 1 << 2) Serial.print("MeasModeInvalid ");
  if (error & 1 << 1) Serial.print("ReadRegInvalid ");
  if (error & 1 << 0) Serial.print("MsgInvalid ");

  Serial.println();
}

//Returns the baseline value
//Used for telling sensor what 'clean' air is
//You must put the sensor in clean air and record this value
unsigned int getBaseline()
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_BASELINE);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 2); //Get two bytes

  byte baselineMSB = Wire.read();
  byte baselineLSB = Wire.read();

  unsigned int baseline = ((unsigned int)baselineMSB << 8) | baselineLSB;

  return (baseline);
}

//Checks to see if DATA_READ flag is set in the status register
boolean dataAvailable()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 3);
}

//Checks to see if APP_VALID flag is set in the status register
boolean appValid()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 4);
}

//Enable the nINT signal
void enableInterrupts(void)
{
  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there
  setting |= (1 << 3); //Set INTERRUPT bit
  writeRegister(CSS811_MEAS_MODE, setting);
}

//Disable the nINT signal
void disableInterrupts(void)
{
  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there
  setting &= ~(1 << 3); //Clear INTERRUPT bit
  writeRegister(CSS811_MEAS_MODE, setting);
}

//Mode 0 = Idle
//Mode 1 = read every 1s
//Mode 2 = every 10s
//Mode 3 = every 60s
//Mode 4 = RAW mode
void setDriveMode(byte mode)
{
  if (mode > 4) mode = 4; //Error correction

  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there

  setting &= ~(0b00000111 << 4); //Clear DRIVE_MODE bits
  setting |= (mode << 4); //Mask in mode
  writeRegister(CSS811_MEAS_MODE, setting);
}

//Given a temp and humidity, write this data to the CSS811 for better compensation
//This function expects the humidity and temp to come in as floats
void setEnvironmentalData(float relativeHumidity, float temperature)
{
  int rH = relativeHumidity * 1000; //42.348 becomes 42348
  int temp = temperature * 1000; //23.2 becomes 23200

  byte envData[4];

  //Split value into 7-bit integer and 9-bit fractional
  envData[0] = ((rH % 1000) / 100) > 7 ? (rH / 1000 + 1) << 1 : (rH / 1000) << 1;
  envData[1] = 0; //CCS811 only supports increments of 0.5 so bits 7-0 will always be zero
  if (((rH % 1000) / 100) > 2 && (((rH % 1000) / 100) < 8))
  {
    envData[0] |= 1; //Set 9th bit of fractional to indicate 0.5%
  }

  temp += 25000; //Add the 25C offset
  //Split value into 7-bit integer and 9-bit fractional
  envData[2] = ((temp % 1000) / 100) > 7 ? (temp / 1000 + 1) << 1 : (temp / 1000) << 1;
  envData[3] = 0;
  if (((temp % 1000) / 100) > 2 && (((temp % 1000) / 100) < 8))
  {
    envData[2] |= 1;  //Set 9th bit of fractional to indicate 0.5C
  }

  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ENV_DATA); //We want to write our RH and temp data to the ENV register
  Wire.write(envData[0]);
  Wire.write(envData[1]);
  Wire.write(envData[2]);
  Wire.write(envData[3]);
}

//Reads from a give location from the CSS811
byte readRegister(byte addr)
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 1);

  return (Wire.read());
}

//Write a value to a spot in the CCS811
void writeRegister(byte addr, byte val)
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.write(val);
  Wire.endTransmission();
}
