/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2023 Rink Springer
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <cstdint>
#include <optional>
#include "tusb.h"
#include "mouse.h"

namespace
{
    struct HidMouse
    {
        uint8_t dev_addr{};
        uint8_t instance{};
        hid_mouse_report_t prev_report = { };

        void processMouseReport(const hid_mouse_report_t& report);
    };

    std::optional<HidMouse> hidMouse;
}

void HidMouse::processMouseReport(const hid_mouse_report_t& report)
{
    uint8_t button{};
    if (report.buttons & MOUSE_BUTTON_LEFT) button |= mouse::ButtonLeft;
    if (report.buttons & MOUSE_BUTTON_RIGHT) button |= mouse::ButtonRight;
    if (report.buttons & MOUSE_BUTTON_MIDDLE) button |= mouse::ButtonMiddle;
    mouse::OnNewEvent({
        .delta_x = report.x,
        .delta_y = report.y,
        .button = button
    });
}


// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, const uint8_t* desc_report, uint16_t desc_len)
{
    const auto itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        printf("hid address %d instance %d: accepted boot mouse protocol\n", dev_addr, instance, itf_protocol);
        hidMouse.emplace(dev_addr, instance);

        // request to receive report
        // tuh_hid_report_received_cb() will be invoked when report is available
        if (!tuh_hid_receive_report(dev_addr, instance)) {
            printf("hid address %d instance %d: error: cannot request to receive report\n", dev_addr, instance);
        }
    } else {
        // TODO implement if we find a device that doesn't properly support boot protocol
        printf("hid address %d instance %d: ignoring uninteresting protocol %d\n", dev_addr, instance, itf_protocol);
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    if (hidMouse && hidMouse->dev_addr == dev_addr && hidMouse->instance == instance) {
        printf("hid: unmounted hid mouse, address %d, instance %d\n", dev_addr, instance);
        hidMouse.reset();
    } else {
        printf("hid: unmounted unrecognized device, address %d, instance %d\n", dev_addr, instance);
    }
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    assert(hidMouse);
    assert(hidMouse->dev_addr == dev_addr);
    assert(hidMouse->instance == instance);

    const auto itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            printf("hid: dev_addr %d instance %d, received boot keyboard report (ignoring)\n", dev_addr, instance);
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            // printf("hid: dev_addr %d instance %d, received boot mouse report\n", dev_addr, instance);
            hidMouse->processMouseReport(*reinterpret_cast<const hid_mouse_report_t*>(report));
            break;

        default:
            // Generic report requires matching ReportID and contents with previous parsed report info
            printf("hid: dev_addr %d instance %d, received generic report (ignoring)\n", dev_addr, instance);
            break;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("hid: error: cannot request to receive report\n");
    }
}
