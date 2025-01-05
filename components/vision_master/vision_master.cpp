#include "vision_master.h"

namespace esphome
{
    namespace vision_master
    {

        static const char *const TAG = "vision_master";

        // ! TO DO: Add the class definition here

        void VisionMaster::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Vision Master:");
            ESP_LOGCONFIG(TAG, "Display Configuration:");
            ESP_LOGCONFIG(TAG, "  CS Pin: %d", PIN_DISPLAY_CS);
            ESP_LOGCONFIG(TAG, "  DC Pin: %d", PIN_DISPLAY_DC);
            ESP_LOGCONFIG(TAG, "  Reset Pin: %d", PIN_DISPLAY_RST);
            ESP_LOGCONFIG(TAG, "  Busy Pin: %d", PIN_DISPLAY_BUSY);
        }

    } // namespace vision_master
} // namespace esphome