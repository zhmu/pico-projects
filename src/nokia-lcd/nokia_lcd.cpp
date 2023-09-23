/*-
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * Copyright (c) 2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "pico/stdlib.h"
#include <cstdio>
#include <array>
#include <initializer_list>
#include <random>
#include <string_view>
#include <limits>
#include <type_traits>
#include "pico/time.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

extern "C" {
#include "fonts/nokia-fonts.h"
}

namespace
{
    constexpr auto messages = std::to_array<std::string_view>({
        "Murder all cows",
        "Obey your elders",
        "We are listening",
        "We are everywhere",
        "Uncover the camera",
        "Submit",
        "Privacy is not an option",
        "GJ phone home",
    });
}

struct LcdPinConfig {
    static constexpr auto inline Backlight = 17;
    static constexpr auto inline Reset = 18;
    static constexpr auto inline ChipEnable = 19;
    static constexpr auto inline DataCommand = 20;
    static constexpr auto inline DataIn = 21;
    static constexpr auto inline Clock = 22;
};

template<typename Pin>
struct Lcd
{
    static constexpr int inline numRowPixels = 48;
    static constexpr int inline numColumnPixels = 84;
    std::array<uint8_t, (numRowPixels * numColumnPixels) / 8> data{};

    enum class DC { Data, Command };

    Lcd()
    {
        for (auto p: { Pin::Reset, Pin::ChipEnable, Pin::DataCommand, Pin::DataIn, Pin::Clock}) {
            gpio_init(p);
            gpio_set_dir(p, GPIO_OUT);
            gpio_put(p, 0);
        }
        gpio_set_function(Pin::Backlight, GPIO_FUNC_PWM);
        auto config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 4.f);
        const auto slice_num = pwm_gpio_to_slice_num(Pin::Backlight);
        pwm_init(slice_num, &config, true);
    }

    void SetBacklight(uint16_t level)
    {
        pwm_set_gpio_level(Pin::Backlight, level);
    }

    void SetChipEnable(bool enable)
    {
        gpio_put(Pin::ChipEnable, enable ? 0 : 1);
    }

    void SetDC(DC dc)
    {
        gpio_put(Pin::DataCommand, dc == DC::Command ? 0 : 1);
    }

    void SetData(bool enable)
    {
        gpio_put(Pin::DataIn, enable);
    }

    void SetClock(bool raise)
    {
        gpio_put(Pin::Clock, raise);
    }

    void Reset()
    {
        // Figure 13 - Serial bus reset function (/RES)
        SetChipEnable(true);
        gpio_put(Pin::Reset, 0); // enable

        // 12 - T_WL(RES) minimum 100 ns... this is way too long
        for(int n = 0; n < 10; ++n) {
            SetClock(true);
            sleep_ms(1);
            SetClock(false);
            sleep_ms(1);
        }
        gpio_put(Pin::Reset, 1);
        sleep_ms(10);

        // Chapter 13 - Application Information
        WriteCommand(0b00100001); // function set, PD=0, V=0, H=1
        WriteCommand(0b01001000); // set vop
        WriteCommand(0b00100000); // function set, PD=0, V=0, H=0
        WriteCommand(0b00001100); // display control, D=1, E=0 

        std::fill(data.begin(), data.end(), 0);
        //Update();
    }

    void PowerDown()
    {
        WriteCommand(0b00100100); // function set, PD=1, V=0, H=0
        std::fill(data.begin(), data.end(), 0);
        Update();
    }

    void PlotPixel(int x, int y)
    {
        data[x + (y / 8) * numColumnPixels] |= 1 << (y % 8);
    }

    void Update()
    {
        SetDC(DC::Data);
        for(size_t n = 0; n < data.size(); ++n) {
            WriteByte(data[n]);
        }
    }

    void WriteByte(uint8_t data)
    {
        // TODO We don't actually need to sleeep 1us, we can sleep 1ns...
        for(int n = 7; n >= 0; --n) {
            SetData((data & (1 << n)) != 0);
            SetClock(true);
            sleep_us(1);
            SetClock(false);
            sleep_us(1);
        }
    }

    void WriteCommand(uint8_t cmd)
    {
        SetDC(DC::Command);
        WriteByte(cmd);
    }

    void WriteData(uint8_t cmd)
    {
        SetDC(DC::Data);
        WriteByte(cmd);
    }
};

template<typename Lcd, typename GetCharFn>
void DrawText(Lcd& lcd, GetCharFn charFn, int x_origin, int y_origin, std::string_view sv)
{
    // Determine the resulting type of a call to charFn(char)
    using GetCharFnResult = std::invoke_result_t<GetCharFn, char>;
    using GlyphType = std::remove_pointer_t<GetCharFnResult>;

    // Determine number of bits and rows to render
    using RowValueType = std::remove_extent_t<decltype(GlyphType::rows)>;
    const auto numberOfBits = std::numeric_limits<RowValueType>::digits;

    // Calculate amount of glyph rows to render
    const auto numberOfRows = [&]() -> int {
        const auto rowCount = sizeof(GlyphType::rows) / sizeof(GlyphType::rows[0]);
        if (y_origin + rowCount >= Lcd::numRowPixels) {
            return Lcd::numRowPixels - y_origin;
        }
        return rowCount;
    }();

    for(auto s: sv) {
        const GlyphType* glyph = charFn(s);
        if (glyph == nullptr)
            continue;

        // Determine amount of glyph columns to render
        const auto width = [&]() -> int {
            if (x_origin + glyph->width > Lcd::numColumnPixels) {
                return Lcd::numColumnPixels - x_origin;
            }
            return glyph->width;
        }();

        for(int y = 0; y < numberOfRows; ++y) {
            const auto v = glyph->rows[y];
            for(int x = 0; x < width; ++x) {
                if (v & (1 << (numberOfBits - 1 - x))) {
                    lcd.PlotPixel(x_origin + x, y_origin + y);
                }
            }
        }
        x_origin += glyph->advance;
    }
}

template<typename GetCharFn>
int CalculateWidth(GetCharFn charFn, std::string_view sv)
{
    int width = 0;
    for(auto s: sv) {
        const auto glyph = charFn(s);
        if (glyph == nullptr)
            continue;

        width += glyph->advance;
    }
    return width;
}

int CalculateSmallFontWidth(std::string_view sv)
{
    return CalculateWidth(nokia_get_small_char, sv);
}

int CalculateBigFontWidth(std::string_view sv)
{
    return CalculateWidth(nokia_get_big_char, sv);
}

template<typename Lcd>
void DrawSmallText(Lcd& lcd, int x_origin, int y_origin, std::string_view sv)
{
    DrawText(lcd, nokia_get_small_char, x_origin, y_origin, sv);
}

template<typename Lcd>
void DrawBigText(Lcd& lcd, int x_origin, int y_origin, std::string_view sv)
{
    DrawText(lcd, nokia_get_big_char, x_origin, y_origin, sv);
}

template<typename Lcd>
void ClearLcd(Lcd& lcd)
{
    std::fill(lcd.data.begin(), lcd.data.end(), 0);
}

template<typename Lcd, typename Rng>
void GenerateNoise(Lcd& lcd, Rng& rng)
{
    std::uniform_int_distribution dist(0, 255);
    for(auto& byte: lcd.data) {
        byte = dist(rng);
    }
}

void EfficientDelayMs(int ms)
{
    sleep_ms(ms);
}

int main()
{
    std::mt19937 rng;

    Lcd<LcdPinConfig> nokiaLcd;

    nokiaLcd.Reset();

    for(;;)
    {
        // Step 1: noise screen
        for(int round = 0; round < 10; ++round) {
            GenerateNoise(nokiaLcd, rng);
            nokiaLcd.Update();
            EfficientDelayMs(100);
        }

        // Step 2: place text on screen
        std::uniform_int_distribution<> messagesDist(0, messages.size() - 1);
        std::string_view sv = messages[messagesDist(rng)];
        const auto w = CalculateSmallFontWidth(sv);
        ClearLcd(nokiaLcd);
        DrawSmallText(nokiaLcd, (nokiaLcd.numColumnPixels - w) / 2, (nokiaLcd.numRowPixels - 8) / 2, sv);
        nokiaLcd.Update();

        // Step 3: haunting backlight
        int value = 0;
        int delta = 16;
        for(int count = 0; count < 30; ++count) {
            value += delta;
            if (value <= 0 || value >= 255)
            {
                delta = -delta;
            }
            else
            {
                nokiaLcd.SetBacklight(value * value);
            }
            EfficientDelayMs(25);
        }

        // Step 4: shutdown
        nokiaLcd.SetBacklight(0);
        nokiaLcd.PowerDown();
        EfficientDelayMs(1000);
        nokiaLcd.Reset();
    }
}
