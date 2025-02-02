#include "esphome/core/log.h"
#include "esphome/core/component.h"

#define VISION_MASTER_E213
#define EInkDisplay_VisionMasterE213 LCMEN2R13EFC1

#include <Arduino.h>
#include "GFX.h"

#include "FreeMono7pt7b.h"
#include "FreeMono9pt7b.h"
#include "FreeMono12pt7b.h"
#include "FreeMono18pt7b.h"
#include "FreeMono24pt7b.h"
#include "FreeMonoBold9pt7b.h"
#include "FreeMonoBold12pt7b.h"
#include "FreeMonoBold18pt7b.h"
#include "FreeMonoBold24pt7b.h"
#include "FreeMonoBoldOblique9pt7b.h"
#include "FreeMonoBoldOblique12pt7b.h"
#include "FreeMonoBoldOblique18pt7b.h"
#include "FreeMonoBoldOblique24pt7b.h"
#include "FreeMonoOblique9pt7b.h"
#include "FreeMonoOblique12pt7b.h"
#include "FreeMonoOblique18pt7b.h"
#include "FreeMonoOblique24pt7b.h"
#include "FreeSans9pt7b.h"
#include "FreeSans12pt7b.h"
#include "FreeSans18pt7b.h"
#include "FreeSans24pt7b.h"
#include "FreeSansBold9pt7b.h"
#include "FreeSansBold12pt7b.h"
#include "FreeSansBold18pt7b.h"
#include "FreeSansBold24pt7b.h"
#include "FreeSansBoldOblique9pt7b.h"
#include "FreeSansBoldOblique12pt7b.h"
#include "FreeSansBoldOblique18pt7b.h"
#include "FreeSansBoldOblique24pt7b.h"
#include "FreeSansOblique9pt7b.h"
#include "FreeSansOblique12pt7b.h"
#include "FreeSansOblique18pt7b.h"
#include "FreeSansOblique24pt7b.h"
#include "FreeSerif9pt7b.h"
#include "FreeSerif12pt7b.h"
#include "FreeSerif18pt7b.h"
#include "FreeSerif24pt7b.h"
#include "FreeSerifBold9pt7b.h"
#include "FreeSerifBold12pt7b.h"
#include "FreeSerifBold18pt7b.h"
#include "FreeSerifBold24pt7b.h"
#include "FreeSerifBoldItalic9pt7b.h"
#include "FreeSerifBoldItalic12pt7b.h"
#include "FreeSerifBoldItalic18pt7b.h"
#include "FreeSerifBoldItalic24pt7b.h"
#include "FreeSerifItalic9pt7b.h"
#include "FreeSerifItalic12pt7b.h"
#include "FreeSerifItalic18pt7b.h"
#include "FreeSerifItalic24pt7b.h"
#include "kongtext5pt7b.h"
#include "Org_01.h"
#include "Picopixel.h"
#include "TomThumb.h"
#include "Tiny3x3a2pt7b.h"

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