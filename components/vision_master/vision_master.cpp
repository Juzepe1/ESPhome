#include "vision_master.h"

namespace esphome
{
    namespace vision_master
    {

        static const char *const TAG = "vision_master";

        void VisionMaster::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Vision Master:");
            ESP_LOGCONFIG(TAG, "Display Configuration:");
            ESP_LOGCONFIG(TAG, "  CS Pin: %d", PIN_DISPLAY_CS);
            ESP_LOGCONFIG(TAG, "  DC Pin: %d", PIN_DISPLAY_DC);
            ESP_LOGCONFIG(TAG, "  Reset Pin: %d", PIN_DISPLAY_RST);
            ESP_LOGCONFIG(TAG, "  Busy Pin: %d", PIN_DISPLAY_BUSY);
        }

        void LCMEN2R13EFC1::init()
        {
            // Pass information to base class, once instantiated
            BaseDisplay::panel_width = this->panel_width;
            BaseDisplay::panel_height = this->panel_height;

            BaseDisplay::drawing_width = this->drawing_width;
            BaseDisplay::drawing_height = this->drawing_height;

            BaseDisplay::supported_colors = this->supported_colors;

            // Get the Bounds subclass ready now (in constructor), so that it can be used to init. globals.
            BaseDisplay::instantiateBounds();

            // Set the initial drawing config early, so user's config commands before the first update aren't ignored
            BaseDisplay::initDrawingParams();
        }

        void LCMEN2R13EFC1::calculateMemoryArea(int16_t &sx, int16_t &sy, int16_t &ex, int16_t &ey,
                                                int16_t region_left, int16_t region_top, int16_t region_right, int16_t region_bottom)
        {

            // These seem very "straight-forward", but this controller doesn't actually support "partial window"
            // We're going to change where in the pagefile we write instead

            sx = region_left;
            sy = region_top;
            ex = region_right;
            ey = region_bottom;
        }

        void LCMEN2R13EFC1::activate()
        {

            // Power on the panel
            sendCommand(0x04);
            wait();

            // Refresh
            sendCommand(0x12);
            wait();

            // Power off
            sendCommand(0x02);
        }

        // Soft-reset the display
        void LCMEN2R13EFC1::reset()
        {

// On "all-in-one" platforms: ensure peripheral power is on, then briefly pull the display's reset pin to ground
// Reason: these boards do actually connect the RST pin
#if ALL_IN_ONE
            Platform::VExtOn();
            Platform::toggleResetPin();
            wait();
#endif

            wait();
        }

        // Prepare display controller to receive image data, then transfer
        void LCMEN2R13EFC1::sendImageData()
        {

            // Always write the full pagefile - no "partial window" support
            // TODO: optimize
            const uint16_t byte_count = panel_height * panel_width / 8;

            // Fastmode Off
            if (fastmode_state == OFF)
            {
                sendCommand(0x10); // Write "BLACK / OLD" memory
                for (uint16_t i = 0; i < byte_count; i++)
                    sendData(page_black[i]);

                sendCommand(0x13); // Write "RED / NEW" memory
                for (uint16_t i = 0; i < byte_count; i++)
                    sendData(page_black[i]);
            }

            // Fastmode - First Pass (new memory)
            else if (!fastmode_secondpass)
            {
                sendCommand(0x13); // Write "RED / NEW" memory
                for (uint16_t i = 0; i < byte_count; i++)
                    sendData(page_black[i]);
            }

            // Fastmode - Second Pass (old memory)
            else
            {
                sendCommand(0x10); // Write "BLACK / OLD" memory
                for (uint16_t i = 0; i < byte_count; i++)
                    sendData(page_black[i]);
            }

            wait();
        }

        void LCMEN2R13EFC1::sendBlankImageData()
        {
            // Calculate memory values needed to display default_color
            uint8_t blank_byte;
            blank_byte = (default_color & WHITE) * 255; // Get the black mem value for default color

            // In this scope, just
            const uint16_t byte_count = panel_height * panel_width / 8;

            sendCommand(0x13); // Write "RED / NEW" memory
            for (uint16_t i = 0; i < byte_count; i++)
                sendData(blank_byte);

            // Also write the OLD memory, so long as we're not clearing in fastmode
            if (fastmode_state == OFF || fastmode_state == NOT_SET)
            {
                sendCommand(0x10); // Write "BLACK / OLD" memory
                for (uint16_t i = 0; i < byte_count; i++)
                    sendData(blank_byte);
            }

            wait();
        }

        // Wait until the display hardware is idle. Inverted on this display
        void LCMEN2R13EFC1::wait()
        {
            while (digitalRead(pin_busy) == LOW) // Pin is LOW when busy - this is different than the SSD display controllers
                yield();
        }

        // Where should the pixel be placed in the pagefile - overriden by derived display classes which do not support "partial window"
        void LCMEN2R13EFC1::calculatePixelPageOffset(uint16_t x, uint16_t y, uint16_t &byte_offset, uint8_t &bit_offset)
        {

            // Instead of the "window" data being written at the very start of the pagefile, we're just putting it where it would normally go if fullscreen

            // Calculate a memory location (byte) for our pixel
            byte_offset = y * (panel_width / 8);
            byte_offset += x / 8;
            bit_offset = x % 8;            // Find the location of the bit in which the value will be stored
            bit_offset = (7 - bit_offset); // For some reason, the screen wants the bit order flipped. MSB vs LSB?
        }

        void LCMEN2R13EFC1::configPartial()
        {

            sendCommand(0x00);                                                         // Panel setting
            sendData(0b11 << 6 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1 | 1 << 0); // [7:6] Display Res, [5] LUT, [4] BW / BWR [3] Scan Vert, [2] Shift Horiz, [1] Booster, [0] Reset?

            sendCommand(0x50);                             // VCOM and data interval setting
            sendData(0b11 << 6 | 0b01 << 4 | 0b0111 << 0); // [7:6] Border, [5:4] Data polarity (default), [3:0] VCOM and Data interval (default)

            // Load the various LUTs
            sendCommand(0x20); // VCOM
            for (uint8_t i = 0; i < sizeof(lut_partial_vcom_dc); i++)
                sendData(lut_partial_vcom_dc[i]);

            sendCommand(0x21); // White -> White
            for (uint8_t i = 0; i < sizeof(lut_partial_ww); i++)
                sendData(lut_partial_ww[i]);

            sendCommand(0x22); // Black -> White
            for (uint8_t i = 0; i < sizeof(lut_partial_bw); i++)
                sendData(lut_partial_bw[i]);

            sendCommand(0x23); // White -> Black
            for (uint8_t i = 0; i < sizeof(lut_partial_wb); i++)
                sendData(lut_partial_wb[i]);

            sendCommand(0x24); // Black -> Black
            for (uint8_t i = 0; i < sizeof(lut_partial_bb); i++)
                sendData(lut_partial_bb[i]);
        }

        void LCMEN2R13EFC1::configFull()
        {

            sendCommand(0x00);                                                // Panel setting
            sendData(0b11 << 6 | 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1 | 1 << 0); // [7:6] Display Res, [5] LUT, [4] BW / BWR [3] Scan Vert, [2] Shift Horiz, [1] Booster, [0] Reset?

            sendCommand(0x50);                             // VCOM and data interval setting
            sendData(0b10 << 6 | 0b11 << 4 | 0b0111 << 0); // [7:6] Border, [5:4] Data polarity (default), [3:0] VCOM and Data interval (default)
        }

        void LCMEN2R13EFC1::clearPageWindow()
        {

            // Instead of the "window" data being written at the start of the pagefile, we're just putting it where it would normally go if fullscreen
            // Means we can't just wipe the whole pagefile, need to actively fill the window region with default color

            uint8_t blank_byte = (default_color & WHITE) * 255; // We're filling in bulk here; bits are either all on or all off

            // Selectively clear bytes from the middle of the (otherwise-intact) display buffer
            for (uint16_t y = winrot_top; y <= winrot_bottom; y++)
            {
                for (uint16_t x = winrot_left; x <= winrot_right; x += 8)
                {
                    page_black[((y * panel_width) + x) / 8] = blank_byte;
                }
            }
        }

    } // namespace vision_master
} // namespace esphome