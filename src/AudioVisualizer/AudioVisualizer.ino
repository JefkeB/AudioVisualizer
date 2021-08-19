/**
 *     _    _____ ___   ___          _             _ _    __     ___                 _ _
 *    / \  |___  / _ \ / _ \        / \  _   _  __| (_) __\ \   / (_)___ _   _  __ _| (_)_______ _ __
 *   / _ \    / / | | | | | |_____ / _ \| | | |/ _` | |/ _ \ \ / /| / __| | | |/ _` | | |_  / _ \ '__|
 *  / ___ \  / /| |_| | |_| |_____/ ___ \ |_| | (_| | | (_) \ V / | \__ \ |_| | (_| | | |/ /  __/ |
 * /_/   \_\/_/  \___/ \___/     /_/   \_\__,_|\__,_|_|\___/ \_/  |_|___/\__,_|\__,_|_|_/___\___|_|
 *
 * ReVox A700 AudioVisualizer
 * Copyright (C) 2021 by DIYLAB <https://www.diylab.de> GitHub: <https://github.com/DIYLAB-DE/AudioVisualizer>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * This Software is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This Software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details: <http://www.gnu.org/licenses/>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * !!! IMPORTANT !!!
 * The GNU General Public License (GNU GPL) obligates the user to also place the software under the
 * conditions of the GPL (copyleft) when redistributing the software in its original or modified form
 * (so-called derivative works). If the licensee does not comply with the conditions, the right to free use expires retroactively!
 * Therefore, the user is also required to make the source code available and to subject the derived software to the GPL.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * !!! WICHTIG !!!
 * Die GNU General Public License (GNU GPL) verpflichtet den Nutzer dazu, bei Weiterverbreitung der Software
 * in ihrer urspr�nglichen oder ver�nderten Form (sog. abgeleitete Werke), diese ebenfalls unter die Bedingungen der GPL zu stellen (Copyleft).
 * H�lt sich der Lizenznehmer nicht an die Bedingungen, erlischt die Befugnis zur freien Benutzung r�ckwirkend!
 * Daher ist der Verwender gehalten, ebenfalls den Quellcode zug�nglich zu machen und die abgeleitete Software wiederum der GPL zu unterwerfen.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Special thanks to Bodmer <https://github.com/Bodmer> for the AA drawing routines and the rainbow colors!
 *
 * Used libraries
 * ---------------
 * Optimized ILI9341 screen driver library for Teensy 4/4.1, with vsync and differential updates: <https://github.com/vindar/ILI9341_T4>
 * TGX - a tiny/teensy graphics library: <https://github.com/vindar/tgx>
 * Arduino OneButton Library: <https://github.com/mathertel/OneButton> (install via the Arduino library manager)
 * IRremote Arduino Library: <https://github.com/Arduino-IRremote/Arduino-IRremote>
 * TeensyID: https://github.com/sstaub/TeensyID
 *
 * Used development software
 * -------------------------
 * Arduino IDE 1.8.15
 * Teensyduino, Version 1.54
 * [optional: Microsoft Visual Studio Community 2019 + Visual Micro - Release 21.06.06.17]
 *
 * This project is only compatible with: PJRC Teensy 4.0 & Teensy 4.1
 **/

 //////////////////////////////////////////////////////////////////////////////////////////////////
 // USER CONFIG SECTION (please only edit here!)                                                 //
 //////////////////////////////////////////////////////////////////////////////////////////////////

#define VERSION "v2.0.0, 14.08.2021"
#define SHOWLOGO                true  // show logo
#define BUTTON                     0  // PIN0
#define IR                         4  // PIN4

// set the pins: here for SPI0 on Teensy 4.x
// ***  Recall that DC must be on a valid cs pin !!! ***
#define PIN_SCK                   13  // (needed) SCK pin for SPI0 on Teensy 4.0
#define PIN_MISO                  12  // (needed) MISO pin for SPI0 on Teensy 4.0
#define PIN_MOSI                  11  // (needed) MOSI pin for SPI0 on Teensy 4.0
#define PIN_DC                    10  // (needed) CS pin for SPI0 on Teensy 4.0
#define PIN_RESET                  6  // (needed) any pin can be used 
#define PIN_CS                     9  // (needed) any pin can be used
#define PIN_BACKLIGHT              5  // only required if LED pin from screen is connected to Teensy 
#define PIN_TOUCH_IRQ            255  // 255 if touch not connected
#define PIN_TOUCH_CS             255  // 255 if touch not connected
#define SPI_SPEED           40000000  // SPI speed

//////////////////////////////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include <Audio.h>
#include "SPI.h"
#include <ILI9341Driver.h>
#include <tgx.h> 
#include <EEPROM.h>
#include <OneButton.h>
#include <IRremote.h>
#include <TeensyID.h>
#include "globals.h"
#include "helpers.h"
#include "analog_black.h"
#include "analog_white.h"
#include "analog_warm.h"
#include "logo.h"
#include "ili9341_t3n_font_Arial.h"
#include "ili9341_t3n_font_ArialBold.h"

//////////////////////////////////////////////////////////////////////////////////////////////////


/// <summary>
/// inherit from the 'AudioControlSGTL5000' class to modify the CHIP_ANA_ADC_CTRL register
/// </summary>
#include "control_sgtl5000.h"
class mSGTL5000 : public AudioControlSGTL5000 {
public:
    void attGAIN(uint8_t att) { modify(0x0020, (att & 1) << 8, 1 << 8); }
};

// namespace for draw primitives
using namespace tgx;

// framebuffers
DMAMEM uint16_t internalBuffer[240 * 320]; // used for internal buffering
DMAMEM uint16_t frontBuffer[240 * 320];    // paint in this buffer
DMAMEM uint16_t backBuffer[240 * 320];     // background buffer

// samplebuffers
/*DMAMEM*/ int16_t samplesLeft[2048];
/*DMAMEM*/ int16_t samplesRight[2048];

// the screen driver object
ILI9341_T4::ILI9341Driver tft(PIN_CS, PIN_DC, PIN_SCK, PIN_MOSI, PIN_MISO, PIN_RESET, PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// two diff buffers
ILI9341_T4::DiffBuffStatic<6000> diff1;
ILI9341_T4::DiffBuffStatic<6000> diff2;

// images that encapsulates framebuffers
Image<RGB565> im(frontBuffer, 240, 320);
Image<RGB565> bg(backBuffer, 240, 320);

// instantiate IR object
IRrecv irrecv(IR);
decode_results results;

// instantiate button object
OneButton btn(BUTTON, true, true);

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=205,270
AudioOutputI2S           i2s2;           //xy=233.00000381469727,400.0000057220459
AudioAnalyzeFFT1024      fft1024_1;      //xy=425.00000762939453,132.00000190734863
AudioAnalyzeFFT1024      fft1024_2;      //xy=426.00000381469727,320.0000047683716
AudioAnalyzeRMS          rms1;           //xy=431.00000381469727,93.00000190734863
AudioRecordQueue         queue2;         //xy=431.00000381469727,280.00000381469727
AudioAnalyzePeak         peak1;          //xy=432.00000762939453,54.000003814697266
AudioRecordQueue         queue1;         //xy=432.00000381469727,171.00000381469727
AudioMixer4              mixer1;         //xy=432.00000381469727,225.00000381469727
AudioAnalyzeRMS          rms2;           //xy=433.00000762939453,359.0000047683716
AudioAnalyzePeak         peak2;          //xy=435.00000762939453,399.0000057220459
AudioAnalyzeFFT1024      fft1024_3;      //xy=566.0000076293945,225.00000286102295
AudioConnection          patchCord1(i2s1, 0, queue1, 0);
AudioConnection          patchCord2(i2s1, 0, peak1, 0);
AudioConnection          patchCord3(i2s1, 0, rms1, 0);
AudioConnection          patchCord4(i2s1, 0, fft1024_1, 0);
AudioConnection          patchCord5(i2s1, 0, mixer1, 0);
AudioConnection          patchCord6(i2s1, 0, i2s2, 0);
AudioConnection          patchCord7(i2s1, 1, queue2, 0);
AudioConnection          patchCord8(i2s1, 1, fft1024_2, 0);
AudioConnection          patchCord9(i2s1, 1, rms2, 0);
AudioConnection          patchCord10(i2s1, 1, peak2, 0);
AudioConnection          patchCord11(i2s1, 1, mixer1, 1);
AudioConnection          patchCord12(i2s1, 1, i2s2, 1);
AudioConnection          patchCord13(mixer1, fft1024_3);
mSGTL5000                sgtl5000_1;
// GUItool: end automatically generated code

/// <summary>
/// setup
/// </summary>
void setup() {
    // link the button events   
    btn.attachClick(buttonClick);
    btn.attachDoubleClick(buttonDoubleClick);
    btn.attachLongPressStart(buttonLongPressStart);
    btn.attachLongPressStop(buttonLongPressStop);
    btn.attachDuringLongPress(buttonLongPress);

    // set inputs
    pinMode(IR, INPUT_PULLUP); // IR

    // start the ir receiver
    irrecv.enableIRIn();

    // set bin ranges
    setBins();

    // Audio
    AudioMemory(64);
    sgtl5000_1.enable();
    sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
    sgtl5000_1.lineOutLevel(13); // 3.16 Volts p-p
    sgtl5000_1.volume(0.7f);     // 0.8 corresponds to the maximum undistorted output for a full scale signal
    sgtl5000_1.lineInLevel(0);   // 3.12 Volts p-p
    sgtl5000_1.attGAIN(1);       // ADC volume range reduction down by 6.0 dB

    // dsp
    sgtl5000_1.autoVolumeDisable();
    sgtl5000_1.surroundSoundDisable();
    sgtl5000_1.enhanceBassDisable();

    // fft
    fft1024_1.windowFunction(AudioWindowHanning1024);
    fft1024_2.windowFunction(AudioWindowHanning1024);
    fft1024_3.windowFunction(AudioWindowHanning1024);

    // display
    while (!tft.begin(SPI_SPEED)) delay(1000);

    tft.setScroll(0);
    tft.setFramebuffers(internalBuffer); // set 1 internal framebuffer -> activate float buffering
    tft.setDiffBuffers(&diff1, &diff2);  // set the 2 diff buffers => activate differential updates 
    tft.setDiffGap(4);                   // use a small gap for the diff buffers
    tft.setRefreshRate(120);             // around 120hz for the display refresh rate 
    tft.setVSyncSpacing(2);              // set framerate = refreshrate/2 (and enable vsync at the same time) 

    // make sure backlight is on
    if (PIN_BACKLIGHT != 255) {
        pinMode(PIN_BACKLIGHT, OUTPUT);
        digitalWrite(PIN_BACKLIGHT, HIGH);
    }

    // force reset to factory defaults
    if (digitalRead(BUTTON) == LOW) {
        clearEEPROM();
        // waiting for release the button
        while (digitalRead(BUTTON) == LOW) {}
    }

    // get config from EEPROM or initialize EEPROM
    if (isFirstStart()) {
        clearEEPROM();
        delay(3000);
    } else {
        readGLOBALConfig();
        readDBCorrection();
        readDIGITALConfig();
        readANALOGConfig();
        readFFTConfig();
        readGONIOConfig();
        readREMOTECONTROLConfig();
    }

    // show logo
#if SHOWLOGO
    showLogo();
#endif

    // initialize display from ModuleType
    initalizeDisplayFromModuleType(moduleType);
}

/// <summary>
/// mainloop
/// </summary>
void loop() {
    static elapsedMillis fps = 0;

    // watching the button
    btn.tick();

    // watching the serialport
    getFromSerial();

    // watching the IR sensor
    if (irrecv.decode(&results)) {
        if (DIGITAL_MenuActive) decodeMenuDigital(results.value);
        else if (ANALOG_MenuActive) decodeMenuAnalog(results.value);
        else if (FFT_MenuActive) decodeMenuFFT(results.value);
        else if (GONIO_MenuActive) decodeMenuGoniometer(results.value);
        else decodeIR(results.value);
        irrecv.resume();
    }

    // draw modules
    if (!lockScreenUpdate && !DIGITAL_MenuActive && !ANALOG_MenuActive && !FFT_MenuActive && !GONIO_MenuActive) {
        if (moduleType == 0) drawDigital(false);
        else if (moduleType == 1) drawAnalog(false);
        else if (moduleType == 2) drawFFT(false);
        else if (moduleType == 3) drawGonio(false);

        // send global RMS & PPM data
        if (calibrate && fps > 16) { fps = 0; sendRMSPPM(); }
    }
}

/// <summary>
/// samples = sampleblocks * 128
/// </summary>
void setSampleBlocks() {
    lockScreenUpdate = true;
    queue1.freeBuffer();
    queue2.freeBuffer();
    memset(samplesLeft, 0, sizeof(samplesLeft));
    memset(samplesRight, 0, sizeof(samplesRight));
    GONIO_SampleBlocks = 8;
    if (GONIO_Samples == 1) GONIO_SampleBlocks = 16;
    lockScreenUpdate = false;
}

/// <summary>
/// get samples for left and right channel
/// </summary>
/// <param name="blocks"></param>
void getSamples(uint16_t blocks) {
    if (blocks == 0) {
        queue1.readBuffer();
        queue1.freeBuffer();
        queue2.readBuffer();
        queue2.freeBuffer();
    } else {
        if (queue1.available() >= blocks && queue2.available() >= blocks) {
            for (byte i = 0; i < blocks; i++) {
                memcpy(&samplesLeft[i * 128], queue1.readBuffer(), 256);
                memcpy(&samplesRight[i * 128], queue2.readBuffer(), 256);
                queue1.freeBuffer();
                queue2.freeBuffer();
            }
        }
    }
}

/// <summary>
/// this function will be called when the button was pressed 1 time
/// </summary>
void buttonClick() {
    if (++moduleType > 3) moduleType = 0;
    modeSwitch();
}

/// <summary>
/// this function will be called when the button was pressed 2 times in a int16_t timeframe
/// </summary>
void buttonDoubleClick() {
    setSubMode(moduleType);
}

/// <summary>
/// this function will be called often, while the button is pressed for a long time
/// </summary>
void buttonLongPress() {
}

/// <summary>
/// this function will be called once, when the button is pressed for a long time
/// </summary>
void buttonLongPressStart() {
    calibrate = !calibrate;
    initalizeDisplayFromModuleType(moduleType);
}

/// <summary>
/// this function will be called once, when the button is released after beeing pressed for a long time
/// </summary>
void buttonLongPressStop() {
}

/// <summary>
/// switch module
/// </summary>
void modeSwitch() {
    lockScreenUpdate = true;
    EEPROM.put(20, moduleType);
    initalizeDisplayFromModuleType(moduleType);
    Serial.printf("{ModuleType=%i}", moduleType);
    lockScreenUpdate = false;
}

/// <summary>
/// initialize display from ModuleType
/// </summary>
/// <param name="mType"></param>
void initalizeDisplayFromModuleType(int16_t mType) {
    if (mType < 0 || mType > 3) { mType = 0; moduleType = 0; }

    lockScreenUpdate = true;
    if (mType == 0) displayInitDigital(true);
    if (mType == 1) displayInitAnalog();
    if (mType == 2) displayInitFFT(true);
    if (mType == 3) displayInitGonio(true);
    lockScreenUpdate = false;
}

/// <summary>
/// set subMode
/// </summary>
/// <param name="mType"></param>
void setSubMode(int16_t mType) {
    lockScreenUpdate = true;
    switch (mType) {
    case 0: // digitalmeter
        if (++DIGITAL_WorkMode > 3)  DIGITAL_WorkMode = 0;
        EEPROM.put(504, DIGITAL_WorkMode);
        Serial.print("{DIGITAL_WorkMode=" + String(DIGITAL_WorkMode) + "}");
        setMode(true);
        break;
    case 1:
        break;
    case 2: // spectrumanalyzer
        if (++FFT_WorkMode > 11) FFT_WorkMode = 0;
        EEPROM.put(706, FFT_WorkMode);
        Serial.print("{FFT_WorkMode=" + String(FFT_WorkMode) + "}");
        setMode(true);
        break;
    case 3: // goniometer
        if (++GONIO_WorkMode > 7) GONIO_WorkMode = 0;
        EEPROM.put(806, GONIO_WorkMode);
        Serial.print("{GONIO_WorkMode=" + String(GONIO_WorkMode) + "}");
        setMode(true);
        break;
    }
    lockScreenUpdate = false;
}

/// <summary>
/// set mode
/// </summary>
/// <param name="showMessage"></param>
void setMode(bool showMessage) {
    switch (moduleType) {
    case 0: // digitalmeter
        switch (DIGITAL_WorkMode) {
        case 0:
            DIGITAL_dBLow = -40;
            DIGITAL_PeakHold = false;
            if (showMessage) messageBox("-40dB");
            break;
        case 1:
            DIGITAL_dBLow = -30;
            DIGITAL_PeakHold = false;
            if (showMessage) messageBox("-30dB");
            break;
        case 2:
            DIGITAL_dBLow = -40;
            DIGITAL_PeakHold = true;
            if (showMessage) messageBox("-40dB HOLD");
            break;
        case 3:
            DIGITAL_dBLow = -30;
            DIGITAL_PeakHold = true;
            if (showMessage) messageBox("-30dB HOLD");
            break;
        }
        break;

    case 1: // analogmeter
        break;

    case 2: // spectrumanalyzer
        switch (FFT_WorkMode) {
        case 0:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = false;
            if (showMessage) messageBox("RMS -40dB");
            break;
        case 1:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = false;
            if (showMessage) messageBox("PPM -40dB");
            break;
            /*****************************************/
        case 2:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = false;
            if (showMessage) messageBox("RMS -30dB");
            break;
        case 3:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = false;
            if (showMessage) messageBox("PPM -30dB");
            break;
            /*****************************************/
        case 4:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD -40dB");
            break;
        case 5:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("P.HOLD -40dB");
            break;
            /*****************************************/
        case 6:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD -30dB");
            break;
        case 7:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("P.HOLD -30dB");
            break;
            /*****************************************/
        case 8:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD4 -40dB");
            break;
        case 9:
            FFT_dBLow = -40;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("P.HOLD4 -40dB");
            break;
            /*****************************************/
        case 10:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 1 /*RMS*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD4 -30dB");
            break;
        case 11:
            FFT_dBLow = -30;
            FFT_LevelBarMode = 0 /*PEAK*/;
            FFT_PeakHold = true;
            if (showMessage) messageBox("P.HOLD4 -30dB");
            break;
        }
        break;

    case 3: // goniometer
        switch (GONIO_WorkMode) {
        case 0:
            GONIO_dBLow = -40;
            GONIO_BarMode = 1 /*RMS*/;
            GONIO_PeakHold = false;
            if (showMessage) messageBox("RMS -40dB");
            break;
        case 1:
            GONIO_dBLow = -40;
            GONIO_BarMode = 0 /*PEAK*/;
            GONIO_PeakHold = false;
            if (showMessage) messageBox("PPM -40dB");
            break;
        case 2:
            GONIO_dBLow = -30;
            GONIO_BarMode = 1 /*RMS*/;
            GONIO_PeakHold = false;
            if (showMessage) messageBox("RMS -30dB");
            break;
        case 3:
            GONIO_dBLow = -30;
            GONIO_BarMode = 0 /*PEAK*/;
            GONIO_PeakHold = false;
            if (showMessage) messageBox("PPM -30dB");
            break;
        case 4:
            GONIO_dBLow = -40;
            GONIO_BarMode = 1 /*RMS*/;
            GONIO_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD -40dB");
            break;
        case 5:
            GONIO_dBLow = -40;
            GONIO_BarMode = 0 /*PEAK*/;
            GONIO_PeakHold = true;
            if (showMessage) messageBox("P.HOLD -40dB");
            break;
        case 6:
            GONIO_dBLow = -30;
            GONIO_BarMode = 1 /*RMS*/;
            GONIO_PeakHold = true;
            if (showMessage) messageBox("RMS.HOLD -30dB");
            break;
        case 7:
            GONIO_dBLow = -30;
            GONIO_BarMode = 0 /*PEAK*/;
            GONIO_PeakHold = true;
            if (showMessage) messageBox("P.HOLD -30dB");
            break;
        }
        break;
    }
}

/// <summary>
/// start queue
/// </summary>
void queueStart() {
    queue1.clear();
    queue2.clear();
    queue1.begin();
    queue2.begin();
}

/// <summary>
/// stop queue
/// </summary>
void queueStop() {
    queue1.end();
    queue2.end();
    queue1.clear();
    queue2.clear();
    queue1.freeBuffer();
    queue2.freeBuffer();
}