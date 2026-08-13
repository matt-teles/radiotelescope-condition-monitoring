#include "network.h"

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>

namespace
{
    // -----------------------------------------------------------------------------
    // TCP server configuration
    // -----------------------------------------------------------------------------
    //
    // This is the IPv4 address of the laptop running TCP_server.py.
    //
    // The ESP32 acts as the TCP client.
    //
    const IPAddress SERVER_IP(192, 168, 15, 8);

    constexpr uint16_t SERVER_PORT = 5005;

    // TCP connection to the Python server.
    //
    // In Arduino-ESP32 2.x, WiFiClient is the generic lwIP TCP client class.
    // Despite its historical name, the TCP/IP stack can use the active Ethernet
    // interface provided by ETH.
    //
    WiFiClient client;

} // namespace

// -----------------------------------------------------------------------------
// Initialize the WT32-ETH01 Ethernet interface
// -----------------------------------------------------------------------------

bool initializeEthernet()
{
    Serial.println("Starting Ethernet...");

    // Because PlatformIO is configured with:
    //
    //     board = wt32-eth01
    //
    // the Arduino board variant already defines the WT32-ETH01 LAN8720 pins.
    //
    // Therefore ETH.begin() can use the board configuration directly.
    if (!ETH.begin())
    {
        Serial.println("Failed to start Ethernet.");
        return false;
    }

    Serial.print("Waiting for IP");

    // DHCP runs asynchronously.
    //
    // 0.0.0.0 means that the interface does not yet have a usable IPv4
    // address.
    while (ETH.localIP() == IPAddress(0, 0, 0, 0))
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();

    Serial.print("IP address: ");
    Serial.println(ETH.localIP());

    return true;
}

// -----------------------------------------------------------------------------
// Connect the ESP32 TCP client to the Python server
// -----------------------------------------------------------------------------

bool connectToServer()
{
    Serial.print("Connecting to ");
    Serial.print(SERVER_IP);
    Serial.print(":");
    Serial.println(SERVER_PORT);

    if (!client.connect(SERVER_IP, SERVER_PORT))
    {
        Serial.println("TCP connection failed.");
        return false;
    }

    Serial.println("TCP connected.");

    return true;
}

// -----------------------------------------------------------------------------
// Sends a line to the server adding \r\n
// Returns false if it failed to connect
// -----------------------------------------------------------------------------
bool sendLine(const char *text)
{
    if (!client.connected())
    {
        return false;
    }

    client.println(text);
    return true;
}

// -----------------------------------------------------------------------------
// Sends a byte sequence to the server
// Returns false if
//      - It failed to connect
//      - It failed to send all of the data
// -----------------------------------------------------------------------------
bool sendBytes(const uint8_t *data, size_t length)
{
    if (!client.connected())
    {
        return false;
    }

    return client.write(data, length) == length;
}