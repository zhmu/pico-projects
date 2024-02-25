/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2023 Rink Springer
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
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

template<size_t Capacity, typename Element = uint8_t>
class Fifo
{
    std::array<Element, Capacity> buffer{};
    size_t readOffset = 0;
    size_t writeOffset = 0;

public:
    bool empty() const
    {
        return readOffset == writeOffset;
    }

    void clear()
    {
        readOffset = writeOffset;
    }

    bool full() const
    {
        return bytes_left() <= 1;
    }

    size_t bytes_left() const
    {
        if (readOffset == writeOffset) {
            return 0;
        } else if (readOffset < writeOffset) {
            return writeOffset - readOffset;
        } else {
            return (buffer.size() - readOffset) + writeOffset;
        }
    }

    auto peek(size_t offset = 0) const
    {
        return buffer[(readOffset + offset) % buffer.size()];
    }

    void drop(size_t amount)
    {
        readOffset = (readOffset + amount) % buffer.size();
    }

    auto pop()
    {
        auto value = std::move(buffer[readOffset]);
        drop(1);
        return value;
    }

    void push(Element&& value)
    {
        buffer[writeOffset] = std::move(value);
        writeOffset = (writeOffset + 1) % buffer.size();
    }
};