#include "esphome/core/log.h"
#include "esphome/core/component.h"

#define VISION_MASTER_E213
#define EInkDisplay_VisionMasterE213 LCMEN2R13EFC1

#include <Arduino.h>
#include "GFX.h"

#include "platforms.h"

#include "display_base.h"

namespace esphome
{
    namespace vision_master
    {
        class LCMEN2R13EFC1 : public BaseDisplay
        {
        public:
            LCMEN2R13EFC1(uint8_t dc, uint8_t cs, uint8_t busy, uint8_t sdi, uint8_t clk, uint16_t max_page_height)
                : BaseDisplay(dc, cs, busy, sdi, clk, max_page_height),
                  lut_partial_vcom_dc{0}, lut_partial_bb{0}, lut_partial_bw{0}, lut_partial_wb{0}, lut_partial_ww{0}
            {
                init();
            }

            void init();
            void calculateMemoryArea(int16_t &sx, int16_t &sy, int16_t &ex, int16_t &ey,
                                     int16_t region_left, int16_t region_top, int16_t region_right, int16_t region_bottom);
            void activate();
            void reset();
            void sendImageData();
            void sendBlankImageData();
            void wait();
            void calculatePixelPageOffset(uint16_t x, uint16_t y, uint16_t &byte_offset, uint8_t &bit_offset);
            void configPartial();
            void configFull();
            void clearPageWindow();

        private:
            static const uint16_t panel_width = 128;
            static const uint16_t panel_height = 250;
            static const uint16_t drawing_width = 122;
            static const uint16_t drawing_height = 250;
            static const Color supported_colors = (Color)(BLACK | WHITE);

            const uint8_t lut_partial_vcom_dc[56];
            const uint8_t lut_partial_bb[56];
            const uint8_t lut_partial_bw[56];
            const uint8_t lut_partial_wb[56];
            const uint8_t lut_partial_ww[56];
        };

        class VisionMaster : public LCMEN2R13EFC1, public Component
        {
        public:
            VisionMaster() : LCMEN2R13EFC1(PIN_DISPLAY_DC, PIN_DISPLAY_CS, PIN_DISPLAY_BUSY, DEFAULT_SDI, DEFAULT_CLK, 250) {}
            void dump_config() override;
        };
    } // namespace vision_master
} // namespace esphome