/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 KNfLrPn
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

#include <stdio.h>
#include <stdlib.h>
#include "pico/multicore.h"
#include <string.h>
#include "bsp/board.h"
#include "tusb.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "usb_descriptors.h"
#include "dscon.h"

#include "hardware/timer.h"
#include "hardware/irq.h"

#define UDP_PORT 2000
#define BUFFER_SIZE 8
char ds_state[BUFFER_SIZE] = {0};
int hometime = 0;

#define BIT(nr) (1UL << (nr))


//--------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------

// Controller reports and ring buffer.
USB_ControllerReport_Input_t neutral_con, current_con;
unsigned int queue_tail, queue_head, rec_head, stream_head;

// VSYNC timing
unsigned int frame_delay_us = 10000;
bool vsync_en = false;

// State variables
bool usb_connected = false;
bool led_on = true;
uint8_t vsync_count = 0;
uint8_t sent_count = 0;
uint8_t lag_amount = 0;
bool recording = false;
bool recording_wrap = false;
static struct udp_pcb *udp_pcb_inst;

uint16_t convert_ds_state_bit(uint16_t dsindex, uint16_t bita, uint16_t bitb){
    if (ds_state[dsindex] & BIT(bita))
        return BIT(bitb);
    else
        return 0;
}

static void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                 const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        memset(ds_state, 0, sizeof(ds_state));
        size_t len = p->len < (BUFFER_SIZE - 1) ? p->len : (BUFFER_SIZE - 1);
        memcpy(ds_state, p->payload, len);
        printf("Received UDP (%.*s) from %s:%d\n", (int)len, ds_state,
               ipaddr_ntoa(addr), port);
        pbuf_free(p);

        /*
        if (ds_state[1] & BIT(5)){
            hometime++;
        }
        else{
            hometime = 0;
        }
        */

        //update controller state
        current_con.Button = 0;
        current_con.Button |= convert_ds_state_bit(1, 3, 0);  // Y button
        current_con.Button |= convert_ds_state_bit(0, 1, 1);  // B button
        current_con.Button |= convert_ds_state_bit(0, 0, 2);  // A button
        current_con.Button |= convert_ds_state_bit(1, 2, 3);  // X button
        current_con.Button |= convert_ds_state_bit(1, 1, 4);  // L button
        current_con.Button |= convert_ds_state_bit(1, 0, 5);  // R button
        current_con.Button |= convert_ds_state_bit(1, 5, 6); // ZL button
        current_con.Button |= convert_ds_state_bit(1, 4, 7); // ZR button
        current_con.Button |= convert_ds_state_bit(0, 2, 8);  // Select button
        current_con.Button |= convert_ds_state_bit(0, 3, 9);  // Start button
        current_con.Button |= convert_ds_state_bit(1, 6, 10);  // L3 button
        current_con.Button |= convert_ds_state_bit(1, 7, 11);  // R3 button
        current_con.Button |= convert_ds_state_bit(2, 0, 12); // Home button
        current_con.Button |= convert_ds_state_bit(2, 1, 13); // Capture button


        
        bool bright = (BIT(4) & ds_state[0]) != 0;
        bool bleft = (BIT(5) & ds_state[0]) != 0;
        bool bup = (BIT(6) & ds_state[0]) != 0;
        bool bdown = (BIT(7) & ds_state[0]) != 0;
        bool hatDiag = false;
        current_con.HAT = 8;

        if (bup && bright) {
            current_con.HAT = 1;
            hatDiag = true;
        }
        if (bdown && bright) {
            current_con.HAT = 3;
            hatDiag = true;
        }
        if (bdown && bleft) {
            current_con.HAT = 5;
            hatDiag = true;
        }
        if (bup && bleft) {
            current_con.HAT = 7;
            hatDiag = true;
        }

        if (!hatDiag) {
            if (bup) {
                current_con.HAT = 0;
            }
            if (bright) {
                current_con.HAT = 2;
            }
            if (bdown) {
                current_con.HAT = 4;
            }
            if (bleft) {
                current_con.HAT = 6;
            }
        }

        bool hasStick = true;

        if (hasStick)
        {
            current_con.LX = ds_state[3];
            current_con.LY = ds_state[4];
            current_con.RX = ds_state[5];
            current_con.RY = ds_state[6];
        }
        else
        {
            current_con.LX = 0x80;
            current_con.LY = 0x80;
            current_con.RX = 0x80;
            current_con.RY = 0x80;
        }
    }
}

//--------------------------------------------------------------------
// Main
//--------------------------------------------------------------------
int main(void)
{
    board_init();

    // Set up USB
    tusb_init();

    /*
    // Set up debug neopixel
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, 16, 800000, IS_RGBW);
    debug_pixel(urgb_u32(1, 1, 0));
    // Set up feedback neopixel
    ws2812_program_init(pio, 2, offset, 8, 800000, IS_RGBW);
    */

    // Start second core (handles the display)
    multicore_launch_core1(core1_task);

    // Start the free-running timer
    alarm_in_us(16667);


    // start access point
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    const char *ap_name = "dscon";

    cyw43_arch_enable_ap_mode(ap_name, NULL, CYW43_AUTH_OPEN);
    sleep_ms(500);
    udp_pcb_inst = udp_new();
    if (!udp_pcb_inst) {
        printf("UDP PCB allocation failed\n");
        cyw43_arch_deinit();
        return 1;
    }
    udp_bind(udp_pcb_inst, IP_ADDR_ANY, UDP_PORT);
    udp_recv(udp_pcb_inst, udp_receive_callback, NULL);
    // Forever loop
    while (1)
    {
        tud_task(); // tinyusb device task
        hid_task();
    }

    return 0;
}

void core1_task()
{
    /*
    while (1)
    {
        if (led_on)
        {
            // Heartbeat
            uint8_t hb = ((vsync_count % 64) == 0) * 4 | ((vsync_count % 64) == 11) * 32;
            debug_pixel(urgb_u32(hb, 0, usb_connected * 16));
        }
        else
            debug_pixel(urgb_u32(0, 0, 0));

        sleep_ms(5);
    }
    */
}

//--------------------------------------------------------------------
// Device callbacks
//--------------------------------------------------------------------

// Invoked when device is mounted
void tud_mount_cb(void)
{
    usb_connected = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    usb_connected = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    usb_connected = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    usb_connected = true;
}

// Invoked when sent REPORT successfully to host
// Nothing to do here, since there's only one report.
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)instance;
    (void)len;
}

// Invoked when received GET_REPORT control request
// Nothing to do here; can just STALL (return 0)
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

//--------------------------------------------------------------------
// USB HID
//--------------------------------------------------------------------

void hid_task(void)
{
    // report controller data
    if (tud_hid_ready())
        tud_hid_report(0, &current_con, sizeof(USB_ControllerReport_Input_t));
}

//--------------------------------------------------------------------
// Timer code
//--------------------------------------------------------------------

/* Alarm interrupt handler.
   This happens once per game frame, and is used to update the USB data.
*/
static void alarm_irq(void)
{
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    if (!vsync_en)
    {
        alarm_in_us(16667); // set an alarm 1/60s in the future
        vsync_count++;
    }
}

/* Set up an alarm in the future.
 */
static void alarm_in_us(uint32_t delay_us)
{
    // Enable the interrupt for the alarm
    hw_set_bits(&timer_hw->inte, 1u << 0);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
    // Enable the alarm irq
    irq_set_enabled(ALARM_IRQ, true);
    // Enable interrupt in block and at processor
    uint64_t target = timer_hw->timerawl + delay_us;

    // Write the lower 32 bits of the target time to the alarm which
    // will arm it
    timer_hw->alarm[0] = (uint32_t)target;
}

//--------------------------------------------------------------------
// Unused (but don't remove)
//--------------------------------------------------------------------

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}