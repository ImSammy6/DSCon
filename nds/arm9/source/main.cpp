#include <stdio.h>
#include <filesystem.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <nds.h>
#include <nf_lib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <dswifi9.h>
#include <math.h>
#include <nds/system.h>

using namespace std;

const bool DEBUG = false;

int held;
int pressed;
int released;
int dpad;
bool match;
int currentButton;
int currentButtonState = 0;
rtcTimeAndDate currentTime;

touchPosition touch;

bool swapSticks = false; // false: left, true: right
bool stickActive = false;
bool triggerMode = false; //false: L R, true: ZL ZR
bool backlightOn = true;
bool swapButtons = false;
int homeButton = 0;
int LR3 = 0;
int sleepTime = 0;
int currentBackground = 0;
bool clockShown = true;
bool dateShown = false;
bool dateShouldBeShown = false;
bool clockBG0Only = true;
bool offMode = false;
int offModeDirs[2] = {0};
bool showButtons = true;
bool trackpadMode = false;
bool RHMode = false;
int oldpx;
int oldpy;
int stickx;
int sticky;
int volumeUp = 0;
int volumeDown = 0;
bool lidHomeButton = false;
bool hotkeyMode = false;
int selectGracePeriod = 0;
int selectTap = 0;
int dateOffset = 0;

const int touchCircleID = 9;

const int NUMBGS = 2;
const int buttonWidth = 38;
const int smallButtonHeight = 44;
const int stickRadius = 83;
const int LR3Radius = 16;
const int LR3MaxTime = 20;
const int buttonTapTime = 3;
const int sleepMaxWaitTime = 120;
const int trackpadModeMaxSpeed = 24;
const bool startSelectHomeButton = true;
const int volumeTime = 21;
const int selectTime = 10;
const int dateSpacing = 6;

int newpx = 127;
int newpy = 95;

int LR3Timer = 0;

static Wifi_AccessPoint AccessPoint;

typedef struct {
    u16 buttons;
    u8 buttons2;
    u8 lx, ly, rx, ry;
} output_data;

output_data output;

const char* str(int s){
    return (to_string(s)).c_str();
}

int getCurrentButton(){
    int buttonRow;
    if (touch.py < smallButtonHeight)
        buttonRow = 0;
    else if (touch.py > 191 - smallButtonHeight)
        buttonRow = 2;
    else
        buttonRow = 1;

    if (touch.px <= buttonWidth)
        return buttonRow;
    else if (touch.px >= 255 - buttonWidth)
        return buttonRow + 3;
    else
        return -1;
}

void fifo_time_handler(int num_bytes, void *userdata){
    fifoGetDatamsg(FIFO_USER_01, sizeof(currentTime), (u8 *)&currentTime);
}

void fifo_power_handler(int num_bytes, void *userdata){
    homeButton = buttonTapTime;
}

void fifo_volume_handler(u32 volumeType, void *userdata){
    //if (KEY_SELECT & held)
    //    return;
    if (volumeType == 0)
        volumeUp = volumeTime;
    else if (volumeType == 1)
        volumeDown = volumeTime;
}

void setBit(int& stuff, int index, u8 value){
    if (value)
    stuff |= (1 << index);
    else
    stuff &= ~(1 << index);
}

void setBit(u16& stuff, u16 index, int value){
    if (value)
    stuff |= (1 << index);
    else
    stuff &= ~(1 << index);
}

void setBit(u8& stuff, u8 index, int value){
    if (value)
    stuff |= (1 << index);
    else
    stuff &= ~(1 << index);
}

void showClock(bool show){
    NF_ShowSprite(0, 0, show);
    NF_ShowSprite(0, 1, show);
    NF_ShowSprite(0, 2, show);
    NF_ShowSprite(0, 3, show);
    NF_ShowSprite(0, 4, show);
}

void showDate(bool show){
    NF_ShowSprite(0, 5, show);
    NF_ShowSprite(0, 6, show);
    NF_ShowSprite(0, 7, show);
}

int main(void)
{

    fifoSetDatamsgHandler(FIFO_USER_01, fifo_time_handler, NULL);
    fifoSetDatamsgHandler(FIFO_USER_02, fifo_power_handler, NULL);
    fifoSetValue32Handler(FIFO_USER_03, fifo_volume_handler, NULL);
    NF_Set2D(0, 5);
    NF_Set2D(1, 5);

    //consoleDemoInit();

    bool nitroOk = nitroFSInit(NULL);
    if (!nitroOk){
        // cout << "NitroFS init failed!";
    }
    NF_SetRootFolder("NITROFS");
    
    NF_Init16bitsBgBuffers();
    NF_InitBitmapBgSys(0, 1);
    NF_InitBitmapBgSys(1, 1);

    NF_InitSpriteBuffers();
    NF_InitSpriteSys(0);
    NF_InitSpriteSys(1);

    NF_Load16bitsBg("bgbottom", 0);
    NF_Load16bitsBg("bg0", 1);
    NF_Copy16bitsBuffer(1, 0, 0); // bottom screen slot 0
    NF_Copy16bitsBuffer(0, 0, 1); // top screen slot 0
    //NF_Unload16bitsBg(0);
    //NF_Unload16bitsBg(1);

    // load palette slots for each button
    NF_LoadSpritePal("button", 0);
    NF_VramSpritePal(1, 0, 0);
    NF_VramSpritePal(1, 0, 1);
    NF_VramSpritePal(1, 0, 2);
    NF_VramSpritePal(1, 0, 3);
    NF_VramSpritePal(1, 0, 4);
    NF_VramSpritePal(1, 0, 5);

    
    NF_LoadSpritePal("pressed", 1);
    NF_LoadSpritePal("cancelled", 2);
    NF_VramSpritePal(1, 1, 6);
    NF_VramSpritePal(1, 2, 7);

    NF_LoadSpriteGfx("backlight", 0, 32, 64);
    NF_VramSpriteGfx(1, 0, 0, false);
    NF_CreateSprite(1, 0, 0, 0, 1, 1);

    NF_LoadSpriteGfx("capture", 1, 32, 64);
    NF_VramSpriteGfx(1, 1, 1, false);
    NF_CreateSprite(1, 1, 1, 1, 1, 49);
    NF_CreateSprite(1, 2, 1, 1, 1, 113);
    NF_SpriteFrame(1, 2, 1);

    NF_LoadSpriteGfx("swapButtons", 2, 32, 64);
    NF_VramSpriteGfx(1, 2, 2, false);
    NF_CreateSprite(1, 3, 2, 2, 1, 145);

    NF_LoadSpriteGfx("toggleLR", 3, 32, 64);
    NF_VramSpriteGfx(1, 3, 3, false);
    NF_CreateSprite(1, 4, 3, 3, 225, 1);

    NF_LoadSpriteGfx("swapLR", 4, 32, 64);
    NF_VramSpriteGfx(1, 4, 4, false);
    NF_CreateSprite(1, 5, 4, 4, 225, 49);
    NF_CreateSprite(1, 6, 4, 4, 225, 113);
    NF_SpriteFrame(1, 5, 1);
    NF_SpriteFrame(1, 6, 2);

    NF_LoadSpriteGfx("swapSticks", 5, 32, 64);
    NF_VramSpriteGfx(1, 5, 5, false);
    NF_CreateSprite(1, 7, 5, 5, 225, 145);

    
    NF_LoadSpritePal("black", 3);
    NF_VramSpritePal(1, 3, 8);

    NF_LoadSpriteGfx("LR", 6, 16, 16);
    NF_VramSpriteGfx(1, 6, 6, false);
    NF_CreateSprite(1, 8, 6, 8, 122, 87);

    NF_LoadSpriteGfx("touchCircle", 7, 64, 64);
    NF_VramSpriteGfx(1, 7, 7, false);
    NF_CreateSprite(1, 9, 7, 8, 0, 0);
    NF_ShowSprite(1, 9, false);

    NF_LoadSpriteGfx("dot", 8, 64, 64);
    NF_VramSpriteGfx(1, 8, 8, false);
    NF_CreateSprite(1, 10, 8, 8, 0, 0);
    NF_CreateSprite(1, 11, 8, 8, 0, 0);
    NF_CreateSprite(1, 12, 8, 8, 0, 0);
    NF_ShowSprite(1, 10, false);
    NF_ShowSprite(1, 11, false);
    NF_ShowSprite(1, 12, false);


    NF_LoadSpritePal("white", 4);
    NF_VramSpritePal(0, 4, 0);

    NF_LoadSpriteGfx("dstime", 9, 8, 8);
    NF_VramSpriteGfx(0, 9, 0, false);
    NF_CreateSprite(0, 0, 0, 0, 232, 3);
    NF_CreateSprite(0, 1, 0, 0, 237, 3);
    NF_CreateSprite(0, 2, 0, 0, 242, 3);
    NF_CreateSprite(0, 3, 0, 0, 244, 3);
    NF_CreateSprite(0, 4, 0, 0, 249, 3);
    NF_SpriteFrame(0, 0, 1);
    NF_SpriteFrame(0, 2, 10);

    NF_CreateSprite(0, 6, 0, 0, 217, 3);
    NF_CreateSprite(0, 7, 0, 0, 222, 3);

    NF_LoadSpriteGfx("days", 10, 16, 8);
    NF_VramSpriteGfx(0, 10, 1, false);
    NF_CreateSprite(0, 5, 1, 0, 198, 3);

    showDate(false);

    const char server_ssid[] = "dscon";
    const int server_ssid_len = 5;
    
    Wifi_InitDefault(INIT_ONLY);
    
    while (1) // main loop
    {
        while (!DEBUG){ // loop between scanning and connecting until connection is successful
            Wifi_ScanMode();
            while (1){ // Scanning for access points
                //consoleClear();
                // cout << "Scanning...\n";
                int count = Wifi_GetNumAP();
                for (int i = 0; i < count; i++){
                    Wifi_GetAPData(i, &AccessPoint);
                    // cout << AccessPoint.ssid << "\n";
                    // cout << +AccessPoint.ssid_len << "\n";
                    if (AccessPoint.ssid_len == server_ssid_len){
                        match = true;
                        for (int j = 0; j < server_ssid_len; j++){
                            if (AccessPoint.ssid[j] != server_ssid[j]){
                                match = false;
                                break;
                            }
                        }
                        if (match){
                            // cout <<"Match found!\n";
                            break;
                        }
                        
                    }
                }
                if (match)
                    break;

            swiWaitForVBlank();
            }

            //Wifi_SetIP(0xC0A8040B, 0xC0A80401, 0xFFFFFF00, 0xC0A80401, 0xC0A80401);
            Wifi_SetIP(0x0B04A8C0, 0x0104A8C0, 0x00FFFFFF, 0x0104A8C0, 0x0104A8C0);
            Wifi_ConnectAP(&AccessPoint, WEPMODE_NONE, 0, 0);
            int status;

            swiWaitForVBlank();

            while (!DEBUG){
                //consoleClear();
                // cout << "Connecting...\n";
                status = Wifi_AssocStatus();
                // cout << ASSOCSTATUS_STRINGS[status] << "\n";
                if (status == ASSOCSTATUS_ASSOCIATED || status == ASSOCSTATUS_CANNOTCONNECT){
                    break;
                }
                swiWaitForVBlank();
            }
            
            // rescan if connection fails
            if (status == ASSOCSTATUS_ASSOCIATED){
                break;
            }
        }

        //swiWaitForVBlank();
        //swiWaitForVBlank();
        //swiWaitForVBlank();
        //swiWaitForVBlank();
        //swiWaitForVBlank();

        struct in_addr ip, gateway, mask, dns1, dns2;
        ip = Wifi_GetIPInfo(&gateway, &mask, &dns1, &dns2);

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        //bool socketSuccess = (sockfd >= 0);
        //connect(sockfd, (struct sockaddr*)&address, sizeof(address)); //cast to a normal sockaddr for some reason

        int PORT = 2000;
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(PORT); // htons ensures the port number is big-endian
        address.sin_addr.s_addr = inet_addr(inet_ntoa(gateway));

        setCpuClock(false);

        while (1){
            //consoleClear();
            // cout << "Connected!\n";
            // cout << "IP:      " << inet_ntoa(ip) << "\n";
            // cout << "Gateway: " << inet_ntoa(gateway) << "\n";
            // cout << "Socket success: " << socketSuccess << "\n";
            // cout << "SSID:    " << AccessPoint.ssid << "\n";
            scanKeys();
            held = keysHeld();
            pressed = keysDown();
            released = keysUp();
            touchRead(&touch);

            if (KEY_TOUCH & pressed){
                if ((touch.px > buttonWidth && touch.px < 255 - buttonWidth) || !showButtons){
                    stickActive = true;
                    NF_ShowSprite(1, touchCircleID, true);
                    if (!trackpadMode){
                        NF_ShowSprite(1, touchCircleID + 1, true);
                        NF_ShowSprite(1, touchCircleID + 2, true);
                        NF_ShowSprite(1, touchCircleID + 3, true);
                    }
                    else {
                        oldpx = touch.px;
                        oldpy = touch.py;
                    }
                }
            }

            if (KEY_TOUCH & held && !stickActive){
                int hoveredButton = getCurrentButton();
                if (currentButtonState == 0){
                    currentButton = hoveredButton;
                }
                if (hoveredButton == currentButton){
                    if (currentButtonState != 1){
                        NF_VramSpritePal(1, 1, currentButton);
                    }
                    currentButtonState = 1;
                } else{
                    if (currentButtonState != 2 && currentButton != 1 && currentButton != 4){ //ignore tall buttons
                        NF_VramSpritePal(1, 2, currentButton);
                    }
                    currentButtonState = 2;
                }
            }

            if (KEY_TOUCH & released){
                if (stickActive){
                stickActive = false;
                NF_ShowSprite(1, touchCircleID, false);
                NF_ShowSprite(1, touchCircleID + 1, false);
                NF_ShowSprite(1, touchCircleID + 2, false);
                NF_ShowSprite(1, touchCircleID + 3, false);
                if (LR3Timer > 0)
                {
                    LR3 = buttonTapTime;
                }
                
                } else{
                    if (currentButtonState == 1){
                        switch (currentButton){
                            case 0:
                                backlightOn = !backlightOn;
                                if (backlightOn)
                                    powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
                                else
                                    powerOff(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
                                break;
                            case 2:
                                swapButtons = !swapButtons;
                                if (swapButtons)
                                    NF_SpriteFrame(1, 3, 1);
                                else
                                    NF_SpriteFrame(1, 3, 0);
                                break;
                            case 3:
                                triggerMode = !triggerMode;
                                if (triggerMode){
                                    NF_SpriteFrame(1, 4, 1);
                                    NF_SpriteFrame(1, 5, 0);
                                } else{
                                    NF_SpriteFrame(1, 4, 0);
                                    NF_SpriteFrame(1, 5, 1);
                                }
                                break;
                            case 5:
                                swapSticks = !swapSticks;
                                if (swapSticks)
                                    NF_SpriteFrame(1, 8, 1);
                                else
                                    NF_SpriteFrame(1, 8, 0);
                                break;
                        }
                    }
                    NF_VramSpritePal(1, 0, currentButton);
                    currentButtonState = 0;
                    currentButton = -1;
                }
            }

            if (stickActive){
                if (!trackpadMode){
                    float distFromCenter = (sqrt(pow(touch.px - 127, 2) + pow(touch.py - 95, 2)));
                    newpx = touch.px;
                    newpy = touch.py;
                    if (distFromCenter > stickRadius){
                        newpx = (int)((touch.px - 127) * stickRadius / distFromCenter) + 127;
                        newpy = (int)((touch.py - 95) * stickRadius / distFromCenter) + 95;
                    }
                    if (distFromCenter <= LR3Radius){
                        if (KEY_TOUCH & pressed)
                            LR3Timer = LR3MaxTime;
                    }
                    else{
                        LR3Timer = 0;
                    }
                    if (LR3Timer > 0)
                        LR3Timer--;
                    NF_MoveSprite(1, touchCircleID, newpx - 31, newpy - 31);
                    NF_MoveSprite(1, touchCircleID + 1, round(newpx * 0.25 + 127 * 0.75 - 2), round(newpy * 0.25 + 95 * 0.75 - 2));
                    NF_MoveSprite(1, touchCircleID + 2, round(newpx * 0.5 + 127 * 0.5 - 2), round(newpy * 0.5 + 95 * 0.5 - 2));
                    NF_MoveSprite(1, touchCircleID + 3, round(newpx * 0.75 + 127 * 0.25 - 2), round(newpy * 0.75 + 95 * 0.25 - 2));
                    stickx = (newpx - 127) * 127 / stickRadius + 128;
                    sticky = (newpy - 95) * 127 / stickRadius + 128;
                } else {
                    int touchdx = touch.px - oldpx;
                    int touchdy = touch.py - oldpy;
                    if (touchdx > trackpadModeMaxSpeed)
                        touchdx = trackpadModeMaxSpeed;
                    if (touchdx < -trackpadModeMaxSpeed)
                        touchdx = -trackpadModeMaxSpeed;
                    if (touchdy > trackpadModeMaxSpeed)
                        touchdy = trackpadModeMaxSpeed;
                    if (touchdy < -trackpadModeMaxSpeed)
                        touchdy = -trackpadModeMaxSpeed;
                    stickx = touchdx * 127 / trackpadModeMaxSpeed + 128;
                    sticky = touchdy * 127 / trackpadModeMaxSpeed + 128;
                    NF_MoveSprite(1, touchCircleID, touch.px - 31, touch.py - 31);
                    oldpx = touch.px;
                    oldpy = touch.py;
                }
            }

            if (KEY_SELECT & pressed)
                selectGracePeriod = selectTime;

            // hotkeys

            if (volumeUp || KEY_SELECT & held){

                if (volumeUp || (KEY_SELECT & held && (pressed & ~(1 << 2) || volumeDown == volumeTime)))
                    hotkeyMode = true;

                if (KEY_X & pressed){
                    lidHomeButton = !lidHomeButton;
                    if (lidHomeButton)
                        disableSleep();
                    else
                        enableSleep();
                }

                if (KEY_Y & pressed){
                    if (!clockShown) {
                        clockShown = true;
                        if (dateShouldBeShown)
                            dateShown = true;
                    } else if (!dateShown) {
                        dateShown = true;
                        dateShouldBeShown = true;
                    } else {
                        clockShown = false;
                        dateShown = false;
                        dateShouldBeShown = false;
                    }
                    showClock(clockShown);
                    showDate(dateShown);
                    if (clockShown) {
                        clockBG0Only = currentBackground == 0;
                    }
                }

                if (KEY_LEFT & pressed){
                    offMode = !offMode;
                }

                if (KEY_UP & pressed && trackpadMode){
                    RHMode = !RHMode;
                }

                if (KEY_DOWN & pressed){
                    backlightOn = !backlightOn;
                    if (backlightOn)
                        powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
                    else
                        powerOff(PM_BACKLIGHT_BOTTOM);
                    break;
                }

                if (KEY_B & pressed){
                    showButtons = !showButtons;
                    for (int i = 0; i <= 7; i++){
                        NF_ShowSprite(1, i, showButtons);
                    }
                }

                if (KEY_A & pressed){
                    trackpadMode = !trackpadMode;
                    if (trackpadMode)
                        NF_Load16bitsBg("bgbottomblank", 0);
                    else{
                        NF_Load16bitsBg("bgbottom", 0);
                        RHMode = false;
                    }
                    NF_Copy16bitsBuffer(1, 0, 0);
                    if (trackpadMode && stickActive){
                        oldpx = touch.px;
                        oldpy = touch.py;
                    }
                }

                if ((KEY_L & pressed || KEY_R & pressed) && NUMBGS > 1){
                    if (KEY_L & pressed){
                        currentBackground--;
                        if (currentBackground == -1)
                            currentBackground = NUMBGS - 1;
                    }
                    if (KEY_R & pressed){
                        currentBackground++;
                        if (currentBackground == NUMBGS)
                            currentBackground = 0;
                    }
                    // reload background
                    char bgname[4] = "bg";
                    char bgNumStr[2] = "";
                    snprintf(bgNumStr, sizeof(bgNumStr), "%d", currentBackground);
                    strcat(bgname, bgNumStr);
                    NF_Load16bitsBg(bgname, 1);
                    NF_Copy16bitsBuffer(0, 0, 1);
                    
                    if (clockBG0Only){
                        clockShown = currentBackground == 0;
                        dateShown = currentBackground == 0 && dateShouldBeShown;
                        showClock(clockShown);
                        showDate(dateShown);
                    }
                }
            }

            if (KEY_SELECT & released && selectGracePeriod){
                selectTap = buttonTapTime;
            }

            //prepare to send button data
            output.buttons = held;
            output.buttons2 = 0;
            output.lx = 128;
            output.ly = 128;
            output.rx = 128;
            output.ry = 128;

            setBit(output.buttons, 2, 0);
            setBit(output.buttons, 12, 0);
            setBit(output.buttons, 13, 0);
            setBit(output.buttons, 14, 0);

            if (offMode || swapSticks){
                setBit(output.buttons, 4, 0);
                setBit(output.buttons, 5, 0);
                setBit(output.buttons, 6, 0);
                setBit(output.buttons, 7, 0);
            }

            if (offMode){
                dpad = 0;
                if ((KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT) & pressed){
                    offModeDirs[1] = offModeDirs[0];
                    if (KEY_UP & pressed)
                        offModeDirs[0] = KEY_UP;
                    else if (KEY_DOWN & pressed)
                        offModeDirs[0] = KEY_DOWN;
                    else if (KEY_LEFT & pressed)
                        offModeDirs[0] = KEY_LEFT;
                    else 
                        offModeDirs[0] = KEY_RIGHT;
                }
                if (offModeDirs[0] & held)
                    dpad = offModeDirs[0];
                else if (offModeDirs[1] & held){
                    offModeDirs[0] = offModeDirs[1];
                    dpad = offModeDirs[0];
                }
                if (!swapSticks)
                    output.buttons |= dpad;
            }\
            else{
                dpad = held;
            }

            if (swapSticks && !hotkeyMode){
                if (KEY_LEFT & dpad)
                    output.lx = 1;
                else if (KEY_RIGHT & dpad)
                    output.lx = 254;
                else
                    output.lx = 128;

                if (KEY_UP & dpad)
                    output.ly = 1;
                else if (KEY_DOWN & dpad)
                    output.ly = 254;
                else
                    output.ly = 128;
            }

            if (stickActive){
                u8& currentStickx = (swapSticks) ? output.rx : output.lx;
                u8& currentSticky = (swapSticks) ? output.ry : output.ly;
                currentStickx = stickx;
                currentSticky = sticky;
            }

            if (swapButtons){
                setBit(output.buttons, 0, KEY_B & held);
                setBit(output.buttons, 1, KEY_A & held);
                setBit(output.buttons, 10, KEY_Y & held);
                setBit(output.buttons, 11, KEY_X & held);
            }

            bool swapTriggers = triggerMode != (currentButton == 4 && currentButtonState != 0);
            if (swapTriggers){
                setBit(output.buttons, 8, 0);
                setBit(output.buttons, 9, 0);
                setBit(output.buttons, 12, KEY_R & held);
                setBit(output.buttons, 13, KEY_L & held);
            }

            if (!swapSticks)
                setBit(output.buttons, 14, LR3);
            else
                setBit(output.buttons, 15, LR3);

            if (KEY_LID & pressed && lidHomeButton)
                homeButton = buttonTapTime;

            if (homeButton){
                setBit(output.buttons2, 0, 1);
            }

            // capture button
            if (currentButton == 1 && currentButtonState != 0)
                setBit(output.buttons2, 1, 1);

            if (RHMode && KEY_TOUCH & held)
                setBit(output.buttons, 0, 1);

            if (volumeDown >= volumeTime - buttonTapTime){
                if (!swapSticks)
                    setBit(output.buttons, 15, 1);
                else
                    setBit(output.buttons, 14, 1);
            }

            if ((KEY_SELECT & held && !selectGracePeriod) || selectTap){
                setBit(output.buttons, 2, 1);
            }

            if (hotkeyMode){
                output.buttons = 0;
                output.buttons2 = 0;
            }

            // ignore hotkey mode since it uses select
            if (KEY_START & held && KEY_SELECT & held && startSelectHomeButton)
                setBit(output.buttons2, 0, 1);

            int result = sendto(sockfd, &output, 7, 0, (struct sockaddr*)&address, sizeof(address));
            if (result < 0){
                // cout << "Send failed!";
            }

            if (homeButton > 0)
                homeButton--;
            
            if (LR3 > 0)
                LR3--;
            
            if (volumeUp > 0)
                volumeUp--;

            if (volumeDown > 0)
                volumeDown--;

            if (selectGracePeriod > 0)
                selectGracePeriod--;

            if (selectTap > 0)
                selectTap--;

            if (hotkeyMode){
                homeButton = 0;
                LR3 = 0;
                volumeDown = 0;
                selectGracePeriod = 0;
                selectTap = 0;
                if (!(KEY_SELECT & held || volumeUp))
                    hotkeyMode = false;
            }

            // update clock
            if (currentTime.hours > 11)
                currentTime.hours = currentTime.hours - 12;
            if (currentTime.hours == 0)
                currentTime.hours = 12;

            if (clockShown){
                NF_ShowSprite(0, 0, currentTime.hours >= 10);
                NF_SpriteFrame(0, 1, currentTime.hours % 10);
                NF_SpriteFrame(0, 3, currentTime.minutes / 10);
                NF_SpriteFrame(0, 4, currentTime.minutes % 10);
            }

            if (dateShown){
                dateOffset = 0;
                if (currentTime.hours < 10)
                    dateOffset += 5;
                if (currentTime.day < 10){
                    dateOffset += 5;
                    NF_ShowSprite(0, 7, false);
                } else
                    NF_ShowSprite(0, 7, true);

                NF_MoveSprite(0, 7, 228 + dateOffset - dateSpacing, 3);
                NF_MoveSprite(0, 6, 223 + dateOffset - dateSpacing, 3);
                NF_MoveSprite(0, 5, 208 + dateOffset - dateSpacing * 2, 3);

                NF_SpriteFrame(0, 5, currentTime.weekday);
                if (currentTime.day >= 10)
                    NF_SpriteFrame(0, 6, currentTime.day / 10);
                else
                    NF_SpriteFrame(0, 6, currentTime.day % 10);
                NF_SpriteFrame(0, 7, currentTime.day % 10);
            }

            NF_SpriteOamSet(0);
            NF_SpriteOamSet(1);
            swiWaitForVBlank();
            oamUpdate(&oamMain);
            oamUpdate(&oamSub);

            if (lidHomeButton && KEY_LID & held){
                sleepTime++;
                if (sleepTime == sleepMaxWaitTime){
                    systemSleep();
                }
            } else
                sleepTime = 0;
        }
        
    }

    return 0;
}
