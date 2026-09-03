#include "adxl.h"

#include <Arduino.h>
#include <Wire.h>

namespace
{

    constexpr uint8_t ADXL_ADDR = 0x53;

    // -----------------------------------------------------------------------------
    // ADXL345 register addresses
    // -----------------------------------------------------------------------------

    constexpr uint8_t REG_DEVID = 0x00;

    constexpr uint8_t REG_BW_RATE = 0x2C;

    constexpr uint8_t REG_POWER_CTL = 0x2D;

    constexpr uint8_t REG_DATA_FORMAT = 0x31;

    constexpr uint8_t REG_DATAX0 = 0x32;

    constexpr uint8_t REG_FIFO_CTL = 0x38;

    constexpr uint8_t REG_FIFO_STATUS = 0x39;

    constexpr uint8_t EXPECTED_DEVID = 0xE5;

    // -----------------------------------------------------------------------------
    // Basic register access
    // -----------------------------------------------------------------------------

    bool writeRegister(
        uint8_t reg,
        uint8_t value)
    {
        Wire.beginTransmission(ADXL_ADDR);

        Wire.write(reg);

        Wire.write(value);

        return Wire.endTransmission() == 0;
    }

    bool readRegister(
        uint8_t reg,
        uint8_t &value)
    {
        Wire.beginTransmission(ADXL_ADDR);

        Wire.write(reg);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        if (
            Wire.requestFrom(
                ADXL_ADDR,
                static_cast<uint8_t>(1)) != 1)
        {
            return false;
        }

        value = Wire.read();

        return true;
    }

    // -----------------------------------------------------------------------------
    // Convert an output data rate to a BW_RATE register value
    // -----------------------------------------------------------------------------

    bool odrToRegister(
        uint16_t odrHz,
        uint8_t &value)
    {
        switch (odrHz)
        {
        case 100:

            value = 0x0A;

            return true;

        case 200:

            value = 0x0B;

            return true;

        case 400:

            value = 0x0C;

            return true;

        case 800:

            value = 0x0D;

            return true;

        case 1600:

            value = 0x0E;

            return true;

        case 3200:

            value = 0x0F;

            return true;

        default:

            return false;
        }
    }

}

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------

namespace adxl
{

    bool begin(
        uint8_t sdaPin,
        uint8_t sclPin)
    {
        Wire.begin(
            sdaPin,
            sclPin);

        // Run the I2C bus at 400 kHz.

        Wire.setClock(400000);

        // -------------------------------------------------------------------------
        // Confirm that an ADXL345 is responding.
        // -------------------------------------------------------------------------

        uint8_t deviceId;

        if (!readRegister(
                REG_DEVID,
                deviceId))
        {
            return false;
        }

        if (deviceId != EXPECTED_DEVID)
        {
            return false;
        }

        // -------------------------------------------------------------------------
        // Put the accelerometer into standby mode.
        // -------------------------------------------------------------------------

        if (!stopMeasurement())
        {
            return false;
        }

        // -------------------------------------------------------------------------
        // Disable the FIFO initially.
        // -------------------------------------------------------------------------

        if (!disableFifo())
        {
            return false;
        }

        // -------------------------------------------------------------------------
        // Configure full resolution and a +/-16 g range.
        //
        // DATA_FORMAT bit 3 enables full resolution.
        // DATA_FORMAT bits 1:0 select the +/-16 g range.
        //
        // 00001011 = 0x0B.
        // -------------------------------------------------------------------------

        if (!writeRegister(
                REG_DATA_FORMAT,
                0x0B))
        {
            return false;
        }

        return true;
    }

    bool setOdr(
        uint16_t odrHz)
    {
        uint8_t value;

        if (!odrToRegister(
                odrHz,
                value))
        {
            return false;
        }

        // Configure BW_RATE while the accelerometer is in standby mode.

        if (!stopMeasurement())
        {
            return false;
        }

        return writeRegister(
            REG_BW_RATE,
            value);
    }

    bool startMeasurement()
    {
        // POWER_CTL bit 3 enables measurement mode.

        return writeRegister(
            REG_POWER_CTL,
            0x08);
    }

    bool stopMeasurement()
    {
        // Clear the Measure bit to enter standby mode.

        return writeRegister(
            REG_POWER_CTL,
            0x00);
    }

    bool enableFifoStream(
        uint8_t watermark)
    {
        if (
            watermark == 0 ||
            watermark > 31)
        {
            return false;
        }

        /*
            FIFO_CTL

            Bits 7:6 = 10 selects Stream Mode.
            Bits 4:0 contain the watermark level.
        */

        uint8_t value =
            0x80 |
            (watermark & 0x1F);

        return writeRegister(
            REG_FIFO_CTL,
            value);
    }

    bool disableFifo()
    {
        // Select Bypass Mode.
        //
        // This also clears the FIFO.

        return writeRegister(
            REG_FIFO_CTL,
            0x00);
    }

    bool fifoEntries(
        uint8_t &entries)
    {
        uint8_t status;

        if (!readRegister(
                REG_FIFO_STATUS,
                status))
        {
            entries = 0;

            return false;
        }

        // Bits 5:0 contain the number of FIFO entries.

        entries = status & 0x3F;

        // The ADXL345 FIFO can never contain more than 32 samples.

        if (entries > FIFO_CAPACITY)
        {
            entries = 0;

            return false;
        }

        return true;
    }

    bool readRaw(
        int16_t &x,
        int16_t &y,
        int16_t &z)
    {
        Wire.beginTransmission(ADXL_ADDR);

        Wire.write(REG_DATAX0);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        // Read six bytes in one I2C burst:
        //
        //     X0 X1
        //     Y0 Y1
        //     Z0 Z1

        if (
            Wire.requestFrom(
                ADXL_ADDR,
                static_cast<uint8_t>(6)) != 6)
        {
            return false;
        }

        uint8_t xLow = Wire.read();

        uint8_t xHigh = Wire.read();

        uint8_t yLow = Wire.read();

        uint8_t yHigh = Wire.read();

        uint8_t zLow = Wire.read();

        uint8_t zHigh = Wire.read();

        x = static_cast<int16_t>(
            (static_cast<uint16_t>(xHigh) << 8) |
            xLow);

        y = static_cast<int16_t>(
            (static_cast<uint16_t>(yHigh) << 8) |
            yLow);

        z = static_cast<int16_t>(
            (static_cast<uint16_t>(zHigh) << 8) |
            zLow);

        return true;
    }

}