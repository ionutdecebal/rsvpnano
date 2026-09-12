#pragma once

constexpr int INPUT_PULLUP = 2;
void pinMode(int pin, int mode);
int digitalRead(int pin);
void delay(unsigned long milliseconds);
