#include "display_base.h"

BaseDisplay::BaseDisplay(uint8_t pin_dc,
                         uint8_t pin_cs,
                         uint8_t pin_busy,
                         uint8_t pin_sdi,
                         uint8_t pin_clk,
                         int16_t page_height)
    : GFX(0, 0)
{
    // Store config from the derived class
    this->pin_dc = pin_dc;
    this->pin_cs = pin_cs;
    this->pin_busy = pin_busy;
    this->pin_sdi = pin_sdi;
    this->pin_clk = pin_clk;
    this->pagefile_height = page_height;
}

void BaseDisplay::begin()
{

    // Only begin() once
    if (begun)
        return;
    else
        begun = true;

    // Set the digital pins that supplement the SPI interface
    pinMode(pin_cs, OUTPUT);
    pinMode(pin_dc, OUTPUT);
    pinMode(pin_busy, INPUT); // NOTE: do not use internal pullups: incompatible logic levels

    // Prepare SPI
    display_spi = Platform::getSPI();
    digitalWrite(pin_cs, HIGH);
    Platform::beginSPI(display_spi, pin_sdi, pin_miso, pin_clk);

// SAMD21: change SPI pins if requested
#ifdef __SAMD21G18A__
    if (pin_sdi != DEFAULT_SDI || pin_clk != DEFAULT_CLK)
        pin_miso = Platform::setSPIPins(pin_sdi, pin_clk);
#endif

// All in one boards: power on the peripherals, and hard reset to clear away whatever traumatic experience the display has been through
#if ALL_IN_ONE
    Platform::VExtOn();
    Platform::toggleResetPin();
    wait();
#endif

    // If PRESERVE_IMAGE possible, memory was allocated in constructor
    if (PRESERVE_IMAGE && pagefile_height == panel_height)
        writePage(); // Make sure display memory is also blanked
}

// Get the Bounds() subclass ready early, so it can be used to initialize globals
void BaseDisplay::instantiateBounds()
{

    // We lied to GFX about dimensions in the constructor, but now we do have the information.
    // I'm not sure it matters, but..

    _width = WIDTH = drawing_width;
    _height = HEIGHT = drawing_height;

    // Instantiate the "Bounds" subclass system, and pass references
    this->bounds = Bounds(drawing_width,
                          drawing_height,
                          &winrot_top,
                          &winrot_right,
                          &winrot_bottom,
                          &winrot_left,
                          &rotation,
                          &imgflip);
}

void BaseDisplay::initDrawingParams()
{
    // Default drawing config must be set early, as user-config may be issued before begin() is auto-called.

    // Calculate pagefile size
    pagefile_height = constrain(pagefile_height, (uint16_t)1, (uint16_t)MAX_PAGE_HEIGHT);
    page_bytecount = panel_width * pagefile_height / 8;

    // If unpaged drawing possible, allocate the memory, and set library to draw fullscreen
    if (PRESERVE_IMAGE && pagefile_height == panel_height)
        grabPageMemory();

    // Default GFX options
    fullscreen();
    setBackgroundColor(WHITE);
    setTextColor(BLACK);
}

// "Custom power swiitching" is disabled on Wireless Paper platforms - irrelevant and maybe the user will break something?
#ifndef ALL_IN_ONE

// Set configuration of custom power-swiching circuit, then power up
void BaseDisplay::useCustomPowerSwitch(uint8_t pin, SwitchType type)
{
    this->pin_power = pin;
    this->switch_type = type;

    // Start powered up
    delay(50);
    pinMode(pin_power, OUTPUT);
    digitalWrite(pin_power, switch_type);

    // Wait for powerup
    delay(100);
}

#endif

// Text positioning
// ======================================================================

// Set the text cursor according to the desired upper left corner
void BaseDisplay::setCursorTopLeft(const char *text, uint16_t x, uint16_t y)
{
    int16_t offset_x(0), offset_y(0);
    uint16_t width(0), height(0);
    getTextBounds(text, 0, 0, &offset_x, &offset_y, &width, &height);
    setCursor(x - offset_x, y - offset_y);
}

// Set the text cursor according to the desired upper left corner, from an Arduino String
void BaseDisplay::setCursorTopLeft(const String &text, uint16_t x, uint16_t y)
{
    setCursorTopLeft(text.c_str(), x, y);
}

// Rendered width of a string
uint16_t BaseDisplay::getTextWidth(const char *text)
{
    int16_t x(0), y(0);
    uint16_t w(0), h(0);
    getTextBounds(text, 0, 0, &x, &y, &w, &h); // Still need x and y; used internally by getTextBounds()
    return w;
}

// Rendered width of an Arduino string
uint16_t BaseDisplay::getTextWidth(const String &text)
{
    return getTextWidth(text.c_str());
}

// Rendered height of a string
uint16_t BaseDisplay::getTextHeight(const char *text)
{
    int16_t x(0), y(0);
    uint16_t w(0), h(0);
    getTextBounds(text, 0, 0, &x, &y, &w, &h); // Still need x and y; used internally by getTextBounds()
    return h;
}

// Rendered height of a string
uint16_t BaseDisplay::getTextHeight(const String &text)
{
    return getTextHeight(text.c_str());
}

// Get the required cursor X position to horizontally center text
uint16_t BaseDisplay::getTextCenterX(const char *text)
{
    int16_t offset_x(0), offset_y(0);
    uint16_t width(0), height(0);

    // Get the text dimensions
    getTextBounds(text, 0, 0, &offset_x, &offset_y, &width, &height);

    uint16_t left = bounds.window.centerX() - offset_x;
    int16_t center = left - (width / 2);
    return (uint16_t)max((int16_t)0, center);
}

// Get the required cursor X position to horizontally center an Arduino String
uint16_t BaseDisplay::getTextCenterX(const String &text)
{
    return getTextCenterX(text.c_str());
}

// Get the required cursor Y position to vertically center text
uint16_t BaseDisplay::getTextCenterY(const char *text)
{
    int16_t offset_x(0), offset_y(0);
    uint16_t width(0), height(0);

    // Get the text dimensions
    getTextBounds(text, 0, 0, &offset_x, &offset_y, &width, &height);

    uint16_t top = bounds.window.centerY() - offset_y;
    int16_t center = top - (height / 2);
    return (uint16_t)max((int16_t)0, center);
}

// Get the required cursor Y position to vertically center an Arduino String
uint16_t BaseDisplay::getTextCenterY(const String &text)
{
    return getTextCenterY(text.c_str());
}

// Print text in the center of the display, with optional offset
void BaseDisplay::printCenter(const char *text, int16_t offset_x, int16_t offset_y)
{
    // Find dimensions of the text
    int16_t text_off_x(0), text_off_y(0); // How far is text top left from the centerpoint
    uint16_t width(0), height(0);
    getTextBounds(text, 0, 0, &text_off_x, &text_off_y, &width, &height);

    // Move the cursor into position
    uint16_t left = bounds.window.centerX() - text_off_x;
    uint16_t top = bounds.window.centerY() - text_off_y;
    setCursor(
        left - (width / 2) + offset_x,
        top - (height / 2) + offset_y);

    // Print the text
    print(text);
}

// Print an Arduino String in the center of the display, with optional offset
void BaseDisplay::printCenter(const String &text, int16_t offset_x, int16_t offset_y)
{
    printCenter(text.c_str(), offset_x, offset_y);
}

// Print an integer in the center of the display, with optional offset
void BaseDisplay::printCenter(int32_t number, int16_t offset_x, int16_t offset_y)
{
    char text[11];
    itoa(number, text, 10);
    printCenter(text, offset_x, offset_y);
}

// Print an integer in the center of the display, with optional offset
void BaseDisplay::printCenter(uint32_t number, int16_t offset_x, int16_t offset_y)
{
    char text[11];
    utoa(number, text, 10);
    printCenter(text, offset_x, offset_y);
}

// Print a float in the center of the display, optionally specifying decimal places and offset
void BaseDisplay::printCenter(float value, uint8_t decimal_places, int16_t offset_x, int16_t offset_y)
{
    printCenter((double)value, decimal_places, offset_x, offset_y);
}

// Print a double in the center of the display, optionally specifying decimal places and offset
void BaseDisplay::printCenter(double value, uint8_t decimal_places, int16_t offset_x, int16_t offset_y)
{
    uint8_t length = 0;
    uint8_t digits_before_decimal = log(abs(value)) + 1;

    // Space for minus sign
    if (value < 0)
        length++;

    // Digits before decimal point
    length += digits_before_decimal;

    // Space for decimal point
    if (decimal_places > 0)
        length += decimal_places;

    // Null terminator
    length++;

    // Get the string (dynamic length), then pass through
    char *text = new char[length];

    // sprintf(text, )
    dtostrf(value, 0, decimal_places, text); // Length without the null-term

    printCenter(text, offset_x, offset_y);
    delete[] text;
}

// Virtual AdafruitGFX methods. Tweaked to implement text-wrapping
// ================================================================

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

// Pointers are a peculiar case...typically 16-bit on AVR boards,
// 32 bits elsewhere.  Try to accommodate both...

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c)
{
#ifdef __AVR__
    return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
#else
    // expression in __AVR__ section may generate "dereferencing type-punned
    // pointer will break strict-aliasing rules" warning In fact, on other
    // platforms (such as STM32) there is no need to do this pointer magic as
    // program memory may be read in a usual way So expression may be simplified
    return gfxFont->glyph + c;
#endif // __AVR__
}

inline uint8_t *pgm_read_bitmap_ptr(const GFXfont *gfxFont)
{
#ifdef __AVR__
    return (uint8_t *)pgm_read_pointer(&gfxFont->bitmap);
#else
    // expression in __AVR__ section generates "dereferencing type-punned pointer
    // will break strict-aliasing rules" warning In fact, on other platforms (such
    // as STM32) there is no need to do this pointer magic as program memory may
    // be read in a usual way So expression may be simplified
    return gfxFont->bitmap;
#endif // __AVR__
}

void BaseDisplay::setCursor(int16_t x, int16_t y)
{
    // Let Adafruit do their thing
    GFX::setCursor(x, y);

    // Save the value; we'll use it for text wrapping
    this->cursor_placed_x = x;
}

size_t BaseDisplay::write(uint8_t c)
{

    if (!gfxFont)
    { // 'Classic' built-in font

        if (c == '\n')
        { // Newline?
            // Newline: either below the first line, or at window edge, if window moved
            cursor_x = max((int16_t)bounds.window.left(), cursor_placed_x);
            cursor_y += textsize_y * 8; // advance y one line
        }
        else if (c != '\r')
        { // Ignore carriage returns
            if (wrap && ((cursor_x + textsize_x * 6) > (int16_t)bounds.window.right()))
            {                                                                   // Off right?
                cursor_x = max((int16_t)bounds.window.left(), cursor_placed_x); // Reset x
                cursor_y += textsize_y * 8;                                     // advance y one line
            }
            drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
            cursor_x += textsize_x * 6; // Advance x one char
        }
    }
    else
    { // Custom font

        if (c == '\n')
        {
            // Newline: either below the first line, or at window edge, if window moved
            cursor_x = max((int16_t)bounds.window.left(), cursor_placed_x);
            cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        }
        else if (c != '\r')
        {
            uint8_t first = pgm_read_byte(&gfxFont->first);
            if ((c >= first) && (c <= (uint8_t)pgm_read_byte(&gfxFont->last)))
            {
                GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);
                uint8_t w = pgm_read_byte(&glyph->width);
                uint8_t h = pgm_read_byte(&glyph->height);

                if ((w > 0) && (h > 0))
                {                                                        // Is there an associated bitmap?
                    int16_t xo = (int8_t)pgm_read_byte(&glyph->xOffset); // sic

                    if (wrap && ((cursor_x + textsize_x * (xo + w)) > (int16_t)bounds.window.right()))
                    { // Cursor beyond right edge
                        cursor_x = max((int16_t)bounds.window.left(), cursor_placed_x);
                        cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
                    }
                    drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
                }
                cursor_x += (uint8_t)pgm_read_byte(&glyph->xAdvance) * (int16_t)textsize_x;
            }
        }
    }
    return 1;
}

void BaseDisplay::charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)
{
    if (gfxFont)
    {
        if (c == '\n')
        {                                                             // Newline
            *x = max((int16_t)bounds.window.left(), cursor_placed_x); // Reset x to line start, advance y by one line
            *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        }

        else if (c != '\r')
        { // Not a carriage return; is normal char
            uint8_t first = pgm_read_byte(&gfxFont->first),
                    last = pgm_read_byte(&gfxFont->last);

            if ((c >= first) && (c <= last))
            { // Char present in this font?
                GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);

                uint8_t gw = pgm_read_byte(&glyph->width),
                        gh = pgm_read_byte(&glyph->height),
                        xa = pgm_read_byte(&glyph->xAdvance);
                int8_t xo = pgm_read_byte(&glyph->xOffset),
                       yo = pgm_read_byte(&glyph->yOffset);

                if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > (int16_t)bounds.window.right()))
                {                                                                  // Cursor beyond right edge
                    *x = max((int16_t)bounds.window.left(), cursor_placed_x) + xa; // Reset x to line start, advance y by one line
                    *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
                }

                int16_t tsx = (int16_t)textsize_x,
                        tsy = (int16_t)textsize_y,
                        x1 = *x + xo * tsx,
                        y1 = *y + yo * tsy,
                        x2 = x1 + gw * tsx - 1,
                        y2 = y1 + gh * tsy - 1;

                if (x1 < *minx)
                    *minx = x1;
                if (y1 < *miny)
                    *miny = y1;
                if (x2 > *maxx)
                    *maxx = x2;
                if (y2 > *maxy)
                    *maxy = y2;

                *x += xa * tsx;
            }
        }
    }

    else
    { // Default font

        if (c == '\n')
        {                                                             // Newline
            *x = max((int16_t)bounds.window.left(), cursor_placed_x); // Reset x to line start, advance y by one line
            *y += textsize_y * 8;                                     // advance y one line
            // min/max x/y unchaged -- that waits for next 'normal' character
        }

        else if (c != '\r')
        { // Normal char; ignore carriage returns

            if (wrap && ((*x + textsize_x * 6) > (int16_t)bounds.window.right()))
            {                                                                   // Cursor beyond right edge
                cursor_x = max((int16_t)bounds.window.left(), cursor_placed_x); // Reset x
                *y += textsize_y * 8;                                           // advance y one line
            }

            int x2 = *x + textsize_x * 6 - 1, // Lower-right pixel of char
                y2 = *y + textsize_y * 8 - 1;
            if (x2 > *maxx)
                *maxx = x2; // Track max x, y
            if (y2 > *maxy)
                *maxy = y2;
            if (*x < *minx)
                *minx = *x; // Track min x, y
            if (*y < *miny)
                *miny = *y;
            *x += textsize_x * 6; // Advance x one char
        }
    }
}

// Fix getTextBounds() for 32bit platforms
// ========================================

// If platform does not use 16 bit ints
#if __INT_MAX__ != __INT16_MAX__

// Original (16 bit integer) definitions

void BaseDisplay::getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    GFX::getTextBounds(str, x, y, x1, y1, w, h);
}

void BaseDisplay::getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    GFX::getTextBounds(str, x, y, x1, y1, w, h);
}

void BaseDisplay::getTextBounds(const __FlashStringHelper *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    GFX::getTextBounds(str, x, y, x1, y1, w, h);
}

// Modified to accept "int" on 32 bit platforms
// Some users may not be comfortable with fixed-width integers

void BaseDisplay::getTextBounds(const char *str, int x, int y, int *x1, int *y1, unsigned int *w, unsigned int *h)
{
    int16_t x16, y16;
    GFX::getTextBounds(str, (int16_t)x, (int16_t)y, (int16_t *)&x16, (int16_t *)&y16, (uint16_t *)w, (uint16_t *)h);
    *x1 = x16;
    *y1 = y16;
}

void BaseDisplay::getTextBounds(const String &str, int x, int y, int *x1, int *y1, unsigned int *w, unsigned int *h)
{
    int16_t x16, y16;
    GFX::getTextBounds(str, (int16_t)x, (int16_t)y, (int16_t *)&x16, (int16_t *)&y16, (uint16_t *)w, (uint16_t *)h);
    *x1 = x16;
    *y1 = y16;
}

void BaseDisplay::getTextBounds(const __FlashStringHelper *str, int x, int y, int *x1, int *y1, unsigned int *w, unsigned int *h)
{
    int16_t x16, y16;
    GFX::getTextBounds(str, (int16_t)x, (int16_t)y, (int16_t *)&x16, (int16_t *)&y16, (uint16_t *)w, (uint16_t *)h);
    *x1 = x16;
    *y1 = y16;
}

#endif

// Used with a WHILE loop, to break the graphics into small parts, and draw them one at a time
bool BaseDisplay::calculating()
{

    // NOTE: this loop runs BEFORE every new page.
    // This means that it is actually processing data generated in the last loop
    // No real reason for this over a do while, just personal preference

    // Beginning of first loop (initial setup)
    // --------------------------------------
    if (page_cursor == 0)
    {

        // Init display, if needed
        if (fastmode_state == NOT_SET)
        {
            fastmodeOff();
            clearAllMemories(); // Fill whole memory, incase updating window after reset (static)
        }

        // Grab memory, if it doesn't persist between updates
        if (!PRESERVE_IMAGE || pagefile_height < panel_height)
            grabPageMemory();

        // Specify display region handled, either in paging, or outside loop
        page_top = winrot_top;
        page_bottom = min((uint16_t)((winrot_top + pagefile_height) - 1), winrot_bottom);
        pagefile_length = (page_bottom - page_top + 1) * ((winrot_right - winrot_left + 1) / 8);

        // This is usually just clearPage(), unless "partial window" is not supported
        clearPageWindow();

        // Track state of display memory (re:customPowerOn)
        display_cleared = false;
        just_restarted = false;

        // Preserve the initial state - the config set before we begin loop
        storeDrawingConfig();
    }

    // End of each loop (start of next)
    // --------------------------------
    else
    {
        // Return to the config which was present at the start of the first loop - incase user setCursor(), etc
        restoreDrawingConfig();

        // Check if the last page contained any part of the window
        if (!(winrot_bottom < page_top || winrot_top > page_bottom))
            writePage(); // Send off the old page

        // Calculate memory locations for the new page
        page_top += pagefile_height;
        page_bottom = min((uint16_t)((page_top + pagefile_height) - 1), winrot_bottom);
        pagefile_length = (page_bottom - page_top + 1) * ((winrot_right - winrot_left + 1) / 8);
    }

    // Check whether loop should continue
    // ----------------------------------
    if (page_top > winrot_bottom)
    {
        page_cursor = 0; // Reset for next time

        // Release the memory, if not preserved
        if (!PRESERVE_IMAGE || pagefile_height < panel_height)
            freePageMemory();
        else
        {
            // Reset page dimensions now, incase big MCU wants to draw outside loop
            page_top = winrot_top;
            page_bottom = min((uint16_t)((winrot_top + pagefile_height) - 1), winrot_bottom);
            pagefile_length = (page_bottom - page_top + 1) * ((winrot_right - winrot_left + 1) / 8);
        }

        // Fastmode OFF or TURBO, (single pass)
        // ----------------------------------
        if (fastmode_state == OFF || fastmode_state == TURBO)
        {
            return false;
        }

        // Fastmode ON
        // ------------
        else
        {
            // If PRESERVE_IMAGE: no need to re-render - we have the whole image in RAM
            // --------------

            if (PRESERVE_IMAGE && pagefile_height == panel_height)
            {
                return false;
            }

            // If Paging
            // (we need to render all the pieces twice..)
            // --------------

            // First pass
            if (fastmode_secondpass == false)
            {

                fastmode_secondpass = true;
                return true; // Re-calculate the whole display again
            }

            // Second Pass
            else
            {
                fastmode_secondpass = false; // Reset state for next time
                return false;
            }
        }
    }
    else
    {
        // This is usually just clearPage(), unless "partial window" is not supported
        clearPageWindow();
        page_cursor++;
        return true; // Keep looping
    }
}

// Clear the data arrays in between pages
void BaseDisplay::clearPage()
{
    uint8_t black_byte = (default_color & WHITE) * 255; // We're filling in bulk here; bits are either all on or all off
    for (uint16_t i = 0; i < page_bytecount; i++)
        page_black[i] = black_byte;

    // Repeat for red
    if (supportsColor(RED))
    {
        uint8_t red_byte = ((default_color & RED) >> 1) * 255;
        for (uint16_t i = 0; i < page_bytecount; i++)
            page_red[i] = red_byte;
    }
}

// By default, just a wrapper for clearPage()
// If controller has no "partial window" support, behaviour needs to be handled seperately, as part of a workaround
void BaseDisplay::clearPageWindow()
{
    clearPage();
}

// Record the drawing config just before paging loop begins
void BaseDisplay::storeDrawingConfig()
{
    before_paging_font = gfxFont;
    before_paging_text_color = (Color)textcolor;
    before_paging_rotation = (Rotation)rotation;
    before_paging_cursor_x = getCursorX();
    before_paging_cursor_y = getCursorY();
}

// Restore the drawing config at the start of each paging loop - allows setCursor() before DRAW()
void BaseDisplay::restoreDrawingConfig()
{
    // Compare these first - they take extra work to set
    if (gfxFont != before_paging_font)
        setFont(before_paging_font);
    if (rotation != before_paging_rotation)
        setRotation(before_paging_rotation);

    setTextColor(before_paging_text_color);
    setCursor(before_paging_cursor_x, before_paging_cursor_y);
}

// Slow, detailed updates
void BaseDisplay::fastmodeOff()
{

    // Init hardware, if not yet done
    begin();

    fastmode_state = Fastmode::OFF;
    reset();
    configFull();
    wait();
}

// Fast, low quality updates.
// Use sparingly.
void BaseDisplay::fastmodeOn(bool clear_if_reset)
{

    // Init hardware, if not yet done
    begin();

    // If display hasn't had first update yet, blank the display's memory, for the differential update (assume display is blank..)
    if (fastmode_state == NOT_SET && clear_if_reset)
    {
        int16_t sx, sy, ex, ey;
        calculateMemoryArea(sx, sy, ex, ey, winrot_left, page_top, winrot_right, page_bottom); // Virtual, derived class
        setMemoryArea(sx, sy, ex, ey);
        sendBlankImageData(); // Transfer (blank) image via SPI
    }

// If memory was lost after customPowerOff
#if !PRESERVE_IMAGE
    if (just_restarted && !display_cleared && clear_if_reset)
        clear();
#endif

    fastmode_state = Fastmode::ON;
    reset();
    configPartial();
    wait();
}

// Fastest. Unstable.
// Only really relevant for Arduino Uno
// Please draw same image twice before changing mode, window
void BaseDisplay::fastmodeTurbo(bool clear_if_reset)
{

    // Init hardware, if not yet done
    begin();

    // If display hasn't had first update yet, blank the display's memory, for the differential update (assume display is blank..)
    if (fastmode_state == NOT_SET && clear_if_reset)
    {
        int16_t sx, sy, ex, ey;
        calculateMemoryArea(sx, sy, ex, ey, winrot_left, page_top, winrot_right, page_bottom); // Virtual, derived class
        setMemoryArea(sx, sy, ex, ey);
        sendBlankImageData(); // Transfer (blank) image via SPI
    }

    fastmode_state = Fastmode::TURBO;
    reset();
    configPartial();
    configPingPong();
    wait();
}

// Set the rotation for the display
void BaseDisplay::setRotation(int16_t r)
{

    // Interpret "intuitive" rotation vals
    switch (r)
    {
    // 90deg clockwise
    case 90:
    case -270:
    case -3:
        r = 1;
        break;

    // 180 deg
    case 180:
    case -180:
    case -2:
        r = 2;
        break;

    // 270 deg clockwise
    case -90:
    case 270:
    case -1:
        r = 3;
        break;
    }

    GFX::setRotation((uint8_t)r); // Base class method

    // Re-calculate window locations, for give accurate bounds info
    setWindow(bounds.window.left(),
              bounds.window.top(),
              bounds.window.width(),
              bounds.window.height(),
              false); // ( Do not clear the page)
}

// Set the image flip
void BaseDisplay::setFlip(Flip flip)
{
    // Reverse the last flip operation applied
    setWindow(window_left, window_top, window_right - window_left + 1, window_bottom - window_top + 1, false);

    // Store the flip property, for later internal use by GFX methods
    this->imgflip = (Flip)(flip & (Flip::HORIZONTAL | Flip::VERTICAL));

    // If flipping the whole screen, not within a window, recalculate bounds
    if (flip == Flip::HORIZONTAL || flip == Flip::VERTICAL)
        setWindow(window_left, window_top, window_right - window_left + 1, window_bottom - window_top + 1, false);
}

// Draw using the whole screen area
// Call before calculating()
void BaseDisplay::fullscreen()
{
    // BUG: Error when calculating fullscreen with rotation = 1
    // Rather than fix it right now, we'll just unset the rotation while we setWindow()
    uint8_t rotation_old = this->rotation;
    setRotation(0);

    uint16_t left = 0;
    uint16_t top = 0;
    uint16_t width = rotation % 2 ? drawing_height : drawing_width;
    uint16_t height = rotation % 2 ? drawing_width : drawing_height;
    setWindow(left, top, width, height);

    setRotation(rotation_old);
}

// Draw on only part of the screen, leaving the rest unchanged
// Call before DRAW
void BaseDisplay::setWindow(uint16_t left, uint16_t top, uint16_t width, uint16_t height)
{
    setWindow(left, top, width, height, true); // true - clear the page (re: PRESERVE_IMAGE)
}

// Window update code, with exposed clear_page argument. Prevents image clear during setRotation, with PRESERVE_IMAGE
void BaseDisplay::setWindow(uint16_t left, uint16_t top, uint16_t width, uint16_t height, bool clear_page)
{
    uint16_t right = left + (width - 1);
    uint16_t bottom = top + (height - 1);
    window_left = left;
    window_top = top;
    window_right = right;
    window_bottom = bottom;

    // Apply flip
    if (imgflip & Flip::HORIZONTAL)
    {
        if (rotation % 2)
        { // If landscape
            window_right = (drawing_height - 1) - left;
            window_left = (drawing_height - 1) - right;
        }
        else
        { // If portrait
            window_right = (drawing_width - 1) - left;
            window_left = (drawing_width - 1) - right;
        }
    }
    if (imgflip & Flip::VERTICAL)
    {
        if (rotation % 2)
        { // If landscape
            window_bottom = (drawing_width - 1) - top;
            window_top = (drawing_width - 1) - bottom;
        }
        else
        { // If portrait
            window_bottom = (drawing_height - 1) - top;
            window_top = (drawing_height - 1) - bottom;
        }
    }

    // Calculate rotated window locations
    switch (rotation)
    {
    case 0:
        winrot_left = window_left;
        winrot_left = winrot_left - (winrot_left % 8); // Expand box on left to fit contents

        winrot_right = winrot_left + 7;
        while (winrot_right < window_right)
            winrot_right += 8; // Iterate until box includes the byte where our far-left bit lives

        winrot_top = window_top;
        winrot_bottom = window_bottom;
        break;

    case 1:
        winrot_left = (drawing_width - 1) - window_bottom;
        winrot_left = winrot_left - (winrot_left % 8); // Expand box on left to fit contents

        winrot_right = winrot_left + 7;
        while (winrot_right < (drawing_width - 1) - window_top)
            winrot_right += 8; // Iterate until box includes the byte where our far-left bit lives

        winrot_top = window_left;
        winrot_bottom = window_right;
        break;

    case 2:
        winrot_left = (drawing_width - 1) - window_right;
        winrot_left = winrot_left - (winrot_left % 8); // Expand box on left to fit contents

        winrot_right = winrot_left + 7;
        while (winrot_right < (drawing_width - 1) - window_left)
            winrot_right += 8; // Iterate until box includes the byte where our far-left bit lives

        winrot_bottom = (drawing_height - 1) - window_top;
        winrot_top = (drawing_height - 1) - window_bottom;

        break;

    case 3:
        winrot_left = window_top;
        winrot_left = winrot_left - (winrot_left % 8); // Expand box on left to fit contents

        winrot_right = winrot_left + 7;
        while (winrot_right < window_bottom)
            winrot_right += 8; // Iterate until box includes the byte where our far-left bit lives

        winrot_top = (drawing_height - 1) - window_right;
        winrot_bottom = (drawing_height - 1) - window_left;
        break;
    } // -- Finish calculating window rotation

    // Limit window to panel
    if (window_left < 0)
        window_left = 0;
    if (window_top < 0)
        window_top = 0;
    if (rotation % 2)
    { // Landscape
        if (window_right >= drawing_height - 1)
            window_right = drawing_height - 1;
        if (window_bottom >= drawing_width - 1)
            window_bottom = drawing_width - 1;
    }
    else
    { // Portrait
        if (window_right >= drawing_width - 1)
            window_right = drawing_width - 1;
        if (window_bottom >= drawing_height - 1)
            window_bottom = drawing_height - 1;
    }

// If preserving image, and window moves, need to reset relevant area for drawPixel, and clear
#if PRESERVE_IMAGE
    if (pagefile_height == panel_height)
    { // If user didn't re-enable paging
        // Specify display region handled, either in paging, or outside loop
        page_top = winrot_top;
        page_bottom = min((uint16_t)((winrot_top + pagefile_height) - 1), winrot_bottom);
        pagefile_length = (page_bottom - page_top + 1) * ((winrot_right - winrot_left + 1) / 8);

        if (clear_page)
            clearPageWindow(); // This is *usually* just clearPage(), unless "partial window" is not supported.
    }
#endif
}

void BaseDisplay::landscape()
{
// Orient with LoRa antenna facing up
#if defined(Vision_Master_E290)
    setRotation(1);
#else
    setRotation(3);
#endif
}

// Allocate memory to the pagefile(s)
void BaseDisplay::grabPageMemory()
{
    page_black = new uint8_t[page_bytecount];

    if (supportsColor(RED)) // Only if 3-color disply
        page_red = new uint8_t[page_bytecount];
}

// Free pagefile memory
void BaseDisplay::freePageMemory()
{
    delete[] page_black;

    if (supportsColor(RED)) // Only if 3-color display
        delete[] page_red;
}

void BaseDisplay::sendCommand(uint8_t command)
{
    display_spi->beginTransaction(spi_settings);
    digitalWrite(pin_dc, LOW); // D/C pin LOW means SPI transfer is a command
    digitalWrite(pin_cs, LOW);

    display_spi->transfer(command);

    digitalWrite(pin_cs, HIGH);
    display_spi->endTransaction();
}

void BaseDisplay::sendData(uint8_t data)
{
    display_spi->beginTransaction(spi_settings);
    digitalWrite(pin_dc, HIGH); // D/C pin HIGH means SPI transfer is data
    digitalWrite(pin_cs, LOW);

    display_spi->transfer(data);

    digitalWrite(pin_cs, HIGH);
    display_spi->endTransaction();
}

// Reset the display
void BaseDisplay::reset()
{
// On all-in-one platforms: ensure peripheral power is on, then briefly pull the display's reset pin to ground
// Reason: these boards *do* actually connect the RST pin
#if ALL_IN_ONE
    Platform::VExtOn();
    Platform::toggleResetPin();
    wait();
#endif

    // All platforms: send software reset SPI command
    sendCommand(0x12);
    wait();
}

// Wait until the display hardware is idle. Important as any commands made while "busy" will be discarded.
void BaseDisplay::wait()
{
    while (digitalRead(pin_busy) == HIGH)
    { // Pin is HIGH when busy
        yield();
    }
}

// Write one page to the panel memory
void BaseDisplay::writePage()
{
    // Calculate rotated x start and stop values (y is already done via paging)
    int16_t sx, sy, ex, ey;
    calculateMemoryArea(sx, sy, ex, ey, winrot_left, page_top, winrot_right, page_bottom); // Virtual, derived class
    setMemoryArea(sx, sy, ex, ey);
    sendImageData(); // Transfer image via SPI
}

// Inform the display of selected memory area
void BaseDisplay::setMemoryArea(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    // Split into bytes
    uint8_t sy1, sy2, ey1, ey2;
    sy1 = sy & 0xFF;
    sy2 = (sy >> 8) & 0xFF;
    ey1 = ey & 0xFF;
    ey2 = (ey >> 8) & 0xFF;

    // Data entry mode - Left to Right, Top to Bottom
    sendCommand(0x11);
    sendData(0x03);

    // Inform the panel hardware of our chosen memory location
    sendCommand(0x44); // Memory X start - end
    sendData(sx);
    sendData(ex);
    sendCommand(0x45); // Memory Y start - end
    sendData(sy1);
    sendData(sy2);
    sendData(ey1);
    sendData(ey2);
    sendCommand(0x4E); // Memory cursor X
    sendData(sx);
    sendCommand(0x4F); // Memory cursor y
    sendData(sy1);
    sendData(sy2);
}

// Prepare display controller to receive image data, then transfer
void BaseDisplay::sendImageData()
{

    // IF Fastmode OFF
    // -------------
    if (fastmode_state == OFF)
    {

        // Send black
        sendCommand(0x24); // Write "BLACK" memory
        for (uint16_t i = 0; i < pagefile_length; i++)
            sendData(page_black[i]);

        // If supports red, send red
        if (supportsColor(RED))
        {                      // If 3-Color red display
            sendCommand(0x26); // Write memory for red(1)/white (0)
            for (uint16_t i = 0; i < pagefile_length; i++)
                sendData(page_red[i]);
        }

        // If mono, send black data to red memory, for future partial refresh (differential update)
        else
        {
            sendCommand(0x26);
            for (uint16_t i = 0; i < pagefile_length; i++)
                sendData(page_black[i]);
        }
    }

    // IF Fastmode ON - first pass
    // OR Fastmode TURBO
    // -------------------------
    else if (!fastmode_secondpass)
    {
        // Send black
        sendCommand(0x24); // Write "BLACK" memory
        for (uint16_t i = 0; i < pagefile_length; i++)
            sendData(page_black[i]);
    }

    // IF Fastmode OFF - second pass
    // ----------------------------
    else
    {
        // Send black data to red memory, for differential update
        sendCommand(0x26);
        for (uint16_t i = 0; i < pagefile_length; i++)
            sendData(page_black[i]);
    }
}

// Used by clear()
void BaseDisplay::sendBlankImageData()
{
    // Calculate memory values needed to display default_color
    uint8_t black_byte, red_byte;
    black_byte = (default_color & WHITE) * 255; // Get the black mem value for default color
    if (supportsColor(RED))
        red_byte = ((default_color & RED) >> 1) * 255; // Get the red mem value for default color
    else
        red_byte = black_byte;

    // Determine how many bytes to write
    uint16_t pagefile_size = (panel_width / 8) * panel_height;

    // Write the data
    sendCommand(0x24); // Write "BLACK" memory
    for (uint16_t i = 0; i < pagefile_size; i++)
        sendData(black_byte);

    // Also write the RED memory, so long as we're not clearing in fastmode (breaks differential update)
    if (fastmode_state == OFF || fastmode_state == NOT_SET)
    {
        sendCommand(0x26); // Write "RED" memory
        for (uint16_t i = 0; i < pagefile_size; i++)
            sendData(red_byte);
    }
}

// End image-data transmission without updating - for differential update
void BaseDisplay::endImageTxQuiet()
{

    // Default behaviour: Solomon Systech style. Overriden in some derived classes
    sendCommand(0x7F); // Terminate frame write without update
    wait();
    return;
}

// Clear the screen in one optimized step - public
void BaseDisplay::clear()
{
    clearAllMemories();

    // Trigger the display update
    // --------------------------

    // We might need to change to Fastmode::OFF for the update - remember where we started
    Fastmode original_state = fastmode_state;

    // If sketch just started, might need to load settings
    if (fastmode_state == NOT_SET)
        fastmodeOff();

    // Trigger the display changes
    activate();

    // If we *didn't* want to be in Fastmode::OFF, return to original state
    if (original_state == ON)
        fastmodeOn();
    else if (original_state == TURBO)
        fastmodeTurbo();

    // Track state of display memory (re:customPowerOn)
    display_cleared = true;
    just_restarted = false;
}

// Clears the display memory, and if PRESERVE_IMAGE, the pagefile too
void BaseDisplay::clearAllMemories()
{

    // Perform hardware init, if not already done
    begin();

    // Fill the image memory with blank data, if drawing unpaged
    if (PRESERVE_IMAGE && pagefile_height == panel_height)
        clearPage();

    // Fill the display's memory with blank data
    int16_t sx, sy, ex, ey;
    calculateMemoryArea(sx, sy, ex, ey, 0, 0, panel_width - 1, panel_height - 1); // Virtual, derived class
    setMemoryArea(sx, sy, ex, ey);
    sendBlankImageData(); // Transfer (blank) image via SPI
    endImageTxQuiet();
}

#if PRESERVE_IMAGE
// Manually update display, drawing on-top of existing contents
void BaseDisplay::update()
{

    // Init display, if needed
    if (fastmode_state == NOT_SET)
        fastmodeOff();

    // Copy the local image data to the display memory, then update
    writePage();
    activate();

    // If fastmode setting requires, repeat
    if (fastmode_state == ON)
    {
        fastmode_secondpass = true;
        writePage();
        endImageTxQuiet();
        fastmode_secondpass = false;
    }

    // Track state of display memory (re:customPowerOn)
    display_cleared = false;
    just_restarted = false;
}
#endif

// "Custom power switching" is disabled on Wireless Paper platforms - irrelevant and maybe the user will break something?
#ifndef ALL_IN_ONE

// Send power-off signal to your custom power switching circuit, and set the display pins to prevent unwanted current flow
void BaseDisplay::customPowerOff(uint16_t pause)
{
    // Just in-case customPowerOff() gets called in some weird place
    begin();

    // If poweroff is called immediately after drawing, it can compromise the image
    delay(pause);

    // Stop SPI
    display_spi->end();

    // Send the power-down signal
    digitalWrite(pin_power, !switch_type);

    // Set the logic pins: prevent a small current return path
    pinMode(pin_dc, OUTPUT);
    pinMode(pin_cs, OUTPUT);
    pinMode(pin_sdi, OUTPUT);
    pinMode(pin_clk, OUTPUT);
    digitalWrite(pin_dc, switch_type);
    digitalWrite(pin_cs, switch_type);
    digitalWrite(pin_sdi, switch_type);
    digitalWrite(pin_clk, switch_type);

    // Prevent some strange leakage through the busy pin (supposedly high-impedance, but..)
    if (switch_type == PNP)
    {
        pinMode(pin_busy, OUTPUT);
        digitalWrite(pin_busy, LOW);
    }

    // Same, if we're using sd card
    if (pin_cs_card != 0xFF)
    {
        digitalWrite(pin_cs_card, switch_type);
        digitalWrite(pin_miso, switch_type);
    }
}

// Send power-on signal to your custom power switching circuit, then re-init display
void BaseDisplay::customPowerOn()
{
    // We set this to OUTPUT in customPowerOff() to prevent current leakage
    pinMode(pin_busy, INPUT);

    // CS pin deselcted; power-up the display
    digitalWrite(pin_cs, HIGH);
    digitalWrite(pin_power, switch_type);

    // Wait for powerup
    delay(50);

    // SPI resumes
    Platform::beginSPI(display_spi, pin_sdi, pin_miso, pin_clk);

// SAMD21: Move the SPI pins back to custom location
#ifdef __SAMD21G18A__
    if (pin_sdi != MOSI || pin_clk != SCK || pin_miso != MISO)
        Platform::setSPIPins(pin_sdi, pin_clk, pin_miso);
#endif

    // Re-load settings for full-refresh
    fastmodeOff();

    // Mark display as potentially out of sync with memory
    just_restarted = true;

    // Re-initialize display memory
    if (PRESERVE_IMAGE && pagefile_height == panel_height) // If not paged - write old image back to display memory
        writePage();
    else // If paged - No record of old image, just fill blank
        clearAllMemories();
}

#endif

// Draw a single pixel.
// Virtual method from AdafruitGFX. All other drawing methods pass through here

void BaseDisplay::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    // Rotate the pixel
    int16_t x1 = 0, y1 = 0;
    switch (rotation)
    {
    case 0: // No rotation
        x1 = x;
        y1 = y;
        break;
    case 1: // 90deg clockwise
        x1 = (drawing_width - 1) - y;
        y1 = x;
        break;
    case 2: // 180deg
        x1 = (drawing_width - 1) - x;
        y1 = (drawing_height - 1) - y;
        break;
    case 3: // 270deg clockwise
        x1 = y;
        y1 = (drawing_height - 1) - x;
        break;
    }
    x = x1;
    y = y1;

    // Handle flip
    if (imgflip & Flip::HORIZONTAL)
    {
        if (rotation % 2) // If landscape
            y = (drawing_height - 1) - y;
        else // If portrait
            x = (drawing_width - 1) - x;
    }
    if (imgflip & Flip::VERTICAL)
    {
        if (rotation % 2) // If landscape
            x = (drawing_width - 1) - x;
        else // If portrait
            y = (drawing_height - 1) - y;
    }

    // Check if pixel falls in our page
    if ((uint16_t)x >= winrot_left && (uint16_t)y >= page_top && (uint16_t)y <= page_bottom && (uint16_t)x <= winrot_right)
    {

        uint16_t byte_offset;
        uint8_t bit_offset;
        calculatePixelPageOffset(x, y, byte_offset, bit_offset); // Position of pixel within the page files. Overriden if no "partial window" support

        // Insert the correct color values into the appropriate location
        uint8_t bitmask = ~(1 << bit_offset);
        page_black[byte_offset] &= bitmask;
        page_black[byte_offset] |= (color & WHITE) << bit_offset;

        // Red, if display supports
        if (supportsColor(RED))
        {
            page_red[byte_offset] &= bitmask;
            page_red[byte_offset] |= (color >> 1) << bit_offset;
        }
    }
}

// Where should the pixel be placed in the pagefile - overriden by derived display classes which do not support "partial window"
void BaseDisplay::calculatePixelPageOffset(uint16_t x, uint16_t y, uint16_t &byte_offset, uint8_t &bit_offset)
{
    // Calculate a memory location (byte) for our pixel
    byte_offset = (y - page_top) * ((winrot_right - winrot_left + 1) / 8);
    byte_offset += ((x - winrot_left) / 8);
    bit_offset = x % 8;            // Find the location of the bit in which the value will be stored
    bit_offset = (7 - bit_offset); // For some reason, the screen wants the bit order flipped. MSB vs LSB?
}

// Set the color of the blank canvas, before any drawing is done
// Only takes effect at the start of a calculation. At any other time, use fillScreen()
void BaseDisplay::setBackgroundColor(uint16_t bgcolor)
{
    default_color = bgcolor;

    // If user might want update() rather than DRAW(), treat this as a "fill" command
    if (PRESERVE_IMAGE && pagefile_height == panel_height)
        clearPage();
}

// Check if chosen display supports a given color
bool BaseDisplay::supportsColor(Color c)
{
    return ((supported_colors & c) == c);
}

#if PRESERVE_IMAGE
// Clear the drawing memory, without updating display
void BaseDisplay::clearMemory()
{
    begin();
    clearPageWindow(); // Clear our local mem (either fullscreen or window)
    writePage();       // Copy the local image data to the display memory
    endImageTxQuiet(); // Terminate the frame without an update
}
#endif

// Constructor
WindowBounds::WindowBounds(uint16_t drawing_width,
                           uint16_t drawing_height,
                           uint16_t *top,
                           uint16_t *right,
                           uint16_t *bottom,
                           uint16_t *left,
                           uint8_t *rotation,
                           Flip *imgflip)
{
    // Store paramaters
    this->drawing_width = drawing_width;
    this->drawing_height = drawing_height;
    edges[T] = top;
    edges[R] = right;
    edges[B] = bottom;
    edges[L] = left;
    this->rotation = rotation,
    this->imgflip = imgflip;
}

uint16_t WindowBounds::top()
{
    return getWindowBounds(T);
}

uint16_t WindowBounds::right()
{
    return getWindowBounds(R);
}

uint16_t WindowBounds::bottom()
{
    return getWindowBounds(B);
}

uint16_t WindowBounds::left()
{
    return getWindowBounds(L);
}

uint16_t WindowBounds::getWindowBounds(WindowBounds::side request)
{

    // Boolean LUT (x:side, y:rotation): after considering rotation, does requested edge need to measure from opposite edge.
    static const uint8_t rotswap_lut[4] = {0b0000,
                                           0b0101,
                                           0b1111,
                                           0b1010};

    // Handle setFlip() - first part
    // If flipping, we need to find the opposite edge, and right at the end, measure from the opposite side
    if ((request % 2) && (*imgflip & HORIZONTAL))
    { // If we're flipping horizontal, and this edge needs flipping
        request = (WindowBounds::side)((request + 2) % 4);
    }
    if (!(request % 2) && (*imgflip & VERTICAL))
    { // If vertical flip
        request = (WindowBounds::side)((request + 2) % 4);
    } // -------------- End Flip Part 1 -----------

    uint16_t rotated_request = (request + *rotation) % 4; // Rotate the WindowBounds::side value
    uint16_t result;                                      // Build in this varaible

    // Given our request side and rotation, do we need to measure from opposite edge
    bool rotswap = rotswap_lut[*rotation] & (1 << request);

    // Start by simply picking a rotated edge
    result = *edges[rotated_request];

    // Handle a special case; funny issues with unusual drawing_width
    if (rotated_request == R)
        result = min(result, (uint16_t)(drawing_width - 1));

    // If required by LUT, find display width or height, and subtract the edge distance
    if (rotswap)
    {
        uint16_t minuend = (rotated_request % 2) ? (drawing_width - 1) : (drawing_height - 1);
        result = minuend - result;
    }

    // Handle setFlip() - 2nd part
    if ((request % 2) && (*imgflip & HORIZONTAL))
    { // If we're flipping horizontal, and this edge needs flipping
        uint16_t minuend = (rotated_request % 2) ? (drawing_width - 1) : (drawing_height - 1);

        // Handle another special case caused by strange drawing_width
        if (result > minuend)
            result = 0;
        else
            result = minuend - result; // (this is the normal case)
    }
    if (!(request % 2) && (*imgflip & VERTICAL))
    { // If vertical flip
        uint16_t minuend = (rotated_request % 2) ? (drawing_width - 1) : (drawing_height - 1);
        result = minuend - result;
    } // ----------- end flip part 2 -------------

    return result;
}