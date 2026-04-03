/*
 * HOW YOU'D FIGURE ALL THIS OUT WITHOUT AI:
 *
 * 1. "What class pattern do I follow?"
 *    -> Read ButtonSampler.h/.cpp in E80 Github/libraries/main/. It's the
 *       simplest DataSource example. Copy its structure: extend DataSource,
 *       implement writeDataBytes(), provide csvVarNames and csvDataTypes
 *       strings to the DataSource constructor. The E80 README.md in
 *       libraries/main/ also explains the DataSource pattern.
 *
 * 2. "What I2C address and registers does the VEML6030 use?"
 *    -> Read the VEML6030 datasheet (Vishay document 84366/veml6030.pdf,
 *       also saved locally at felix-notes/resources/veml6030.pdf).
 *       Section 1 "General Description" gives the I2C address (0x10 when
 *       ADDR=GND, 0x48 when ADDR=VDD). The register map is in Section 5.
 *       Register 0x00 = ALS_CONF, 0x04 = ALS data output.
 *
 * 3. "What gain and integration time should I use?"
 *    -> Datasheet Table 1 "Resolution and Maximum Detection Range" shows
 *       every gain/IT combination's lux-per-count and max lux. For outdoor
 *       sunlight (~100,000 lux), you need gain=1/8 and IT=25ms, which gives
 *       resolution=1.8432 lux/count and max=~120,793 lux. If you picked a
 *       higher gain or longer IT, the sensor would saturate outdoors.
 *
 * 4. "How do I compute the 0x1300 config value?"
 *    -> Datasheet Section 5, Table "ALS_CONF Register" shows each bit's
 *       position and meaning. You manually compose the 16-bit value:
 *       bits[12:11]=10 (gain 1/8), bits[9:6]=1100 (IT 25ms), bit[0]=0
 *       (power on). Write it out in binary and convert to hex.
 *
 * 5. "How do I read/write I2C registers on Arduino?"
 *    -> Arduino Wire library docs (arduino.cc/reference/en/language/
 *       functions/communication/wire/). The pattern is always:
 *       Wire.beginTransmission(addr) -> Wire.write(register) ->
 *       Wire.endTransmission() for writes, and Wire.requestFrom()
 *       for reads. The VEML6030 datasheet Section 6 shows that it
 *       uses standard I2C with low-byte-first (little-endian) data.
 *
 * 6. "Which Teensy pins are SDA and SCL?"
 *    -> Teensy pinout card (pjrc.com/teensy/pinout.html) shows pin 18=SDA0,
 *       pin 19=SCL0. Cross-check with the MTB schematic
 *       (design_files/motherboard/MTB_Schematic_3.0.2021.pdf) to confirm
 *       these pins are routed through the J1 protoboard connector.
 *
 * 7. "Will this conflict with the IMU?"
 *    -> SensorIMU.cpp calls Wire.begin() for the LSM303AGR. I2C supports
 *       multiple devices on the same bus at different addresses. The IMU
 *       uses 0x19 and 0x1E; the VEML6030 uses 0x10. No conflict. Check
 *       addresses in the LSM303AGR datasheet and VEML6030 datasheet.
 */

#ifndef __SENSOR_VEML6030_H__
#define __SENSOR_VEML6030_H__

#include <Arduino.h>
#include <Wire.h>
#include "DataSource.h"

// I2C address when ADDR pin is tied to GND (datasheet Section 1)
#define VEML6030_ADDR       0x10

// Register addresses (datasheet Section 5, register map)
#define VEML6030_REG_CONF   0x00  // ALS configuration register
#define VEML6030_REG_ALS    0x04  // ALS data output register (16-bit)
#define VEML6030_REG_WHITE  0x05  // White channel data output

/*
 * SensorVEML6030 reads ambient light from the VEML6030 over I2C.
 * Teensy pins: 18 (SDA0), 19 (SCL0)
 * Pull-ups: 4.7k to 3.3V on both SDA and SCL
 *
 * Configured for outdoor sunlight (gain x1/8, integration time 25ms)
 * which gives a max range of ~120,000 lux.
 */
class SensorVEML6030 : public DataSource {
public:
  SensorVEML6030(void);

  // Configure sensor registers over I2C
  void init(void);

  // Read ALS register and convert to lux
  void read(void);

  String printState(void);
  size_t writeDataBytes(unsigned char * buffer, size_t idx);

  uint16_t alsRaw = 0;   // raw 16-bit ALS register value
  float lux = 0.0;       // converted to lux

  int lastExecutionTime = -1;

private:
  void writeRegister(uint8_t reg, uint16_t value);
  uint16_t readRegister(uint8_t reg);

  // Lux per ADC count — depends on gain and integration time setting.
  // At gain=1/8, IT=25ms this is 1.8432 (from datasheet Table 1).
  static constexpr float RESOLUTION = 1.8432;
};

#endif
