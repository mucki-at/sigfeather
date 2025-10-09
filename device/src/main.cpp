#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200);
}

void loop()
{
  Serial.println("Hello, World USB!");
  Serial1.println("Hello, World 1!");
  delay(100);
}

