#pragma once

#include <stdint.h>

namespace adxl
{

    constexpr uint8_t FIFO_CAPACITY = 32;

    struct Sample
    {
        int16_t x;
        int16_t y;
        int16_t z;
    };

    bool begin(
        uint8_t sdaPin,
        uint8_t sclPin);

    bool setOdr(
        uint16_t odrHz);

    bool startMeasurement();

    bool stopMeasurement();

    bool enableFifoStream(
        uint8_t watermark);

    bool disableFifo();

    bool fifoEntries(
        uint8_t &entries);

    bool readAvailableSamples(
        Sample *buffer,
        uint8_t &samplesRead);

    bool readRaw(
        int16_t &x,
        int16_t &y,
        int16_t &z);

}