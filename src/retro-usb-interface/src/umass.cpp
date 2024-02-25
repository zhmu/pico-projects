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
#include <cstdio>
#include <cstdint>
#include <optional>
#include <atomic>
#include <array>

#include "tusb.h"

namespace
{
    struct MassDevice
    {
        uint8_t dev_addr{};
        uint8_t lun{};
        std::atomic<bool> done{};

        void WaitUntilDone()
        {
            while(!done) {
                tuh_task();
            }
        }
    };
    std::array<uint8_t, 512> transferBuffer;

    std::optional<MassDevice> massDevice;

    bool msc_callback([[maybe_unused]] uint8_t dev_addr, [[maybe_unused]] const tuh_msc_complete_data_t* cb_data)
    {
        assert(massDevice);
        assert(massDevice->dev_addr == dev_addr);
        massDevice->done = true;
        return true;
    }
}

extern "C" void tuh_msc_mount_cb(uint8_t dev_addr)
{
    if (massDevice) {
        printf("umass: ignoring mount of device, address %d (already attached device %d)\n", dev_addr, massDevice->dev_addr);
        return;
    }
    printf("umass: mounted device, address %d\n", dev_addr);
    massDevice.emplace(dev_addr);

    scsi_inquiry_resp_t inquiry_resp;
    massDevice->done = false;
    tuh_msc_inquiry(massDevice->dev_addr, massDevice->lun, &inquiry_resp, msc_callback, 0);
    massDevice->WaitUntilDone();

    const auto block_count = tuh_msc_get_block_count(dev_addr, massDevice->lun);
    const auto block_size = tuh_msc_get_block_size(dev_addr, massDevice->lun);
    printf("umass: %lu blocks of %lu bytes, total size %lu MB\n", block_count, block_size, block_count / ((1024 * 1024) / block_size));
    if (block_size == transferBuffer.size()) {
        massDevice->done = false;
        tuh_msc_read10(massDevice->dev_addr, massDevice->lun, transferBuffer.data(), 0, 1, msc_callback, 0);
        massDevice->WaitUntilDone();

        int n = 0;
        for(const auto b: transferBuffer) {
            printf("%02x ",b);
            ++n;
            if(n == 16) {
                printf("\n");
                n = 0;
            }
        }
    } else {
        printf("umass: unsupported block size, giving up\n");
        massDevice.reset();
    }
}

void umass_read_sector(uint32_t sector_nr, uint8_t* buffer)
{
    massDevice->done = false;
    tuh_msc_read10(massDevice->dev_addr, massDevice->lun, buffer, sector_nr, 1, msc_callback, 0);
    massDevice->WaitUntilDone();
}

extern "C" void tuh_msc_umount_cb(uint8_t dev_addr)
{
    if (massDevice && massDevice->dev_addr == dev_addr) {
        printf("umass: unmounted storage device, adress %d\n", dev_addr);
        massDevice.reset();
    } else {
        printf("umass: ignoring unmount of device, adress %d\n", dev_addr);
    }
}