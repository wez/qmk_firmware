#pragma once
#include <memory>
#include <vector>
#include <chrono>

class RawHid {
public:
  virtual ~RawHid() = default;
  // Returns the product name provided by the USB device
  virtual const std::string &getProductName() const = 0;
  // Returns the vendor ID
  virtual int getVendor() const = 0;
  // Returns the product ID
  virtual int getProduct() const = 0;
  // Returns an ID string that uniquely identifies the device, with some constraints:
  // - The ID is unique wrt. all other devices that are connected at the same time.
  // - Unplugging and reconnecting the device may result in a different ID being
  //   assigned to the same physical device, especially if connected to a different
  //   port.
  virtual const std::string &getId() const = 0;

  virtual bool read(std::string &result, std::chrono::milliseconds timeout) = 0;
  virtual bool write(const void *buf, size_t bufsize,
                     std::chrono::milliseconds timeout) = 0;

  static std::vector<std::shared_ptr<RawHid>>
  listDevices(int vid = 0, int pid = 0);
};

#define RawHidUsagePage 0xff31
#define RawHidUsage 0x0074
