#pragma once

#include <cstddef>
#include <cstdint>

bool initializeEthernet();
bool connectToServer();

bool sendLine(const char *text);
bool sendBytes(const uint8_t *data, size_t length);