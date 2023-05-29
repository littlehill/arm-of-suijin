/*
 *******************************************************************************
 * Project:		F411_usbTerminal
 * Component:
 * Element:
 * File: main.cpp
 *
 * __Description:__
 * usbserial terminal for small uC like STM32F411/F103 (black/blue-pill board etc.)
 * main reason for creating this:
 * - to have something that can be easily modified for custom fixtures in automated testing
 * - digital IO or analog input readout+controll over serial allows for easy prototyping from shell/terminal on host
 *
 * __Credit:__
 * terminal implemetation: https://os.mbed.com/users/murilopontes/code/tinyshell/
 * printf 'hack' https://forums.mbed.com/t/redirecting-printf-to-usb-serial-in-mbed-os-6/14279
 * usbserial doc: https://os.mbed.com/handbook/USBSerial
 *
 * __History:__
 * Version      Date        Author      Description
 * --------------------------------------------------------------------------- *
 * 1.0			28/Jul/2022	Mirek			Initial
 *******************************************************************************/

#include "mbed.h"
#include <cstdint>

#include "TextLCD.h"


#define MY_VERSION_MAJOR 1
#define MY_VERSION_MINOR 0

#define MAIN_LOOP_DELAY_MS 50
#define HBLED_TIME_MS 1000
#define USB_CONNECTED_WAIT_MS 500

#define USB_ECHO_ENABLED 1
#define USB_RX_RINGBUFFER_SIZE  128

#define HW_SERIAL_BAUDRATE  115200
#define HW_ECHO_ENABLED 1
#define HW_RX_RINGBUFFER_SIZE  128

//¬24h (60*60*24 * 1000)
#define DAY_IN_MS 86400000
//86400000 

//first loop from POR
#define FIRST_LOOP 10000  
#define BUFFER_TIME 2000
#define A_RUNTIME_STROMEK 30000
#define B_RUNTIME_KVETINAC 65000

// Standardized LED and button names
#define LED1_PIN     PC_13   // blackpill on-board led
#define BUTTON1_PIN  PA_0  // blackpill on-board button
#define HW_SERIAL_TX_PIN PA_2
#define HW_SERIAL_RX_PIN PA_3



DigitalOut led1(LED1_PIN);
DigitalOut red_led(PC_8);
DigitalIn button(BUTTON1_PIN);
DigitalOut motor_A(PB_7); //stromecek
DigitalOut motor_B(PB_8); //kvetinace
/*---------------------------*/

// SDA // SCL // addr // type           
TextLCD lcd(PA_10, PA_9, 0x4E, TextLCD::LCD16x2);

int main()
{
   // hwserial.attach(&rxhandler_hwserial, SerialBase::RxIrq);
    unsigned int loopCount = 0;
    unsigned int heartbeatTime = 0;
    

    uint32_t timenow = HAL_GetTick();

    unsigned int motor_Aon = timenow + FIRST_LOOP;
    unsigned int motor_Aoff = timenow + DAY_IN_MS + 1000000;
    unsigned int motor_Bon = timenow + DAY_IN_MS + 1000000;
    unsigned int motor_Boff = timenow + DAY_IN_MS + 1000000;
    

    bool trigger_manual;

    int count=0;
    lcd.printf("Hi mbed World!\n");
    
    HAL_Delay(1000);
    lcd.cls();
    //lcd.locate(1,2);
    lcd.locate(0, 0);
    lcd.putc('A');
    lcd.locate(0, 1);
    lcd.putc('B');
    lcd.locate(2, 0);
    lcd.putc('2');
    lcd.locate(2, 1);
    lcd.putc('2');
    lcd.locate(15, 0);
    lcd.putc('H');
    lcd.locate(15, 1);
    lcd.putc('L');

    HAL_Delay(2000);
    lcd.locate(0, 1);
    lcd.printf("count: ");

    while (true)
    {
        timenow = HAL_GetTick();

        //trigger_manual = button.read();

        if ((timenow - heartbeatTime) > HBLED_TIME_MS ) {
            red_led = !red_led;
            heartbeatTime = timenow;

            lcd.locate(8,1); 
            lcd.printf("%3d",count++);
            HAL_Delay(500); 
        }

        HAL_Delay(MAIN_LOOP_DELAY_MS);
        loopCount++;
    }
}
