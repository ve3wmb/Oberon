#ifndef OBERONCONFIG_H
#define OBERONCONFIG_H
/*
    BBTechATTiny85Config.h - Configuration File for Oberon Tiny QRSS Beacon

   Copyright (C) 2018-2019 Michael Babineau <mbabineau.ve3wmb@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// This file contains all of the user configurable parameters which control the QRSS Beacon but also
// customize the software, via conditional compile, for the hardware it is running on.



#define OBERON_CODE_VERSION "v0.02"
#define BOARDNAME " BBTechATTiny85"

// Comment this out for flight to disable debug mode or to reduce code footprint
//#define OBERON_DEBUG_MODE // Comment this out for normal flight operation

#define TARGET_PROCESSOR_ATTINY85

/***************************************
   Hardware Configuration Parameters
***************************************/
//#define SI5351A_USES_SOFTWARE_I2C    // Uncomment this if using software I2C to communicate with the Si5351a

/***************************************
   Configuration Parameters for Beacon
***************************************/
#define PARK_FREQ_HZ          108000000ULL  // Use this on clk SI5351A_PARK_CLK_NUM to keep the SI5351a warm to avoid thermal drift during WSPR transmissions. Max 109 Mhz.

// Regular CW beacon parameter (12wpm for RBN)
#define CW_BEACON_FREQ_HZ     14063000UL        // This is the fixed beacon frequency for 12 wpm morse, which will hopefully be picked up by the Reverse Beacon Network. 
#define CW_BEACON_MESSAGE     "TEST TEST DE VE3WMB/B VE3WMB/B"

// Note: actual QRSS TX Frequency = QRSS_BEACON_BASE_FREQ_HZ + QRSS_BEACON_FREQ_OFFSET_HZ
#define QRSS_BEACON_BASE_FREQ_HZ       QRSS_BASE_FREQUENCY_30m  // Change this to move to a different band
#define QRSS_BEACON_FREQ_OFFSET_HZ     10UL  // Change this to add to QRSS_BEACON_BASE_FREQ_HZ to determine your actual QRSS TX frequency
#define QRSS_MESSAGE "  VE3WMB "       // Message - put your callsign here, in capital letters. I recommend two spaces before and one after callsign
#define POST_TX_DELAY_MS   300000     // - Five minutes. Delay after Transmission before repeating.


#define QRSS_BEACON_FSK_OFFSET_HZ 4    // DITS will be transmitted at QRSS_BEACON_BASE_FREQ_HZ + QRSS_BEACON_FREQ_OFFSET_HZ and DAHS QRSS_BEACON_FSK_OFFSET_HZ 
                                       // higher for FSKCW. It is recommended that this be less that 5Hz so that stations can be spaced every 5 Hz on a band.
#define FSK_HIGH  QRSS_BEACON_FSK_OFFSET_HZ
#define FSK_LOW 0


/*************************************
    Si5351a Configuration Parameters
**************************************/
// Si5351a Clock Definitions
#define SI5351A_PARK_CLK_NUM    1    // The Si5351a Clock Number output used to mimic the QRP Labs U3S Park feature. This needs to be an unused clk port.

#define SI5351A_TX_CLK_NUM  0       // The Si5351a Clock Number output used for the Beacon Transmission

#define SI5351BX_XTALPF   3         // Crystal Load Capacitance 1:6pf  2:8pf  3:10pf -  assuming 10 pF, otherwise change

#define SI5351BX_ADDR 0x60              // I2C address of Si5351   (typical)

// If using 27mhz crystal, set XTAL=27000000, MSA=33.  Then vco=891mhz.
#define SI5351BX_XTAL 2500000000ULL      // Crystal freq in centi Hz 
#define SI5351BX_MSA  35                // VCOA is at 25mhz*35 = 875mhz

//  You need to calibrate your Si5351a and substitute your correction value for SI5351A_CLK_FREQ_CORRECTION below.
#define SI5351A_CLK_FREQ_CORRECTION   11219  // Correction value for Si5351a clock

/*************************************
    QRSS Base Frequencies per band
**************************************/
// These frequencies shouldn't have to change so don't touch them! Change

// The idea is to modify QRSS_BEACON_FREQ_OFFSET_HZ to modify where in the QRSS
// window on your selected band that you are transmitting.
#define QRSS_BASE_FREQUENCY_40m     7039800UL        // base frequency for 40m QRSS wwith extends upwards 200hz - i.e.  7039800 to  7040000Hz
#define QRSS_BASE_FREQUENCY_30m    10139900UL        // base frequency for 30m QRSS wwith extends upwards 200hz - i.e. 10139900 to 10140100Hz
#define QRSS_BASE_FREQUENCY_20m    14096800UL        // base frequency for 20m QRSS wwith extends upwards 200hz - i.e. 14096800 to 14097000Hz
#define QRSS_BASE_FREQUENCY_10m    28000700UL        // base frequency for 10m QRSS which extends upwards 200hz - i.e. 700 to 900

#endif
