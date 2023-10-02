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
#include <charconv>
#include <random>
#include <span>
#include <string_view>
#include <algorithm>
#include <limits>
#include <type_traits>
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"
#include "hardware/sync.h"
#include "hardware/pll.h"
#include "hardware/spi.h"

#include "pico/sleep.h"

#include "eye.h"

extern "C" {
#include "fonts/nokia-fonts.h"
}

namespace
{
    constexpr auto messages = std::to_array<std::string_view>({
        "Eet meer vlees",
        "Obey your elders",
        "We are listening",
        "We are everywhere",
        "Uncover the camera",
        "Submit",
        "Privacy is not an option",
        "GJ phone home",
        "Het duurt niet lang meer",
        "Dat ging maar net goed",
        "5G maakt het mogelijk",
        "#0" // Eye
        "Alles op X is nep",
        "X doesn't mark the spot",
        "Niemand gelooft je",
        "The truth is lost to us",
        "The matrix is a3@*792 lie",
        "The cake is real",
        "Was dat nu de rode of groene pil?",
        "Platte televisies geven ook straling",
        "Breedbeeld is een leugen",
        "We zijn nooit op de maan geweest",
        "De Illuminatie zitten op Tinder",
        "Alles op Facebook is echt",
        "Studie is intellectuele uitdaging voor het kuddevolk",
        "Iedereen weet waar je bent",
        "Niemand volgt je elke dag",
        "Consumeer, voordat het op is",
        "Ik luister mee",
        "Wat denk je nu echt?",
        "Wie luistert er dan niet mee?",
        "Zonnepanelen ontvangen ook signalen"
    });
    std::uniform_int_distribution<> messagesDist(0, messages.size() - 1);

    // Seconds slept between messages
    std::uniform_int_distribution<> sleepSecondsDist(30, 90);
    // std::uniform_int_distribution<> sleepSecondsDist(3, 5);
}

struct LcdPinConfig {
    static const auto inline SpiPeripheral = spi0;
    static constexpr auto inline Backlight = 16;
    static constexpr auto inline Reset = 20;
    static constexpr auto inline ChipEnable = 17; // SPI0 CSn
    static constexpr auto inline DataCommand = 21;
    static constexpr auto inline DataIn = 19; // SPI0 TX
    static constexpr auto inline Clock = 18; // SPI0 SCK
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
        for (auto p: { Pin::Reset, Pin::ChipEnable, Pin::DataCommand}) {
            gpio_init(p);
            gpio_set_dir(p, GPIO_OUT);
            gpio_put(p, 0);
        }
        gpio_set_function(Pin::Backlight, GPIO_FUNC_PWM);
        auto config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 4.f);
        const auto slice_num = pwm_gpio_to_slice_num(Pin::Backlight);
        pwm_init(slice_num, &config, true);

        spi_init(Pin::SpiPeripheral, 1'000'000);
        spi_set_format(Pin::SpiPeripheral, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(Pin::Clock, GPIO_FUNC_SPI);
        gpio_set_function(Pin::DataIn, GPIO_FUNC_SPI);
    }

    void SetBacklight(uint16_t level)
    {
        pwm_set_gpio_level(Pin::Backlight, level);
    }

    void SetDC(DC dc)
    {
        gpio_put(Pin::DataCommand, dc == DC::Command ? 0 : 1);
    }

    void SetChipEnable(bool enable)
    {
        gpio_put(Pin::ChipEnable, enable ? 0 : 1);
    }

    void Reset()
    {
        // Figure 13 - Serial bus reset function (/RES)
        SetChipEnable(true);
        gpio_put(Pin::Reset, 0); // enable

        // 12 - T_WL(RES) minimum 100 ns... this is way too long
        for(int n = 0; n < 10; ++n) {
           const uint8_t dummy = 0;
           spi_write_blocking(Pin::SpiPeripheral, &dummy, 1);
        }
        gpio_put(Pin::Reset, 1);
        sleep_ms(10);

        // Chapter 13 - Application Information
        WriteCommand(0b00100001); // function set, PD=0, V=0, H=1
        WriteCommand(0b01001000); // set vop
        WriteCommand(0b00100000); // function set, PD=0, V=0, H=0
        WriteCommand(0b00001100); // display control, D=1, E=0 

        std::fill(data.begin(), data.end(), 0);
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
        spi_write_blocking(Pin::SpiPeripheral, data.data(), data.size());
    }

    void WriteCommand(uint8_t cmd)
    {
        SetDC(DC::Command);
        spi_write_blocking(Pin::SpiPeripheral, &cmd, 1);
    }
};

template<typename GetGlyphFn>
class Font
{
    using GetGlyphFnResult = std::invoke_result_t<GetGlyphFn, char>;
    using GlyphType = std::remove_pointer_t<GetGlyphFnResult>;
    using RowValueType = std::remove_extent_t<decltype(GlyphType::rows)>;

public:
    static inline constexpr auto FontHeight = sizeof(GlyphType::rows) / sizeof(GlyphType::rows[0]);
    static inline constexpr auto BitsPerGlyphRow = std::numeric_limits<RowValueType>::digits;

    static const auto GetGlyphWidth(GetGlyphFn getGlyph, char ch)
    {
        const auto glyph = getGlyph(ch);
        return glyph ? glyph->advance : 0;
    }
};

template<typename Lcd, typename GetGlyphFn>
void DrawText(Lcd& lcd, GetGlyphFn getGlyph, int x_origin, int y_origin, std::string_view sv)
{
    using TheFont = Font<GetGlyphFn>;

    // Calculate amount of pixel rows to render (performs clipping)
    const auto numberOfRows = [&]() -> int {
        constexpr auto rowCount = TheFont::FontHeight;
        if (y_origin + rowCount >= Lcd::numRowPixels) {
            return Lcd::numRowPixels - y_origin;
        }
        return rowCount;
    }();

    const auto numberOfBits = TheFont::BitsPerGlyphRow;
    for(auto ch: sv) {
        const auto glyph = getGlyph(ch);
        if (glyph == nullptr)
            continue;

        // Determine amount of glyph columns to render (performs clipping)
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

struct WidthAndSpan {
    int width;
    int start_offset;
    int end_offset;
};

template<typename GetGlyphFn>
auto SplitTextInWords(GetGlyphFn glyphFn, std::string_view sv)
{
    auto isSpace = [](const char ch) { return ch == ' '; };

    std::vector<WidthAndSpan> words;
    for (size_t current_index = 0; current_index < sv.size(); ) {
        int current_index_width = 0;
        auto next_index = current_index;
        while(next_index < sv.size() && !isSpace(sv[next_index])) {
            current_index_width += Font<GetGlyphFn>::GetGlyphWidth(glyphFn, sv[next_index]);
            ++next_index;
        }

        words.emplace_back(current_index_width, current_index, next_index);
        while(next_index < sv.size() && isSpace(sv[next_index]))
            ++next_index;
        current_index = next_index;
    }
    return words;
}

template<typename Lcd, typename GetGlyphFn>
auto CombineWordsToLines(Lcd& lcd, GetGlyphFn glyphFn, std::span<const WidthAndSpan> words)
{
    const auto space_width = Font<GetGlyphFn>::GetGlyphWidth(glyphFn, ' ');

    std::vector<WidthAndSpan> lines;
    for(size_t current_word_index = 0; current_word_index < words.size(); ) {
        // Always place the first word in the line, no matter how wide it is
        int current_width = words[current_word_index].width;
        const auto start_offset = words[current_word_index].start_offset;
        int end_offset = words[current_word_index].end_offset;
        ++current_word_index;

        // Try to place as many words as will fit
        while(current_word_index < words.size()) {
            const auto word_width = space_width + words[current_word_index].width;
            if (current_width + word_width >= Lcd::numColumnPixels)
                break;
            current_width += word_width;
            end_offset = words[current_word_index].end_offset;
            ++current_word_index;
        }

        lines.emplace_back(current_width, start_offset, end_offset);
    }
    return lines;
}

template<typename Lcd, typename GetGlyphFn>
void CenterText(Lcd& lcd, GetGlyphFn getGlyph, std::string_view sv, std::span<const WidthAndSpan> lines)
{
    constexpr auto font_height = Font<GetGlyphFn>::FontHeight;
    int current_y = (Lcd::numRowPixels - (lines.size() * font_height)) / 2;
    for(const auto& line: lines) {
        const auto current_x = (Lcd::numColumnPixels - line.width) / 2;
        DrawText(lcd, getGlyph, current_x, current_y, sv.substr(line.start_offset, line.end_offset - line.start_offset));
        current_y += font_height;
    }
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
    std::ranges::for_each(lcd.data, [&](auto& byte) { byte = dist(rng); });
}

template<typename Lcd>
void DrawImage(Lcd& lcd, [[maybe_unused]] int imageNo)
{
    std::copy(imageEye.begin(), imageEye.end(), lcd.data.begin());
}

void SleepSeconds(int seconds)
{
    datetime_t initial_time = {
        .year  = 2023,
        .month = 9,
        .day   = 23,
        .dotw  = 6, // Saturday
        .hour  = 12,
        .min   = 0,
        .sec   = 0
    };
    auto wakeup_time = initial_time;
    while(seconds >= 60) {
        ++wakeup_time.min;
        seconds -= 60;
    }
    wakeup_time.sec += seconds;

    rtc_init();
    rtc_set_datetime(&initial_time);
    rtc_set_alarm(&wakeup_time, nullptr);
    __wfi();
}

int main()
{
    std::mt19937 rng;

    Lcd<LcdPinConfig> nokiaLcd;

    // Switch to XOSC; this drops the clock speed to roughly 12MHz (from
    // 133MHz) and drops power consumption from 100mA to rougly 9mA
    sleep_run_from_xosc();
    for(;;)
    {
        nokiaLcd.Reset();

        // Step 1: noise screen
        for(int round = 0; round < 10; ++round) {
            GenerateNoise(nokiaLcd, rng);
            nokiaLcd.Update();
            sleep_ms(100);
        }

        // Step 2: place text on screen
        {
            std::string_view sv = messages[messagesDist(rng)];
            ClearLcd(nokiaLcd);
            if (!sv.empty() && sv[0] == '#') {
                int imageNo{};
                std::from_chars(sv.begin() + 1, sv.end(), imageNo);
                DrawImage(nokiaLcd, imageNo);
            } else {
                const auto words = SplitTextInWords(nokia_get_small_char, sv);
                const auto lines = CombineWordsToLines(nokiaLcd, nokia_get_small_char, words);
                CenterText(nokiaLcd, nokia_get_small_char, sv, lines);
            }
            nokiaLcd.Update();
        }

        // Step 3: haunting backlight
        int value = 0;
        int delta = 4;
        for(int count = 0; count < 50; ++count) {
            value += delta;
            if (value <= 0 || value >= 128)
            {
                delta = -delta;
            }
            else
            {
                nokiaLcd.SetBacklight(value * value);
            }
            sleep_ms(100);
        }

        // Step 4: shutdown
        nokiaLcd.SetBacklight(0);
        nokiaLcd.PowerDown();
        SleepSeconds(sleepSecondsDist(rng));
    }
}
