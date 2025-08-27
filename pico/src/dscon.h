#include <stdint.h>
// #include "ws2812.pio.h"

// Controller HID report structure.
typedef struct {
	uint16_t Button; // 16 buttons;
	uint8_t  HAT;    // HAT switch; one nibble w/ unused nibble
	uint8_t  LX;     // Left  Stick X
	uint8_t  LY;     // Left  Stick Y
	uint8_t  RX;     // Right Stick X
	uint8_t  RY;     // Right Stick Y
	uint8_t  VendorSpec;
} USB_ControllerReport_Input_t;

// The output is structured as a mirror of the input.
typedef struct {
	uint16_t Button; // 16 buttons;
	uint8_t  HAT;    // HAT switch; one nibble w/ unused nibble
	uint8_t  LX;     // Left  Stick X
	uint8_t  LY;     // Left  Stick Y
	uint8_t  RX;     // Right Stick X
	uint8_t  RY;     // Right Stick Y
} USB_ControllerReport_Output_t;

// Type Defines
// Enumeration for controller buttons.
typedef enum {
	KEY_Y       = 0x01,
	KEY_B       = 0x02,
	KEY_A       = 0x04,
	KEY_X       = 0x08,
	KEY_L       = 0x10,
	KEY_R       = 0x20,
	KEY_ZL      = 0x40,
	KEY_ZR      = 0x80,
	KEY_SELECT  = 0x100,
	KEY_START   = 0x200,
	KEY_LCLICK  = 0x400,
	KEY_RCLICK  = 0x800,
	KEY_HOME    = 0x1000,
	KEY_CAPTURE = 0x2000,
} ControllerButtons_t;

// Action state
enum {
	A_PLAY, // play from buffer
	A_RT,   // real-time
	A_LAG,  // lag
	A_STOP  // stop
};

// Serial control information
enum {
    C_IDLE,        // nothing happening
    C_ACTIVATED,   // activated by command character
    C_Q,           // receiving a controller state for queue
    C_I,           // receiving an immediate controller state
	C_F,           // request for queue buffer fill amount
	C_M,           // mode change
	C_R,           // request to read from record buffer 
	C_D            // receiving a new delay value
};


#define CMD_CHAR '+'

#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE
#define VSYNC_IN_PIN 14

#define ALARM_IRQ TIMER_IRQ_0


void core1_task(void);
void hid_task(void);
static void alarm_in_us(uint32_t delay_us);
