#pragma once

#include <cstdint>

bool initializeMPU();
bool readAcceleration(int16_t &ax, int16_t &ay, int16_t &az);