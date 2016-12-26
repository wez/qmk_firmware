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

#if defined(__linux__)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rawhid.h"
#include <sys/poll.h>

/*************************************************************************/
/**                                                                     **/
/**                             Linux                                   **/
/**                                                                     **/
/*************************************************************************/

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include <algorithm>
#include <system_error>

class LinuxRawHid : public RawHid {
public:
  std::string devname;
	int fd;
	struct hidraw_devinfo info;
  hidraw_report_descriptor* desc = nullptr;
  std::vector<uint8_t> descBuffer;
  std::string name_;
  std::string phys_;
  std::string id_;

  LinuxRawHid(int fd, const char *devname) : devname(devname), fd(fd) {
    auto r = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (r < 0) {
      throw std::system_error(errno, std::generic_category(),
                               "HIDIOCGRAWINFO");
    }

    int len = 0;
    r = ioctl(fd, HIDIOCGRDESCSIZE, &len);
    if (r < 0) {
      throw std::system_error(errno, std::generic_category(),
                               "HIDIOCGRDESCSIZE");
    }

    descBuffer.resize(std::max(int(sizeof(hidraw_report_descriptor)), len), 0);
    desc = reinterpret_cast<hidraw_report_descriptor*>(descBuffer.data());
    desc->size = len;
    r = ioctl(fd, HIDIOCGRDESC, desc);
    if (r < 0) {
      throw std::system_error(errno, std::generic_category(), "HIDIOCGRDESC");
    }

    char buf[256];
    r = ioctl(fd, HIDIOCGRAWNAME(sizeof(buf)), buf);
    if (r > 0) {
      name_ = std::string(buf, r);
    }

    r = ioctl(fd, HIDIOCGRAWPHYS(sizeof(buf)), buf);
    if (r > 0) {
      phys_ = std::string(buf, r);
    }

    // Populate the ID string
    snprintf(buf, sizeof(buf), "%s_v%04x_p%04x_%s",
             getProductName().c_str(), getVendor(), getProduct(),
             phys_.c_str());
    id_ = buf;

    // Retain only the punctuation used in the physical address,
    // plus some alphanumerics.  This removes spaces from the product
    // name that we have at the start of this string.
    id_.erase(std::remove_if(id_.begin(), id_.end(),
                             [](char x) {
                               return x != '_' && x != ':' && x != '/' &&
                                      x != '.' && x != '-' && !std::isalnum(x);
                             }),
              id_.end());
  }

  const std::string &getProductName() const override { return name_; }

  const std::string &getId() const override { return phys_; }

  int getVendor() const override {
    return info.vendor;
  }

  int getProduct() const override {
    return info.product;
  }

  ~LinuxRawHid() {
    if (fd != -1) {
      close(fd);
    }
  }

  bool write(const void *buf, size_t bufsize,
                     std::chrono::milliseconds timeout) override {
    return ::write(fd, buf, bufsize);
  }

  bool read(std::string &result, std::chrono::milliseconds timeout) override {
    int num;

    result.clear();
    if (fd == -1) {
      return false;
    }

    char buf[64];

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, timeout.count()) != 1) {
      return true;
    }

    num = ::read(fd, buf, sizeof(buf));
    if (num < 0) {
      close(fd);
      fd = -1;
      return false;
    }
    result.append(buf, num);
    return true;
  }
};

std::vector<std::shared_ptr<RawHid>> RawHid::listDevices(int vid, int pid) {
  std::vector<std::shared_ptr<RawHid>> devices;
  char devname[512];
  const unsigned char signature[] = {0x06, 0x31, 0xFF, 0x09, 0x74};
  int i;

  for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
    snprintf(devname, sizeof(devname), "/dev/hidraw%d", i);
    auto fd = open(devname, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
      if (errno == ENOENT) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(), devname);
    }
    auto hid = std::make_shared<LinuxRawHid>(fd, devname);
    if (memcmp(hid->desc->value, signature, sizeof(signature)) == 0) {
      bool match = true;

      if (vid && vid != hid->info.vendor) {
        match = false;
      }
      if (pid && pid != hid->info.product) {
        match = false;
      }

      if (match) {
        devices.emplace_back(hid);
      }
    }
  }
  return devices;
}

#endif // linux
