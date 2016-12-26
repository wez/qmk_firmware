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


// This code will someday be turned into "librawhid", an easy-to-use
// and truly cross platform library for accessing HID reports.  But
// there are many complexities not properly handled by this simple
// code that would be expected from a high quality library.  In
// particular, how report IDs are handled is not uniform on the 3
// platforms.  The mac code uses a single buffer which assumes no
// other functions can cause the "run loop" to process HID callbacks.
// The linux version doesn't extract usage and usage page from the
// report descriptor and just hardcodes a signature for the Teensy
// USB debug example.  Lacking from all platforms are functions to
// manage multiple devices and robust detection of device removal
// and attachment.  There are probably lots of other issues... this
// code has really only been used in 2 projects.  If you use it,
// please report bugs to paul@pjrc.com


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rawhid.h"


/*************************************************************************/
/**                                                                     **/
/**                     Windows 2000/XP/Vista                           **/
/**                                                                     **/
/*************************************************************************/

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#include <ddk/hidsdi.h>
#include <ddk/hidclass.h>

// http://msdn.microsoft.com/en-us/library/ms790932.aspx

struct rawhid_struct {
	HANDLE handle;
};


rawhid_t * rawhid_open_only1(int vid, int pid)
{
	GUID guid;
	HDEVINFO info;
	DWORD index=0, required_size;
	SP_DEVICE_INTERFACE_DATA iface;
	SP_DEVICE_INTERFACE_DETAIL_DATA *details;
	HIDD_ATTRIBUTES attrib;
	PHIDP_PREPARSED_DATA hid_data;
	HIDP_CAPS capabilities;
	struct rawhid_struct *hid;
	HANDLE h;
	BOOL ret;


	HidD_GetHidGuid(&guid);
	info = SetupDiGetClassDevs(&guid, NULL, NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (info == INVALID_HANDLE_VALUE) {
		printf("HID/win32: SetupDiGetClassDevs failed");
		return NULL;
	}
	for (index=0; ;index++) {
		iface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		ret = SetupDiEnumDeviceInterfaces(info, NULL, &guid, index, &iface);
		if (!ret) {
			// end of list
			SetupDiDestroyDeviceInfoList(info);
			return NULL;
		}
		SetupDiGetInterfaceDeviceDetail(info, &iface, NULL, 0, &required_size, NULL);
		details = (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(required_size);
		if (details == NULL) continue;
		memset(details, 0, required_size);
		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		ret = SetupDiGetDeviceInterfaceDetail(info, &iface, details,
			required_size, NULL, NULL);
		if (!ret) {
			free(details);
			continue;
		}
		h = CreateFile(details->DevicePath, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED, NULL);
		free(details);
		if (h == INVALID_HANDLE_VALUE) continue;
		attrib.Size = sizeof(HIDD_ATTRIBUTES);
		ret = HidD_GetAttributes(h, &attrib);
		if (!ret) {
			CloseHandle(h);
			continue;
		}
		//printf("HID/win32:   USB Device:\n");
		//printf("HID/win32:     vid =        0x%04X\n", (int)(attrib.VendorID));
		//printf("HID/win32:     pid =        0x%04X\n", (int)(attrib.ProductID));
		if (vid > 0 && vid != (int)(attrib.VendorID)) {
			CloseHandle(h);
			continue;
		}
		if (pid > 0 && pid != (int)(attrib.ProductID)) {
			CloseHandle(h);
			continue;
		}
		if (!HidD_GetPreparsedData(h, &hid_data)) {
			printf("HID/win32: HidD_GetPreparsedData failed\n");
			CloseHandle(h);
			continue;
		}
		if (!HidP_GetCaps(hid_data, &capabilities)) {
			printf("HID/win32: HidP_GetCaps failed\n");
			HidD_FreePreparsedData(hid_data);
			CloseHandle(h);
			continue;
		}
		//printf("HID/win32:     usage_page = 0x%04X\n", (int)(capabilities.UsagePage));
		//printf("HID/win32:     usage      = 0x%04X\n", (int)(capabilities.Usage));
		if (RawHidUsagePage != (int)(capabilities.UsagePage)) {
			HidD_FreePreparsedData(hid_data);
			CloseHandle(h);
			continue;
		}
		if (RawHidUsage != (int)(capabilities.Usage)) {
			HidD_FreePreparsedData(hid_data);
			CloseHandle(h);
			continue;
		}
		HidD_FreePreparsedData(hid_data);
		hid = (struct rawhid_struct *)malloc(sizeof(struct rawhid_struct));
		if (!hid) {
			CloseHandle(h);
			printf("HID/win32: Unable to get %d bytes", sizeof(struct rawhid_struct));
			continue;
		}
		hid->handle = h;
		return hid;
	}
}


int rawhid_status(rawhid_t *hid)
{
	PHIDP_PREPARSED_DATA hid_data;

	if (!hid) return -1;
	if (!HidD_GetPreparsedData(((struct rawhid_struct *)hid)->handle, &hid_data)) {
		printf("HID/win32: HidD_GetPreparsedData failed, device assumed disconnected\n");
		return -1;
	}
	printf("HID/win32: HidD_GetPreparsedData ok, device still online :-)\n");
	HidD_FreePreparsedData(hid_data);
	return 0;
}

void rawhid_close(rawhid_t *hid)
{
	if (!hid) return;
	CloseHandle(((struct rawhid_struct *)hid)->handle);
	free(hid);
}

int rawhid_read(rawhid_t *h, void *buf, int bufsize, int timeout_ms)
{
	DWORD num=0, result;
	BOOL ret;
	OVERLAPPED ov;
	struct rawhid_struct *hid;
	int r;

	hid = (struct rawhid_struct *)h;
	if (!hid) return -1;

	memset(&ov, 0, sizeof(OVERLAPPED));
	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ov.hEvent == NULL) return -1;

	ret = ReadFile(hid->handle, buf, bufsize, &num, &ov);
	if (ret) {
		//printf("HID/win32:   read success (immediate)\n");
		r = num;
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			result = WaitForSingleObject(ov.hEvent, timeout_ms);
			if (result == WAIT_OBJECT_0) {
				if (GetOverlappedResult(hid->handle, &ov, &num, FALSE)) {
					//printf("HID/win32:   read success (delayed)\n");
					r = num;
				} else {
					//printf("HID/win32:   read failure (delayed)\n");
					r = -1;
				}
			} else {
				//printf("HID/win32:   read timeout, %lx\n", result);
				CancelIo(hid->handle);
				r = 0;
			}
		} else {
			//printf("HID/win32:   read error (immediate)\n");
			r = -1;
		}
	}
	CloseHandle(ov.hEvent);
	return r;
}


int rawhid_write(rawhid_t *h, const void *buf, int len, int timeout_ms)
{
	DWORD num=0;
	BOOL ret;
	OVERLAPPED ov;
	struct rawhid_struct *hid;
	int r;

	hid = (struct rawhid_struct *)h;
	if (!hid) return -1;

	memset(&ov, 0, sizeof(OVERLAPPED));
	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ov.hEvent == NULL) return -1;

	// first byte is report ID, must be zero if report IDs not used
	ret = WriteFile(hid->handle, buf, len, &num, &ov);
	if (ret) {
		if (num == len) {
			//printf("HID/win32:   write success (immediate)\n");
			r = 0;
		} else {
			//printf("HID/win32:   partial write (immediate)\n");
			r = -1;
		}
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (GetOverlappedResult(hid->handle, &ov, &num, TRUE)) {
				if (num == len) {
					//printf("HID/win32:   write success (delayed)\n");
					r = 0;
				} else {
					//printf("HID/win32:   partial write (delayed)\n");
					r = -1;
				}
			} else {
				//printf("HID/win32:   write error (delayed)\n");
				r = -1;
			}
		} else {
			//printf("HID/win32:   write error (immediate)\n");
			r = -1;
		}
	}
	CloseHandle(ov.hEvent);
	return r;
}


#endif // windows
