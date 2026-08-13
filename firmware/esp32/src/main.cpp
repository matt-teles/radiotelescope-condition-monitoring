#include "mpu.h"
#include "network.h"

#include <Arduino.h>
#include <Wire.h>
#include <ETH.h>
#include <WiFi.h>

// -----------------------------------------------------------------------------
// Acquisition timing
// -----------------------------------------------------------------------------
//
// 1 kHz means:
//
//     1000 samples / second
//
// Therefore:
//
//     1 second / 1000 = 1 millisecond per sample
//
// or:
//
//     1000 microseconds per sample
//
constexpr uint32_t SAMPLE_PERIOD_US = 1000;

// Sequence number assigned by the ESP32 to each transmitted sample.
//
// This allows the receiving side to check whether samples were lost or
// received out of sequence.
//
uint32_t sequence = 0;

// Timestamp at which the next sample should be acquired.
//
// This is initialized later, after the MPU and Ethernet are ready.
//
uint32_t nextSampleUs = 0;

// -----------------------------------------------------------------------------
// Arduino setup
// -----------------------------------------------------------------------------

void setup()
{
  // Serial is used only for diagnostics.
  //
  // Sensor samples themselves will be sent over Ethernet.
  Serial.begin(115200);

  // Keep the initial delay for now so that opening the serial monitor around
  // reset remains convenient during development.
  delay(2000);

  Serial.println();
  Serial.println("Starting sensor streaming proof of concept.");

  // -------------------------------------------------------------------------
  // Initialize the sensor first.
  // -------------------------------------------------------------------------

  if (!initializeMPU())
  {
    Serial.println("MPU initialization failed.");

    // Stop here because streaming meaningless data would hide the error.
    while (true)
    {
      delay(1000);
    }
  }

  // -------------------------------------------------------------------------
  // Initialize Ethernet.
  // -------------------------------------------------------------------------

  if (!initializeEthernet())
  {
    Serial.println("Ethernet initialization failed.");

    while (true)
    {
      delay(1000);
    }
  }

  // -------------------------------------------------------------------------
  // Establish the TCP connection.
  // -------------------------------------------------------------------------

  if (!connectToServer())
  {
    // For this first combined test we deliberately do not implement
    // reconnection logic yet.
    //
    // We want failures to remain obvious and easy to diagnose.
    while (true)
    {
      delay(1000);
    }
  }

  // Send a CSV header once.
  sendLine("seq,t_us,ax,ay,az");

  // Establish the first acquisition deadline.
  nextSampleUs = micros();

  Serial.println("Streaming started.");
}

// -----------------------------------------------------------------------------
// Arduino main loop
// -----------------------------------------------------------------------------

void loop()
{
  // Read the ESP32 microsecond clock.
  uint32_t now = micros();

  // -------------------------------------------------------------------------
  // Wait until the next scheduled acquisition instant.
  // -------------------------------------------------------------------------
  //
  // We intentionally do NOT use:
  //
  //     delay(1);
  //
  // because delay-based timing would make the real period equal to:
  //
  //     delay
  //     + I2C reading time
  //     + TCP transmission time
  //     + execution overhead
  //
  // Instead, we compare against an absolute schedule.
  //
  // The signed subtraction also keeps this comparison working when micros()
  // eventually wraps around.
  //
  if (static_cast<int32_t>(now - nextSampleUs) < 0)
  {
    return;
  }

  // Move the schedule forward by exactly one nominal sample period.
  //
  // Notice that this is:
  //
  //     nextSampleUs += 1000
  //
  // rather than:
  //
  //     nextSampleUs = now + 1000
  //
  // The first form avoids accumulating ordinary execution-time drift.
  uint32_t sampleTimestamp = nextSampleUs;
  nextSampleUs += SAMPLE_PERIOD_US;

  // Variables that will receive the raw signed 16-bit accelerometer values.
  int16_t ax;
  int16_t ay;
  int16_t az;

  // -------------------------------------------------------------------------
  // Acquire one real MPU6050 sample.
  // -------------------------------------------------------------------------

  if (!readAcceleration(ax, ay, az))
  {
    Serial.println("I2C read failed.");
    return;
  }

  // -------------------------------------------------------------------------
  // Build one CSV record.
  // -------------------------------------------------------------------------
  //
  // Example:
  //
  //     1234,58123000,-182,451,16421
  //
  // Meaning:
  //
  //     sequence = 1234
  //     timestamp = 58,123,000 us
  //     ax = -182
  //     ay = 451
  //     az = 16421
  //
  // A fixed char buffer is used instead of String so that this first
  // streaming implementation does not need dynamic heap allocation for every
  // sample.
  //
  // In the future we should consider both sending readings in batch and in a struct for efficiency
  //
  char message[96];

  int messageLength = snprintf(
      message,
      sizeof(message),
      "%lu,%lu,%d,%d,%d\n",
      static_cast<unsigned long>(sequence),
      static_cast<unsigned long>(sampleTimestamp),
      ax,
      ay,
      az);

  // -------------------------------------------------------------------------
  // Send the complete sample to the TCP socket.
  // -------------------------------------------------------------------------
  //
  // TCP is a byte stream. The newline character is our application-level
  // delimiter indicating the end of one sample record.
  //
  if (messageLength > 0)
  {
    sendBytes(
        reinterpret_cast<const uint8_t *>(message),
        static_cast<size_t>(messageLength));
  }

  // Increment only after the sample has been prepared for transmission.
  sequence++;
}