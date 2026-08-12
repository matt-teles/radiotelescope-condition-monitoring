#include <Arduino.h>
#include <ETH.h>

IPAddress serverIP(192, 168, 15, 8);
const uint16_t serverPort = 5005;
int messageCount = 0;

WiFiClient client;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting Ethernet...");

  ETH.begin(
      1,  // PHY address
      16, // PHY power
      23, // MDC
      18, // MDIO
      ETH_PHY_LAN8720,
      ETH_CLOCK_GPIO0_IN);

  Serial.print("Waiting for IP");

  while (ETH.localIP() == IPAddress(0, 0, 0, 0))
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(ETH.localIP());

  Serial.print("Connecting to server...");

  while (!client.connect(serverIP, serverPort))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();
  Serial.println("TCP connected.");
}

void loop()
{
  if (messageCount >= 10)
  {
    Serial.println("Closing TCP connection...");
    client.stop();
    Serial.println("Connection closed.");

    /* Stop all other commands until reset*/
    while (true)
    {
      delay(1000);
    }
  }

  client.println("hello");
  messageCount++;
  delay(1000);
}