#pragma once

#include <Arduino.h>

class StorageManager {
 public:
  bool begin();
  void listBooks();

 private:
  bool mounted_ = false;
  bool listedOnce_ = false;
};
