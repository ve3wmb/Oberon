# Oberon
 Simple and Small QRSS Beacon for Sprite pico-balloon Project
 
 Oberon.ino - QRSS (slow speed CW) Beacon for PicoBallon using Si5351a and ATTINY85(ultimately)

  Oberon is the mythical king of the fairies who appears as a character in William Shakespeare's play "A Midsummer Nights Dream".
  This code was written during Midsummer of 2020 for project Sprite, so the name seemed appropriate.

  The majority of this code is derived from the QRSS/FSKCW/DFCW Beacon Keyer by Hans Summers, G0UPL(copyright 2012)
  and used with his permission for this derivitive work. The original source code is from here :
  https://qrp-labs.com/images/qrssarduino/qrss.ino

  Adapted by Michael, VE3WMB to use the Si5351a as a transmitter for Orion WSPR Beacon and to be ported to ATTINY85 for the
  Sprite QRSS PicoBallon Project. Public domain Si5351a code written by Gerry, KE7ER is also used.

  The transmit_glyph() function was provided by Graham, VE3GTC.

  Copyright (C) 2020 Michael Babineau <mbabineau.ve3wmb@gmail.com>

Version History
- v0.01 Initial prototype submission, runs on Arduino UNO/QRP Labs Arduino Shield, with Si5351a Synthesizer board.