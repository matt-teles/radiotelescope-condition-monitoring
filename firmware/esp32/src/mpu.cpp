#include "mpu.h"

#include <Arduino.h>
#include <Wire.h>

namespace
{

    // -----------------------------------------------------------------------------
    // MPU6050 I2C configuration
    // -----------------------------------------------------------------------------
    //
    // We are explicitly using GPIO4 and GPIO14 instead of the default I2C pins
    // defined by the WT32-ETH01 board variant.
    //
    constexpr uint8_t SDA_PIN = 4;
    constexpr uint8_t SCL_PIN = 14;

    // The MPU6050 responded at 0x68 in our previous I2C scanner test.
    constexpr uint8_t MPU_ADDR = 0x68;

    // -----------------------------------------------------------------------------
    // MPU6050 register addresses
    // -----------------------------------------------------------------------------
    //
    // These values come from the MPU6000/MPU6050 register map.
    //
    // SMPLRT_DIV:
    //     Divides the internal sample rate.
    //
    // CONFIG:
    //     Among other things, configures the digital low-pass filter (DLPF).
    //
    // PWR_MGMT_1:
    //     Controls sleep mode and clock source.
    //
    // ACCEL_XOUT_H:
    //     First register containing accelerometer output data.
    //
    // WHO_AM_I:
    //     Identification register.
    //
    constexpr uint8_t REG_SMPLRT_DIV = 0x19;
    constexpr uint8_t REG_CONFIG = 0x1A;
    constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
    constexpr uint8_t REG_WHO_AM_I = 0x75;

    // -----------------------------------------------------------------------------
    // Write one byte to an MPU6050 register
    // -----------------------------------------------------------------------------

    void writeRegister(uint8_t reg, uint8_t value)
    {
        // Begin an I2C transmission to the MPU6050.
        Wire.beginTransmission(MPU_ADDR);

        // First byte tells the MPU which register we want to modify.
        Wire.write(reg);

        // Second byte is the value that will be written into that register.
        Wire.write(value);

        // Finish and physically transmit the I2C transaction.
        Wire.endTransmission();
    }

    // -----------------------------------------------------------------------------
    // Read one byte from an MPU6050 register
    // -----------------------------------------------------------------------------

    uint8_t readRegister(uint8_t reg)
    {
        // Start talking to the MPU.
        Wire.beginTransmission(MPU_ADDR);

        // Tell the MPU which register we want to read.
        Wire.write(reg);

        // Finish the write phase without issuing a STOP condition.
        //
        // false causes a repeated START to be used before the read phase.
        Wire.endTransmission(false);

        // Request exactly one byte from the MPU.
        Wire.requestFrom(MPU_ADDR, static_cast<uint8_t>(1));

        // If one byte is available, return it.
        if (Wire.available())
        {
            return Wire.read();
        }

        // 0xFF is used here as a simple error value for this proof of concept.
        return 0xFF;
    }
} // namespace mpu

// -----------------------------------------------------------------------------
// Read one XYZ acceleration sample from the MPU6050
// -----------------------------------------------------------------------------

bool readAcceleration(int16_t &ax, int16_t &ay, int16_t &az)
{
    // The acceleration registers are contiguous:
    //
    // 0x3B ACCEL_XOUT_H
    // 0x3C ACCEL_XOUT_L
    // 0x3D ACCEL_YOUT_H
    // 0x3E ACCEL_YOUT_L
    // 0x3F ACCEL_ZOUT_H
    // 0x40 ACCEL_ZOUT_L
    //
    // Therefore, all three axes can be read with one I2C burst.

    Wire.beginTransmission(MPU_ADDR);

    // Select the first acceleration register.
    Wire.write(REG_ACCEL_XOUT_H);

    // Keep the bus active for the following read operation.
    //
    // A non-zero return value means the I2C transaction failed.
    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    // Three 16-bit axes require six bytes.
    //
    // X = 2 bytes
    // Y = 2 bytes
    // Z = 2 bytes
    //
    // Total = 6 bytes.
    if (Wire.requestFrom(MPU_ADDR, static_cast<uint8_t>(6)) != 6)
    {
        return false;
    }

    // Each axis is stored as two bytes:
    //
    //     HIGH byte | LOW byte
    //
    // The first byte is shifted eight positions to become the upper
    // half of the 16-bit value.
    //
    // Example:
    //
    //     HIGH = 10110110
    //     LOW  = 00101100
    //
    // becomes:
    //
    //     10110110 00101100
    //
    // The final type is int16_t because acceleration can be negative.
    ax = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
    ay = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
    az = static_cast<int16_t>((Wire.read() << 8) | Wire.read());

    return true;
}

// -----------------------------------------------------------------------------
// Configure the MPU6050
// -----------------------------------------------------------------------------

bool initializeMPU()
{
    // Start the ESP32 I2C controller using our chosen pins.
    Wire.begin(SDA_PIN, SCL_PIN);

    // Run the I2C bus at 400 kHz.
    //
    // This is the bus communication speed.
    // It is NOT the accelerometer sampling frequency.
    Wire.setClock(400000);

    // -------------------------------------------------------------------------
    // Confirm that an MPU6050 is responding.
    // -------------------------------------------------------------------------

    uint8_t whoAmI = readRegister(REG_WHO_AM_I);

    Serial.print("MPU WHO_AM_I: 0x");
    Serial.println(whoAmI, HEX);

    if (whoAmI != 0x68)
    {
        Serial.println("Unexpected MPU WHO_AM_I value.");
        return false;
    }

    // -------------------------------------------------------------------------
    // Wake the MPU6050.
    // -------------------------------------------------------------------------
    //
    // PWR_MGMT_1 defaults to sleep mode after reset.
    //
    // Writing 0x01:
    //
    //     SLEEP = 0
    //     CLKSEL = 1
    //
    // wakes the device and selects the X-axis gyroscope PLL as its clock
    // reference.
    //
    writeRegister(REG_PWR_MGMT_1, 0x01);

    delay(100);

    // -------------------------------------------------------------------------
    // Configure the internal sensor output rate.
    // -------------------------------------------------------------------------
    //
    // CONFIG = 0x01 sets DLPF_CFG = 1.
    //
    // With the DLPF enabled, the MPU's internal gyroscope output rate used by
    // the sample-rate divider is 1 kHz.
    //
    writeRegister(REG_CONFIG, 0x01);

    // -------------------------------------------------------------------------
    // Configure the sample-rate divider.
    // -------------------------------------------------------------------------
    //
    // The MPU6050 sample-rate relationship is:
    //
    //     Sample Rate = Internal Rate / (1 + SMPLRT_DIV)
    //
    // We configured the relevant internal rate to 1 kHz above.
    //
    // Therefore:
    //
    //     SMPLRT_DIV = 0
    //
    // gives:
    //
    //     1000 Hz / (1 + 0) = 1000 Hz
    //
    writeRegister(REG_SMPLRT_DIV, 0x00);

    Serial.println("MPU initialized at 1 kHz.");

    return true;
}