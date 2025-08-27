# DSCon
Use a Nintendo DS as a wireless Switch controller, using a Raspberry Pi Pico W as a receiver.

<img width="256" height="384" alt="dsconexample" src="https://github.com/user-attachments/assets/51b644f5-8d19-43f9-b260-f9d88fa3ae71" />

You can tap the center of the screen where it says which joystick is selected for L3/R3. You can also press volume-down on a DSi for the opposite L3/R3 button.
The power button also works as the home button on a DSi.

In addition to the on-screen buttons, you can press select or volume-up and one of the following buttons for extra options:  
A: Trackpad mode  
B: Hide buttons  
X: Send home button when lid closed  
Y: Show/hide clock  
L/R: Next/previous background on top screen  
UP: Send A when stylus is down (only in trackpad mode)  
DOWN: Toggle bottom screen backlight  
LEFT: Toggle 4-way D-Pad  
Start: Home button  

More custom backgrounds can be added in the nitrofiles folder, though it requires changing NUMBGs and recompiling.

# Instructions

Flash dscon.uf2 onto your Pi Pico W and plug it into your switch. Then run dscon.nds on your DS and it'll automatically connect.

# Credits

DS code uses BlocksDS. Most of the Pico code is from https://github.com/knflrpn/SwiCC_RP2040 .
