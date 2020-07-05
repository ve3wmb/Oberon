/*
  Oberon.ino - QRSS (slow speed CW) Beacon for PicoBallon using Si5351a and ATTINY85

  Oberon is the mythical king of the fairies who appears as a character in William Shakespeare's play "A Midsummer Nights Dream".
  This code was written during Midsummer of 2020 for project SPRITE, so the name seemed appropriate.

  The majority of this code is derived from the QRSS/FSKCW/DFCW Beacon Keyer by Hans Summers, G0UPL(copyright 2012)
  and used with his permission for this derivitive work. The original source code is from here :
  https://qrp-labs.com/images/qrssarduino/qrss.ino

  Adapted by Michael, VE3WMB to use the Si5351a as a transmitter for Orion WSPR Beacon and ported to ATTINY85 for the
  Sprite QRSS PicoBallon Project.

  The transmit_glyph()function was provided by Graham, VE3GTC and modified slightly to fit our needs.

  Copyright (C) 2020 Michael Babineau <mbabineau.ve3wmb@gmail.com>

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
#include "OberonConfig.h"

#if defined (SI5351A_USES_SOFTWARE_I2C)
#include <SoftWire.h>  // Needed for Software I2C otherwise include <Wire.h>
#else
#include <Wire.h>
#endif

#if defined (OBERON_DEBUG_MODE)
#include <TimeLib.h>
#define debugSerial Serial
#define MONITOR_SERIAL_BAUD 9600
#endif



// Likewise for Glyph timing parameters
#define GLYPH_SYMBOL_TIME                200          // in milliseconds                  300
#define GLYPH_CHARACTER_SPACE           1200          // in milliseconds
#define GLYPH_TONE_SPACING               300          // 100 = 1 hz in 100'ths of hertz   300 or 220
#define GLYPH_TRANSMIT_OFFSET            100          // in hertz

/***************************************
    SI5351a definitions and macros
****************************************/
uint64_t si5351bx_vcoa = (SI5351BX_XTAL*SI5351BX_MSA);  // 25mhzXtal calibrate
int32_t  si5351_correction = SI5351A_CLK_FREQ_CORRECTION;  //Frequency correction factor
uint8_t  si5351bx_rdiv = 0;             // 0-7, CLK pin sees fout/(2**rdiv) // Note that 0 means divide by 1
uint8_t  si5351bx_drive[3] = {3, 3, 3}; // 0=2ma 1=4ma 2=6ma 3=8ma for CLK 0,1,2 - Set CLK 0,1,2 to 8ma
uint8_t  si5351bx_clken = 0xFF;         // Private, all CLK output drivers off
uint64_t beacon_tx_frequency_hz;        // This is used qrss_beacon() so that we can have a different frequency for regular CW vs QRSS Xmissions

// Macros used by the KE7ER Si5351 Code
#define BB0(x) ((uint8_t)x)             // Bust int32 into Bytes
#define BB1(x) ((uint8_t)(x>>8))
#define BB2(x) ((uint8_t)(x>>16))



#define RFRAC_DENOM 1000000ULL
#define SI5351_CLK_ON true
#define SI5351_CLK_OFF false

// Si5351a forward definitions
// Turn the specified clock number on or off.
void si5351bx_enable_clk(uint8_t clk_num, bool on_off);

// Initialize the Si5351
void si5351bx_init();

// Set the frequency for the specified clock number
// Note that fout is in hertz x 100 (i.e. hundredths of hertz).
// Frequency range must be between 500 Khz and 109 Mhz
// Boolean tx_on specifies whether clock is enabled after frequency
// change.

void si5351bx_setfreq(uint8_t clknum, uint64_t fout, bool tx_on);

// Forward definitions for QRSS Beacon code
enum QrssMode {MODE_NONE, MODE_QRSS, MODE_FSKCW, MODE_DFCW};
enum QrssSpeed {s12wpm, QRSS3, QRSS6, QRSS10};
const char msg[] = QRSS_MESSAGE;

// This array is indexed by a parameter of type QrssSpeed
const unsigned int speeds[] = {1, 30, 60, 100};   // Speeds for: s12wpm, QRSS3, QRSS6, QRSS10

void qrss_beacon(QrssMode mode, QrssSpeed speed);

// Create an instance of Softwire named Wire if using Software I2C
#if defined (SI5351A_USES_SOFTWARE_I2C)
SoftWire Wire = SoftWire();
#endif// Create an instance of Softwire named Wire if using Software I2C

// Debug logging
enum debugLogType {STARTUP, GLYPH_TX, GLYPH_TX_STOP, QRSS_TX, QRSS_TX_STOP, WAIT};
void debugLog( debugLogType type, QrssMode mode, QrssSpeed speed);

/** *************  SI5315 routines - (tks Jerry Gaffke, KE7ER)   ***********************
   A minimalist standalone set of Si5351 routines originally written by Jerry Gaffke, KE7ER
   but modified by VE3WMB for use with Software I2C and to provide sub-Hz resolution for WSPR
   transmissions.

   VCOA is fixed at 875mhz, VCOB not used.
   The output msynth dividers are used to generate 3 independent clocks
   with 1hz resolution to any frequency between 4khz and 109mhz.

   Usage:
   Call si5351bx_init() once at startup with no args;

   Call si5351bx_setfreq(clknum, freq, tx_on) each time one of the
   three output CLK pins is to be updated to a new frequency.
   The bool tx_on determines whether the clock is enabled after the
   frequency change.

   A freq of 0 also serves to shut down that output clock or alternately a
   call to si5351bx_enable_clk(uint8_t clk_num, bool on_off)

   The global variable si5351bx_vcoa starts out equal to the nominal VCOA
   frequency of 25mhz*35 = 875000000 Hz.  To correct for 25mhz crystal errors,
   the user can adjust this value.  The vco frequency will not change but
   the number used for the (a+b/c) output msynth calculations is affected.
   Example:  We call for a 5mhz signal, but it measures to be 5.001mhz.
   So the actual vcoa frequency is 875mhz*5.001/5.000 = 875175000 Hz,
   To correct for this error:     si5351bx_vcoa=875175000;

   Most users will never need to generate clocks below 500khz.
   But it is possible to do so by loading a value between 0 and 7 into
   the global variable si5351bx_rdiv, be sure to return it to a value of 0
   before setting some other CLK output pin.  The affected clock will be
   divided down by a power of two defined by  2**si5351_rdiv

   A value of zero gives a divide factor of 1, a value of 7 divides by 128.
   This lightweight method is a reasonable compromise for a seldom used feature.
*/

// Write a single 8 bit value to an Si5351a register address
void i2cWrite(uint8_t reg, uint8_t val) {   // write reg via i2c
  Wire.beginTransmission(SI5351BX_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// Write an array of 8bit values to an Si5351a register address
void i2cWriten(uint8_t reg, uint8_t *vals, uint8_t vcnt) {  // write array
  Wire.beginTransmission(SI5351BX_ADDR);
  Wire.write(reg);
  while (vcnt--) Wire.write(*vals++);
  Wire.endTransmission();
}

// Turn the specified clock number on or off.
void si5351bx_enable_clk(uint8_t clk_num, bool on_off) {
  if (on_off == SI5351_CLK_OFF ) { // Off Disable ClK
    si5351bx_clken |= 1 << clk_num;      //  Set Bit to shut down the clock
  }
  else {  // Enable CLK
    si5351bx_clken &= ~(1 << clk_num);   // Clear bit to enable clock
  }
  i2cWrite(3, si5351bx_clken);
}

// Initialize the Si5351a
void si5351bx_init() {                  // Call once at power-up, start PLLA
  uint8_t reg;  uint32_t msxp1;
  Wire.begin();
  i2cWrite(149, 0);                     // SpreadSpectrum off
  i2cWrite(3, si5351bx_clken);          // Disable all CLK output drivers
  i2cWrite(183, ((SI5351BX_XTALPF << 6) | 0x12)); // Set 25mhz crystal load capacitance (tks Daniel KB3MUN)
  msxp1 = 128 * SI5351BX_MSA - 512;     // and msxp2=0, msxp3=1, not fractional
  uint8_t  vals[8] = {0, 1, BB2(msxp1), BB1(msxp1), BB0(msxp1), 0, 0, 0};
  i2cWriten(26, vals, 8);               // Write to 8 PLLA msynth regs
  i2cWrite(177, 0x20);                  // Reset PLLA  (0x80 resets PLLB)
}


// Set the frequency for the specified clock number
// Note that fout is in hertz x 100 (i.e. hundredths of hertz).
// Frequency range must be between 500 Khz and 109 Mhz
// An fout value of 0 will shutdown the specified clock.

void si5351bx_setfreq(uint8_t clknum, uint64_t fout, bool tx_on)
{
  // Note that I am not being lazy here in naming variables. If you refer to SiLabs
  // application note AN619 - "Manually Generating an Si5351 Register Map", the formulas
  // within refer to calculating values named a,b,c and p1, p2, p3.
  // For consistency I continue to use the same notation, even though the calculations appear
  // a bit cryptic.
  uint64_t a, b, c, ref_freq;
  uint32_t p1, p2, p3;
  uint8_t vals[8];

  if ((fout < 50000000) || (fout > 10900000000)) {  // If clock freq out of range 500 Khz to 109 Mhz
    si5351bx_clken |= 1 << clknum;      //  shut down the clock
    i2cWrite(3, si5351bx_clken);
  }

  else {

    // Determine the integer part of feedback equation
    ref_freq = si5351bx_vcoa;
    ref_freq = ref_freq + (int32_t)((((((int64_t)si5351_correction) << 31) / 1000000000LL) * ref_freq) >> 31);
    a = ref_freq / fout;
    b = (ref_freq % fout * RFRAC_DENOM) / fout;
    c = b ? RFRAC_DENOM : 1;

    p1 = 128 * a + ((128 * b) / c) - 512;
    p2 = 128 * b - c * ((128 * b) / c);
    p3 = c;

    // Setup the bytes to be sent to the Si5351a register
    vals[0] = (p3 & 0x0000FF00) >> 8;
    vals[1] = p3 & 0x000000FF;
    vals[2] = (p1 & 0x00030000) >> 16;
    vals[3] = (p1 & 0x0000FF00) >> 8;
    vals[4] = p1 & 0x000000FF;
    vals[5] = (((p3 & 0x000F0000) >> 12) | ((p2 & 0x000F0000) >> 16));
    vals[6] = (p2 & 0x0000FF00) >> 8;
    vals[7] = p2 & 0x000000FF;
    i2cWriten(42 + (clknum * 8), vals, 8); // Write to 8 msynth regs
    i2cWrite(16 + clknum, 0x0C | si5351bx_drive[clknum]); // use local msynth

    if (tx_on == true)
      si5351bx_clken &= ~(1 << clknum);   // Clear bit to enable clock
    else
      si5351bx_clken |= 1 << clknum;      //  Set bit to shut down the clock

    i2cWrite(3, si5351bx_clken); // Enable/disable clock
  }

}


byte charCode(char c) {

  // This function returns the encoded CW pattern for the character passed in.
  // Binary encoding is left-padded. Unused high-order bits are all ones.
  // The first zero is the start bit, which is discarded.
  // Processing from higher to lower order, bits we skip over ones, then discard first 0 (start bit). The next bit is the first element.
  // We process each element sending a DIT or DAH, until we reach the end of the pattern.
  //
  // Pattern encoding is 0 = DIT, 1 = DAH.
  // So 'A' = B11111001, which is 1 1 1 1 1 (padding bits) 0 (start bit)  0 1 (dit, dah)
  // This excellent encoding scheme was developed by Hans, G0UPL as noted above.

  switch (c)
  {
    case 'A':  return B11111001; break;    // A  .-
    case 'B':  return B11101000; break;    // B  -...
    case 'C':  return B11101010; break;    // C  -.-.
    case 'D':  return B11110100; break;    // D  -..
    case 'E':  return B11111100; break;    // E  .
    case 'F':  return B11100010; break;    // F  ..-.
    case 'G':  return B11110110; break;    // G  --.
    case 'H':  return B11100000; break;    // H  ....
    case 'I':  return B11111000; break;    // I  ..
    case 'J':  return B11100111; break;    // J  .---
    case 'K':  return B11110101; break;    // K  -.-
    case 'L':  return B11100100; break;    // L  .-..
    case 'M':  return B11111011; break;    // M  --
    case 'N':  return B11111010; break;    // N  -.
    case 'O':  return B11110111; break;    // O  ---
    case 'P':  return B11100110; break;    // P  .--.
    case 'Q':  return B11101101; break;    // Q  --.-
    case 'R':  return B11110010; break;    // R  .-.
    case 'S':  return B11110000; break;    // S  ...
    case 'T':  return B11111101; break;    // T  -
    case 'U':  return B11110001; break;    // U  ..-
    case 'V':  return B11100001; break;    // V  ...-
    case 'W':  return B11110011; break;    // W  .--
    case 'X':  return B11101001; break;    // X  -..-
    case 'Y':  return B11101011; break;    // Y  -.--
    case 'Z':  return B11101100; break;    // Z  --..
    case '0':  return B11011111; break;    // 0  -----
    case '1':  return B11001111; break;    // 1  .----
    case '2':  return B11000111; break;    // 2  ..---
    case '3':  return B11000011; break;    // 3  ...--
    case '4':  return B11000001; break;    // 4  ....-
    case '5':  return B11000000; break;    // 5  .....
    case '6':  return B11010000; break;    // 6  -....
    case '7':  return B11011000; break;    // 7  --...
    case '8':  return B11011100; break;    // 8  ---..
    case '9':  return B11011110; break;    // 9  ----.
    case ' ':  return B11101111; break;    // Space - equal to 4 dah lengths
    case '/':  return B11010010; break;    // /  -..-.
    default: return charCode(' ');
  }
}

void setRfFsk(boolean rf_on, boolean setFSK_high)
{
  uint8_t fsk_value;

  // Determine if there is an FSK shift and if so adjust the frequency
  // accordingly before turning on the Si5351a clock.

  if (setFSK_high == true) {
    fsk_value = FSK_HIGH;
  }
  else {
    fsk_value = FSK_LOW;
  }

  if (rf_on == true) {
    si5351bx_setfreq(SI5351A_TX_CLK_NUM, ((beacon_tx_frequency_hz + fsk_value) * 100ULL), SI5351_CLK_ON );
  }
  else {
    si5351bx_enable_clk(SI5351A_TX_CLK_NUM, SI5351_CLK_OFF); // Disable the TX clock
  }
}


//
bool qrss_transmit(QrssMode mode, QrssSpeed ditSpeed)
{
  static byte timerCounter;              // Counter to get to divide by 100 to get 10Hz
  static int ditCounter;                 // Counter to time the length of each dit
  static byte pause;                     // Generates the pause between characters
  static byte msgIndex = 255;            // Index into the message
  static byte character;                 // Bit pattern for the character being sent
  static byte key;                       // State of the key
  static byte charBit;                   // Which bit of the bit pattern is being sent

  static boolean dah;                    // True when a dah is being sent
  byte divisor;                          // Divide 1kHz by 100 normally, but by 33 when sending DFCW)
  bool transmission_done = false;

  // Set Divisor based on Mode
  if (mode == MODE_DFCW)                 // Divisor is 33 for DFCW, to get the correct timing
    divisor = 33;                        // (inter-symbol gap is 1/3 of a dit)
  else
    divisor = 100;                      // For ever other mode it is one dit length


  timerCounter++;                        // 1000Hz at this point

  if (timerCounter == divisor)           // Divides by 100 (or 33 for DFCW)
  {
    timerCounter = 0;                    // 10 Hz here (30Hz for DFCW)
    ditCounter++;                        // Generates the correct dit-length

    if (ditCounter >= speeds[ditSpeed]) { // We have counted the duration of a dit

      ditCounter = 0;

      if (!pause) {
        // Pause is set to 2 after the last element of the character has been sent
        key--;                         // This generates the correct pause between characters (3 dits)
        if ((!key) && (!charBit)) {

          if (mode == MODE_DFCW)
            pause = 3;                 // DFCW needs an extra delay to make it 4/3 dit-length
          else
            pause = 2;
        }
      } // end if (!pause)
      else
        pause--;


      // Key becomes 255 when the last element (dit or dah) of the character has been sent
      if (key == 255) {

        // Done sending the last element (dit or dah) in the character
        // If the last symbol of the character has been sent, get the next character

        if (!charBit) {
          // Increment the message character index
          msgIndex++;

          // If we are at the end of the message flag transmission_done
          if (!msg[msgIndex]) {
            msgIndex = 0;
            transmission_done = true;
          }
          else {

            // Get the encoded bit pattern for the morse character
            character = charCode(msg[msgIndex]);
            // Start at the 7'th (leftmost) bit of the bit pattern
            charBit = 7;

            // Loop through bits looking for a 0, signifying start of coding bits
            while (character & (1 << charBit)) charBit--;
          }

        } // end if (!charBit)

        charBit--;                     // Move to the next rightermost bit of the pattern, this is the first element


        if ((transmission_done == true) || (character == charCode(' ') )) { // Special case for space
          key = 0;
          dah = false;
        }
        else {
          // Get the state of the current bit in the pattern
          key = character & (1 << charBit);

          if (key) {                     // If it's a 1, set this to a dah
            key = 3;
            dah = true;
          }
          else {                        // otherwise it's a dit
            if (mode == MODE_DFCW)     // Special case for DFCW - dit's and dah's are both
              key = 3;                 // the same length.
            else
              key = 1;

            dah = false;
          }
        }
      } // end if (key == 255 )

      if (!key) dah = false;


      //
      if (mode == MODE_FSKCW)
      {
        //setRF(true);                    // in FSK/CW mode, the RF output is always ON
        //setFSK(key);                    // and the FSK depends on the key state
        setRfFsk(true, key);
      }
      else if (mode == MODE_QRSS)
      {
        //setRF(key);                     // in QRSS mode, the RF output is keyed
        //setFSK(false);                  // and the FSK is always off
        setRfFsk(key, false);
      }
      else if (mode == MODE_DFCW)
      {
        //setRF(key);                     // in DFCW mode, the RF output is keyed (ON during a dit or a dah)
        //setFSK(dah);                    // and the FSK depends on the key state
        setRfFsk(key, dah);
      }
      else
        setRfFsk(false, false);

    } // end if (ditcounter >= speeds[ditspeed];

  } // end if (timercounter == divisor)


  if (transmission_done == true) {
    // reset the static variables for the next transmission
    timerCounter = 0;
    ditCounter = 0;
    pause = 0;
    msgIndex = 255;
    character = 0;
    key = 0;
    charBit = 0;
    dah = false;
  }

  return transmission_done;

} // end qrss_transmit function


void qrss_beacon(QrssMode tx_mode, QrssSpeed tx_speed) {

  static unsigned long milliPrev;        // Static variable stores previous millisecond count
  unsigned long milliNow;
  bool done_transmission = false;

  // Since we are using FSKCW, turn on the clock now to let it warm up, delay one second and then turn on TX
  si5351bx_setfreq(SI5351A_TX_CLK_NUM, ((beacon_tx_frequency_hz) * 100ULL), SI5351_CLK_OFF );
  delay(1000);
  si5351bx_enable_clk(SI5351A_TX_CLK_NUM, SI5351_CLK_ON);

  // Turn off the PARK clock
  si5351bx_enable_clk(SI5351A_PARK_CLK_NUM, SI5351_CLK_OFF);

  debugLog(QRSS_TX, tx_mode, tx_speed);

  while (!done_transmission) {
    milliNow = millis();                   // Get millisecond counter value

    if (milliNow != milliPrev)             // If one millisecond has elapsed, call the beacon() function
    {
      milliPrev = milliNow;
      done_transmission = qrss_transmit(tx_mode, tx_speed); // This gets called once per millisecond (i.e 1000 times per second)
    }
  } // end while (!done)

  // Ensure that the Si5351a TX clock is shutdown
  si5351bx_enable_clk(SI5351A_TX_CLK_NUM, SI5351_CLK_OFF);

  // Re-enable the Park Clock
  si5351bx_setfreq(SI5351A_PARK_CLK_NUM, (PARK_FREQ_HZ * 100ULL), SI5351_CLK_ON);

  debugLog(QRSS_TX_STOP, tx_mode, tx_speed);

} // end qrss_beacon()


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    Transmit GLYPH message
//
//  This code is courtest of Graham, VE3GTC.
//
//  Each glyph character is defined as a set of pixels within a 5 column by 7 row matrix
//
//  Each character is transmitted one pixel at a time starting with the character's left most column
//  and least significant bit
//
//  Each character is defined as five bytes each reprsenting one column of the transmitted glyph.
//
//  For example:  column1, column2, column3, column4, column5
//
//  Within each of these bytes, bit 1 through bit 7 ( MSB ) is used where a 1 represents a pixel that is ON and 0 a pixel
//  that is off. Note that bit 0 ( LSB ) is not used.
//
//  As an example, the character C is defined as the five bytes 0x7C, 0x82, 0x82, 0x82, 0x44
//
//          | b | b | b | b | b                                  b b b b b
//          | y | y | y | y | y                                  y y y y y
//          | t | t | t | t | t                                  t t t t t
//          | e | e | e | e | e                                  e e e e e
//      bit | 1 | 2 | 3 | 4 | 5                            bit   1 2 3 4 5
//    ===========================     results in ==>      ==================
//    MSB 7 | 0 | 1 | 1 | 1 | 0 |                        MSB 7     * * *
//        6 | 1 | 0 | 0 | 0 | 1 |                            6   *       *
//        5 | 1 | 0 | 0 | 0 | 0 |                            5   *
//        4 | 1 | 0 | 0 | 0 | 0 |                            4   *
//        3 | 1 | 0 | 0 | 0 | 0 |                            3   *
//        2 | 1 | 0 | 0 | 0 | 1 |                            2   *       *
//        1 | 0 | 1 | 1 | 1 | 0 |                            1     * * *
//    LSB 0 | 0 | 0 | 0 | 0 | 0 |                        LSB 0
//
//  Each bit is offset upwards in frequency from the base transmit frequency.
//
//                                                     b b b b b
//                                                     y y y y y
//                                                     t t t t t
//                                                     e e e e e
//                                               bit   1 2 3 4 5
//                                            =================
//    base frequency + ( 7 * tone spacing )    MSB 7     * * *
//    base frequency + ( 6 * tone spacing )        6   *       *
//    base frequency + ( 5 * tone spacing )        5   *
//    base frequency + ( 4 * tone spacing )        4   *
//    base frequency + ( 3 * tone spacing )        3   *
//    base frequency + ( 2 * tone spacing )        2   *       *
//    base frequency + ( 1 * tone spacing )        1     * * *
//                                             LSB 0
//
//  Character set:
//
//     adapated from:  http://www.banburyares.co.uk/TechGroup/Arduino/HELLSCHEIBER.pdf

//         { 0x00, 0x00, 0x00, 0x00, 0x00, },             //  <space>
//         { 0x04, 0x08, 0x10, 0x20, 0x40, },             //  0 ZERO
//         { 0x7C, 0x8A, 0x92, 0xA2, 0x7C, },             //  / slash
//         { 0x00, 0x42, 0xFE, 0x02, 0x00, },             //  1
//         { 0x42, 0x86, 0x8A, 0x92, 0x62, },             //  2
//         { 0x84, 0x82, 0xA2, 0xD2, 0x8C, },             //  3
//         { 0x18, 0x28, 0x48, 0xFE, 0x08, },             //  4
//         { 0xE4, 0xA2, 0xA2, 0xA2, 0x9C, },             //  5
//         { 0x3C, 0x52, 0x92, 0x92, 0x0C, },             //  6
//         { 0x80, 0x8E, 0x90, 0xA0, 0xC0, },             //  7
//         { 0x6C, 0x92, 0x92, 0x92, 0x6C, },             //  8
//         { 0x60, 0x92, 0x92, 0x94, 0x78, },             //  9
//         { 0x7E, 0x88, 0x88, 0x88, 0x7E, },             //  A
//         { 0xFE, 0x92, 0x92, 0x92, 0x6C, },             //  B
//         { 0x7C, 0x82, 0x82, 0x82, 0x44, },             //  C
//         { 0xFE, 0x82, 0x82, 0x44, 0x38, },             //  D
//         { 0xFE, 0x92, 0x92, 0x92, 0x82, },             //  E
//         { 0xFE, 0x90, 0x90, 0x90, 0x80, },             //  F
//         { 0x7C, 0x82, 0x92, 0x92, 0x5E, },             //  G
//         { 0xFE, 0x10, 0x10, 0x10, 0xFE, },             //  H
//         { 0x00, 0x82, 0xFE, 0x82, 0x00, },             //  I
//         { 0x04, 0x02, 0x82, 0xFC, 0x80, },             //  J
//         { 0xFE, 0x10, 0x28, 0x44, 0x82, },             //  K
//         { 0xFE, 0x02, 0x02, 0x02, 0x02, },             //  L
//         { 0xFE, 0x40, 0x30, 0x40, 0xFE, },             //  M
//         { 0xFE, 0x20, 0x10, 0x08, 0xFE, },             //  N
//         { 0x7C, 0x82, 0x82, 0x82, 0x7C, },             //  O
//         { 0xFE, 0x90, 0x90, 0x90, 0x60, },             //  P
//         { 0x7C, 0x82, 0x8A, 0x84, 0x7A, },             //  Q
//         { 0xFE, 0x90, 0x98, 0x94, 0x62, },             //  R
//         { 0x62, 0x92, 0x92, 0x92, 0x8C, },             //  S
//         { 0x80, 0x80, 0xFE, 0x80, 0x80, },             //  T
//         { 0xFC, 0x02, 0x02, 0x02, 0xFC, },             //  U
//         { 0xF8, 0x04, 0x02, 0x04, 0xF8, },             //  V
//         { 0xFC, 0x02, 0x1C, 0x02, 0xFC, },             //  W
//         { 0xC6, 0x28, 0x10, 0x28, 0xC6,},              //  X
//         { 0xE0, 0x10, 0x0E, 0x10, 0xE0, },             //  Y
//         { 0x86, 0x8A, 0x92, 0xA2, 0xC2, },             //  Z
//         { 0xFE, 0x42, 0x10, 0x42, 0XFE  },             //  Dagal - Viking rune meaning hope/happiness (looks like "|><|"  )
//
//  Each character in the desired message is encoded in the glyph array rather tha search through a large predefined
//  character for inividual characters in a message, this in the hopes of saving a bit of memory space so that this might
//  be squeezed into an attiny85 microcontroller.
//
//  For a six character message this would result in 6 x 5 = 30 bytes rather than 38 x 5 = 190 bytes.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void transmit_glyph()
{
  const uint16_t      GLYPH_SymbolTime       = GLYPH_SYMBOL_TIME;
  const uint16_t      GLYPH_ToneSpaceing     = GLYPH_TONE_SPACING;
  const uint16_t      GLYPH_CharacterSpace   = GLYPH_CHARACTER_SPACE;

  unsigned long       TransmitOffset;
  unsigned long       MEPT_Frequency;
  unsigned long       previous_millis;


  byte glyphs[][5] = {

    { 0xF8, 0x04, 0x02, 0x04, 0xF8, },                   // V
    { 0x00, 0x00, 0x00, 0x00, 0x00, },                   // Space
    { 0xF8, 0x04, 0x02, 0x04, 0xF8  }                    // V
  };
  

  TransmitOffset = GLYPH_TRANSMIT_OFFSET;

  MEPT_Frequency = beacon_tx_frequency_hz;

  si5351bx_setfreq( SI5351A_TX_CLK_NUM, MEPT_Frequency * 100, SI5351_CLK_OFF );

  int row;
  int column ;
  int pixel;

  debugLog(GLYPH_TX, 0 , 0);

  for ( row = 0; row <= ( sizeof( glyphs ) / 5 - 1); row++ ) {                // for each row in message glyphs[row][column]

    for ( column = 0; column <= 4; column++ ) {                               // for each character pixel column glyphs[row][0] to [row][4]

      for ( pixel = 1; pixel <= 7; pixel++ ) {                                // for each column bit from b1 to b7

        if ( bitRead(glyphs[row][column], pixel ) == 1 ) {                    // pixel ON

          si5351bx_setfreq( SI5351A_TX_CLK_NUM, ( MEPT_Frequency * 100 ) + ( pixel * GLYPH_ToneSpaceing ), SI5351_CLK_ON );

          previous_millis = millis();

          while ( millis() - previous_millis < GLYPH_SymbolTime ) {
            // just loop til done!
          }

          // si5351.output_enable(SI5351_CLK0, TX_OFF);
          si5351bx_enable_clk( SI5351A_TX_CLK_NUM, SI5351_CLK_OFF );

        } else {                                                            // pixel OFF

          previous_millis = millis();

          while ( millis() - previous_millis < GLYPH_SymbolTime ) {
            // just loop til done!
          }

        }
      }
    }

    previous_millis = millis();

    while ( millis() - previous_millis < GLYPH_CharacterSpace ) {
      // just loop til done!
    }

  }

  si5351bx_enable_clk( SI5351A_TX_CLK_NUM, SI5351_CLK_OFF );
  debugLog(GLYPH_TX_STOP, 0 , 0);
}


// Debug Serial code conditionally compiled

#if defined (OBERON_DEBUG_MODE)

void print_date_time() {
  debugSerial.print(year());
  debugSerial.print(F("-"));
  debugSerial.print(month());
  debugSerial.print(F("-"));
  debugSerial.print(day());
  debugSerial.print(F(" "));
  debugSerial.print(hour());
  debugSerial.print(F(":"));
  debugSerial.print(minute());
  debugSerial.print(F(":"));
  debugSerial.print(second());
  debugSerial.print(F(" "));

}

void debugLog( debugLogType type, QrssMode mode, QrssSpeed speed) {
  // enum debugLogType {STARTUP, GLYPH_TX, GLYPH_TX_STOP, QRSS_TX, QRSS_TX_STOP, WAIT};
  print_date_time();
  switch (type) {

    case  STARTUP :
      debugSerial.print(F(" *** Startup *** - Oberon Code Version & HW : "));
      debugSerial.print(F(OBERON_CODE_VERSION));
      debugSerial.println(F(BOARDNAME));
      break;

    case  GLYPH_TX :
      debugSerial.println(F(" Glyph Tx Start "));
      break;

    case  GLYPH_TX_STOP :
      debugSerial.println(F(" Glyph Tx Completed "));
      break;

    case  QRSS_TX : {

        unsigned long freq_hz = beacon_tx_frequency_hz; // To get around problem with typing and print.
        debugSerial.print(F(" QRSS TX Start : "));
        debugSerial.print(F("QrssMode: "));
        debugSerial.print(mode);
        debugSerial.print(F(" QrssSpeed: "));
        debugSerial.print(speed);
        debugSerial.print(F(" Freq_hz: "));
        debugSerial.println(freq_hz);
      }
      break;

    case  QRSS_TX_STOP :
      debugSerial.println(F(" QRSS TX Stop"));
      break;

    case  WAIT :
      debugSerial.println(F(" ... WAIT ..."));

      break;

    default :
      break;
  }

}

#else
// This is the do-nothing version of debugLog that is compiled when OBERON_DEBUG_MODE is not defined
void debugLog( debugLogType type, QrssMode mode, QrssSpeed speed) {
  return;
}
#endif

// End of conditional compilation


/*************************
         SETUP
 ************************/
void setup() {

  pinMode(LED_BUILTIN, OUTPUT);

#if defined (OBERON_DEBUG_MODE)
  debugSerial.begin(MONITOR_SERIAL_BAUD);
#endif

  // Setup the Si5351a
  si5351bx_init();
  delay (10000);

  // Set the Park Clock Frequency and enable it
  si5351bx_setfreq(SI5351A_PARK_CLK_NUM, (PARK_FREQ_HZ * 100ULL), SI5351_CLK_ON);

  // Setup for QRSS FSKCW transmission
  beacon_tx_frequency_hz = QRSS_BEACON_BASE_FREQ_HZ + QRSS_BEACON_FREQ_OFFSET_HZ;

  digitalWrite(LED_BUILTIN, LOW);
  debugLog(STARTUP, 0, 0);
}


/*************************
     main body loop
 ************************/
void loop() {

  digitalWrite(LED_BUILTIN, HIGH); // Indicate that the Beacon is transmitting
  transmit_glyph(); // Send a character glyph to help id the transmission
  qrss_beacon(MODE_FSKCW, QRSS6); // FSKCW at QRSS06 (i.e. 6 second dits)
  digitalWrite(LED_BUILTIN, LOW); // Not transmitting
  debugLog(WAIT, 0, 0);
  
  delay(POST_TX_DELAY_MS);

  // Setup for conventional CW transmission in hopes of getting RBN spots
  //beacon_tx_frequency_hz = CW_BEACON_FREQ_HZ;
  //qrss_beacon(MODE_QRSS, s12wpm); //CW at 12 wpm
  

}
