/* mbed I2CTextLCD Library, for a 4-bit LCD driven by I2C and a PCF8574
 * Copyright (c) 2007-2010, sford, rcocking
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
 */

/* This is a hack of Simon Ford's direct-driven TextLCD code.
* It should be refactored to extract a common superclass
* but my C++ isn't yet good enough :) 
*
* The code assumes the following connections between the PCF8574
* and the LCD
* 
* nc  - D0
* nc  - D1
* nc  - D2
* nc  - D3
* P0  - D4
* P1  - D5
* P2  - D6
* P3  - D7
* P4  - E
* P5  - nc
* P6  - nc
* P7  - RS
* gnd - R/W
*
* D0-3 of the LCD are not connected because we work in 4-bit mode
* R/W is hardwired to gound, as we only ever write to the LCD
* A0-2 on the PCF8574 can be set in any combination; you will need to modify
* the I2C address in the I2CTextLCD constructor.
* Remember that the mbed uses 8-bit addresses, which should be
* in the range 0x40-0x4E for the PCF8574
*/
#include "TextLCD.h"
#include "mbed.h"


TextLCD::TextLCD(PinName sda, PinName scl, int i2cAddress, LCDType type) : _i2c(sda, scl), _i2cAddress(i2cAddress) , _type(type){
   // _i2cAddress = i2cAddress;
    writeByte(E_ON,false);
    wait_us(15000);        // Wait 15ms to ensure powered up

    // send "Display Settings" 3 times (Only top nibble of 0x30 as we've got 4-bit bus)
    for (int i=0; i<3; i++) {
        writeByte(0x3, false);
        wait_us(1700);  // this command takes 1.64ms, so wait for it
    }
    writeByte(0x2, false);     // 4-bit mode
    wait_us(40);    // most instructions take 40us

    writeCommand(0x28); // Function set 001 BW N F - -
    writeCommand(0x0C);
    writeCommand(0x6);  // Cursor Direction and Display Shift : 0000 01 CD S (CD 0-left, 1-right S(hift) 0-no, 1-yes
    cls();
}

void TextLCD::character(int column, int row, int c) {
    int a = address(column, row);
    writeCommand(a);
    writeData(c);
}

void TextLCD::cls() {
    writeCommand(0x01); // cls, and set cursor to 0
    wait_us(1700);     // This command takes 1.64 ms
    locate(0, 0);
}

void TextLCD::locate(int column, int row) {
    _column = column;
    _row = row;
}

int TextLCD::_putc(int value) {
    if (value == '\n') {
        _column = 0;
        _row++;
        if (_row >= rows()) {
            _row = 0;
        }
    } else {
        character(_column, _row, value);
        _column++;
        if (_column >= columns()) {
            _column = 0;
            _row++;
            if (_row >= rows()) {
                _row = 0;
            }
        }
    }
    return value;
}

int TextLCD::_getc() {
    return -1;
}

void TextLCD::writeI2CByte(int data) {
    // equivalent to writeI2CByte
    char cmd[1];
    cmd[0] = data;
    _i2c.write(_i2cAddress, cmd, 1);
}

void TextLCD::writeNibble(int data, bool rs) {
    data = data << 4;
    data |= BL_ON; // E on
    if (rs) {
        data = data | RS_ON; // set rs bit
    }
    data |= E_ON; // E on
    writeI2CByte(data);
    data ^= E_ON; // E off
    wait_us(40);
    writeI2CByte(data);
    wait_us(1000);
  //  wait_us(1000);
}

void TextLCD::writeByte(int data, bool rs) {
    writeNibble(data >> 4, rs);
    writeNibble(data & 0x0F, rs);
}

void TextLCD::writeCommand(int command) {
    // equivalent to ard commandWrite
    writeByte(command, false);
}

void TextLCD::writeData(int data) {

    writeByte(data, true);
}

int TextLCD::address(int column, int row) {
    switch (_type) {
        case LCD20x4:
            switch (row) {
                case 0:
                    return 0x80 + column;
                case 1:
                    return 0xc0 + column;
                case 2:
                    return 0x94 + column;
                case 3:
                    return 0xd4 + column;
            }
        case LCD16x2B:
            return 0x80 + (row * 40) + column;
        case LCD16x2:
        case LCD20x2:
        default:
            return 0x80 + (row * 0x40) + column;
    }
}

int TextLCD::columns() {
    switch (_type) {
        case LCD20x4:
        case LCD20x2:
            return 20;
        case LCD16x2:
        case LCD16x2B:
        default:
            return 16;
    }
}

int TextLCD::rows() {
    switch (_type) {
        case LCD20x4:
            return 4;
        case LCD16x2:
        case LCD16x2B:
        case LCD20x2:
        default:
            return 2;
    }
}
