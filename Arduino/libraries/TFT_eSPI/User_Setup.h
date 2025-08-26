#define USER_SETUP_ID 100

// LCD DRIVER
#define ST7789_DRIVER  // 1.47-inch LCD uses ST7789 controller

// SCREEN DIMENSIONS
#define TFT_WIDTH  172
#define TFT_HEIGHT 320

// SPI CONFIGURATION (Based on Waveshare ESP32-S3-LCD-1.47)
#define TFT_MISO   -1   // Not used
#define TFT_MOSI    6   // Data pin (SPI MOSI)
#define TFT_SCLK    7   // Clock pin (SPI SCK)
#define TFT_CS      5   // Chip Select
#define TFT_DC      4   // Data/Command control pin
#define TFT_RST    -1   // No reset pin, set to -1
#define TFT_BL     38   // Backlight control pin

// BACKLIGHT CONFIGURATION
#define TFT_BACKLIGHT_ON HIGH

// SPI SETTINGS
#define SPI_FREQUENCY  27000000   // 27MHz max speed
#define SPI_READ_FREQUENCY  20000000  // 20MHz for reading

// NO TOUCH SCREEN (Disable touch functions)
#define TOUCH_CS -1
