/* Raw HID I/O Routines
 * Copyright 2008, PJRC.COM, LLC
 * paul@pjrc.com
 *
 * You may redistribute this program and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */
#if defined(__APPLE__)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rawhid.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <unistd.h>
#include <deque>
#include <string>
#include <cctype>
#include <algorithm>

// http://developer.apple.com/technotes/tn2007/tn2187.html

// Helper for automatically tracking Core Foundation objects.
// This partially smart-pointer-ifies a CF*Ref so that we don't
// need to worry about calling retain/release at the right places.
template <typename T> class CFPointer {
public:
  CFPointer() : ref_(nullptr) {}

  CFPointer(T pointer) : ref_(toRefType(pointer)) {
    if (ref_) {
      CFRetain(ref_);
    }
  }

  CFPointer(CFTypeRef ref, CFTypeID matchingType) {
    if (ref && CFGetTypeID(ref) == matchingType) {
      ref_ = ref;
      CFRetain(ref_);
    }
  }

  CFPointer(const CFPointer &other) : ref_(other.ref_) {
    if (CFTypeRef ptr = ref_) {
      CFRetain(ptr);
    }
  }

  CFPointer(CFPointer &&other) : ref_(nullptr) {
    std::swap(ref_, other.ref_);
  }

  ~CFPointer() {
    reset();
  }

  void reset() {
    if (CFTypeRef pointer = ref_) {
      CFRelease(pointer);
    }
  }

  static inline CFPointer<T> adopt(T CF_RELEASES_ARGUMENT ptr) {
    return CFPointer<T>(ptr, CFPointer<T>::Adopt);
  }

  inline T get() const { return fromRefType(ref_); }
  CFPointer &operator=(CFPointer other) {
    swap(other);
    return *this;
  }

  // Implicit conversion to T
  operator T() const {
    return get();
  }

private:
  CFTypeRef ref_;

  enum AdoptTag { Adopt };
  CFPointer(T ptr, AdoptTag) : ref_(toRefType(ptr)) {}

  inline CFTypeRef toRefType(CFTypeRef ptr) const { return CFTypeRef(ptr); }

  inline T fromRefType(CFTypeRef pointer) const { return T(pointer); }

  void swap(CFPointer &other) { std::swap(ref_, other.ref_); }
};

// Automatically deduces the CFPointer return type for the type of
// the argument, so that you can just do:
// auto foo = adopt_cfpointer(SomeAPICall(...))
template <typename T>
CFPointer<T> adopt_cfpointer(T CF_RELEASES_ARGUMENT ptr) {
  return CFPointer<T>::adopt(ptr);
}

static const constexpr size_t kBufferSize = 0x1000;

std::string CFStringToUTF8(CFPointer<CFStringRef> ref) {
  if (!ref) {
    return "";
  }

  auto len = CFStringGetLength(ref);
  std::string result;
  result.resize(len * 4, 0);

  CFRange range;
  range.location = 0;
  range.length = len;

  CFIndex used_len = 0;
  auto utf8_len = CFStringGetBytes(ref, range, kCFStringEncodingUTF8,
      '?', false, (UInt8*)&result[0], result.size(), &used_len);

  result.resize(utf8_len);
  return result;
}

int32_t CFNumberToInt(CFPointer<CFNumberRef> ref) {
  int32_t val;
  CFNumberGetValue(CFNumberRef(ref), kCFNumberSInt32Type, &val);
  return val;
}

int64_t CFNumberToInt64(CFPointer<CFNumberRef> ref) {
  int64_t val;
  CFNumberGetValue(CFNumberRef(ref), kCFNumberSInt64Type, &val);
  return val;
}

class AppleRawHid : public RawHid {
  public:
  // The manager needs to outlive ref
  CFPointer<IOHIDManagerRef> mgr;
  // The device that we're talking to
  CFPointer<IOHIDDeviceRef> ref;
  bool disconnected = false;
  uint8_t buffer_[kBufferSize];
  std::string name_;
  std::string id_;

  const std::string &getProductName() const override { return name_; }

  const std::string &getId() const override { return id_; }

  int getVendor() const override {
    return CFNumberToInt(CFPointer<CFNumberRef>(
        IOHIDDeviceGetProperty(ref, CFSTR(kIOHIDVendorIDKey)),
        CFNumberGetTypeID()));
  }

  int getProduct() const override {
    return CFNumberToInt(CFPointer<CFNumberRef>(
        IOHIDDeviceGetProperty(ref, CFSTR(kIOHIDProductIDKey)),
        CFNumberGetTypeID()));
  }

  struct HidReport {
    IOReturn result;
    IOHIDReportType type;
    uint32_t reportID;
    std::string report;

    HidReport(IOReturn result, IOHIDReportType type, uint32_t reportID,
              uint8_t *reportBytes, CFIndex reportLength)
        : result(result), type(type), reportID(reportID),
          report((char *)reportBytes, reportLength) {}
  };

  std::deque<HidReport> reports;

  AppleRawHid(CFPointer<IOHIDManagerRef> mgr, CFPointer<IOHIDDeviceRef> ref)
      : mgr(mgr), ref(ref) {
    // open the first device in the list
    auto ret = IOHIDDeviceOpen(ref, kIOHIDOptionsTypeNone);
    if (ret != kIOReturnSuccess) {
      throw std::runtime_error("HID/macos: error opening device\n");
    }

    name_ = CFStringToUTF8(CFPointer<CFStringRef>(
        IOHIDDeviceGetProperty(ref, CFSTR(kIOHIDProductKey)),
        CFStringGetTypeID()));

    // Populate the ID string
    char idbuf[256];
    snprintf(idbuf, sizeof(idbuf), "%s_v%04x_p%04x_loc%llx",
             getProductName().c_str(), getVendor(), getProduct(),
             CFNumberToInt64(CFPointer<CFNumberRef>(
                 IOHIDDeviceGetProperty(ref, CFSTR(kIOHIDLocationIDKey)),
                 CFNumberGetTypeID())));
    id_ = idbuf;

    // The product name probably has spaces in it; strip those out leaving
    // only alpha numerics and underscores.
    id_.erase(
        std::remove_if(id_.begin(), id_.end(),
                       [](char x) { return x != '_' && !std::isalnum(x); }),
        id_.end());

    // register a callback to receive input
    IOHIDDeviceRegisterInputReportCallback(ref, buffer_, sizeof(buffer_),
                                           input_callback, this);

    // register a callback to find out when it's unplugged
    IOHIDDeviceScheduleWithRunLoop(ref, CFRunLoopGetCurrent(),
                                   kCFRunLoopDefaultMode);
    IOHIDDeviceRegisterRemovalCallback(ref, unplug_callback, this);
  }

  ~AppleRawHid() {
    IOHIDDeviceUnscheduleFromRunLoop(ref, CFRunLoopGetCurrent(),
                                     kCFRunLoopDefaultMode);
    IOHIDDeviceRegisterRemovalCallback(ref, NULL, NULL);
    IOHIDDeviceClose(ref, kIOHIDOptionsTypeNone);
  }

  static void unplug_callback(void *hidptr, IOReturn ret, void *ref) {
    auto hid = static_cast<AppleRawHid *>(hidptr);
    hid->disconnected = true;
  }

  static void input_callback(void *context, IOReturn result, void *sender,
                             IOHIDReportType type, uint32_t reportID,
                             uint8_t *report, CFIndex reportLength) {
    auto hid = static_cast<AppleRawHid *>(context);
    hid->reports.emplace_back(result, type, reportID, report, reportLength);
  }

  bool read(std::string &result, std::chrono::milliseconds timeout) override {
    result.clear();
    if (disconnected) {
      return false;
    }

    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) ==
           kCFRunLoopRunHandledSource) {
      if (!reports.empty()) {
        break;
      }
      if (disconnected) {
        return false;
      }
    }

    if (reports.empty()) {
      CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                         (double)timeout.count() / 1000.0, true);
      if (reports.empty()) {
        return true;
      }
    }

    auto &rep = reports.front();
    result = std::move(rep.report);
    reports.pop_front();

    return true;
  }

  bool write(const void *buf, size_t len,
             std::chrono::milliseconds timeout) override {
    if (disconnected) {
      return false;
    }
    auto ret =
        IOHIDDeviceSetReport(ref, kIOHIDReportTypeOutput, 0,
                             reinterpret_cast<const uint8_t *>(buf), len);
    if (ret != kIOReturnSuccess) {
      return false;
    }
    return true;
  }
};

std::vector<std::shared_ptr<RawHid>> RawHid::listDevices(int vid, int pid) {
  std::vector<std::shared_ptr<RawHid>> devices;
  auto hid_manager = adopt_cfpointer(
      IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
  if (hid_manager == nullptr) {
    throw std::runtime_error("HID/macos: unable to access HID manager");
  }
  // configure it to look for our type of device
  auto dict = adopt_cfpointer(IOServiceMatching(kIOHIDDeviceKey));
  if (dict == nullptr) {
    throw std::runtime_error("HID/macos: unable to create iokit dictionary");
  }
  if (vid > 0) {
    CFDictionarySetValue(
        dict, CFSTR(kIOHIDVendorIDKey),
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid));
  }
  if (pid > 0) {
    CFDictionarySetValue(
        dict, CFSTR(kIOHIDProductIDKey),
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid));
  }

  int usage_page = RawHidUsagePage;
  CFDictionarySetValue(
      dict, CFSTR(kIOHIDPrimaryUsagePageKey),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_page));
  int usage = RawHidUsage;
  CFDictionarySetValue(
      dict, CFSTR(kIOHIDPrimaryUsageKey),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage));
  IOHIDManagerSetDeviceMatching(hid_manager, dict);

  // now open the HID manager
  auto ret = IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone);
  if (ret != kIOReturnSuccess) {
    throw std::runtime_error(
        "HID/macos: Unable to open HID manager (IOHIDManagerOpen failed)");
  }
  // get a list of devices that match our requirements
  auto device_set = adopt_cfpointer(IOHIDManagerCopyDevices(hid_manager));
  if (device_set == NULL) {
    return devices;
  }
  auto num_devices = CFSetGetCount(device_set);

  // Copy the set of devices into a vector
  std::vector<IOHIDDeviceRef> device_list;
  device_list.resize(num_devices);
  CFSetGetValues(device_set, (const void **)device_list.data());

  for (auto &dev : device_list) {
    auto hid = std::make_shared<AppleRawHid>(hid_manager, dev);
    devices.emplace_back(hid);
  }

  return devices;
}

#endif // Darwin - Mac OS X
