/* HID Listen, http://www.pjrc.com/teensy/hid_listen.html
 * Listens (and prints) all communication received from a USB HID device,
 * which is useful for view debug messages from the Teensy USB Board.
 * Copyright 2008, PJRC.COM, LLC
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

#include <stdio.h>
#include <thread>
#include <algorithm>
#include <string>
#include "rawhid.h"
#include <map>

class DeviceList {
  std::map<std::string, std::shared_ptr<RawHid>> devices_;

public:
  void scanDeviceList() {
    auto devices = RawHid::listDevices(0, 0);

    for (auto &dev : devices) {
      auto it = devices_.find(dev->getId());
      if (it == devices_.end()) {
        devices_[dev->getId()] = dev;
        printf("[%s] attached\n", dev->getId().c_str());
      }
    }
  }

  size_t size() const {
    return devices_.size();
  }

  void stripEmbeddedNulBytes(std::string &str) {
    str.erase(std::remove(str.begin(), str.end(), 0), str.end());
  }

  // Try to collect together data that is printed around the same time,
  // so that the [id] prefix we emit looks reasonable.
  bool readDevice(const std::string &id, std::shared_ptr<RawHid> hid) {
    std::string fullText;
    std::string buf;

    if (!hid->read(buf, std::chrono::milliseconds(200))) {
      return false;
    }

    stripEmbeddedNulBytes(buf);
    while (!buf.empty()) {
      fullText.append(buf);

      if (!hid->read(buf, std::chrono::milliseconds(200))) {
        break;
      }

      stripEmbeddedNulBytes(buf);
    }

    if (!fullText.empty()) {
      // Print each line prefixed by the device id
      bool needId = true;

      for (auto &c : fullText) {
        if (needId) {
          printf("[%s] ", id.c_str());
          needId = false;
        }

        if (c == '\n') {
          needId = true;
        }

        fputc(c, stdout);
      }
      fflush(stdout);
    }
    return true;
  }

  void pollDevices() {
    if (devices_.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
			return;
		}

    std::string buf;
    std::vector<std::string> disconnected;

    for (auto &it : devices_) {
      auto &id = it.first;
      auto &hid = it.second;

      if (!readDevice(id, hid)) {
        // Remember that we're disconnecting; we can't erase inside
        // this loop because that will invalidate the iterator.
        disconnected.push_back(id);
        continue;
      }
    }

    if (!disconnected.empty()) {
      for (auto &id : disconnected) {
        devices_.erase(id);
        printf("[%s] detached\n", id.c_str());
      }
      if (devices_.empty()) {
        // Transitioning to having no devices; advise that we're
        // waiting for a device to be connected.
        printf("[info] waiting for device\n");
      }
    }
  }
};

int main(void)
{
  DeviceList devices;
  devices.scanDeviceList();
  if (devices.size() == 0) {
    // no devices at startup; advise that we're
    // waiting for a device to be connected.
    printf("[info] waiting for device\n");
  }
  while (1) {
    devices.pollDevices();
    devices.scanDeviceList();
  }
  return 0;
}
