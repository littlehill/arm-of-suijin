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
#include "ds3231.h"

#define ESC 0x1B

#define MY_VERSION_MAJOR 1
#define MY_VERSION_MINOR 0

#define MAIN_LOOP_DELAY_MS 10
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
#define RED_LED_PIN     PC_8
#define WHITE_LED_PIN     PC_6

// #define HW_SERIAL_TX_PIN PA_2
// #define HW_SERIAL_RX_PIN PA_3

#define SELECT_BTN_PIN  PB_5
#define ENTER_BTN_PIN   PB_3



DigitalOut red_led(RED_LED_PIN);
DigitalOut white_led(WHITE_LED_PIN);

DigitalIn btn_select(SELECT_BTN_PIN);
DigitalIn btn_enter(SELECT_BTN_PIN);
DigitalOut motor_A(PB_7); //stromecek
DigitalOut motor_B(PB_8); //kvetinace
/*---------------------------*/


// SDA // SCL // addr // type           
TextLCD lcd(PA_10, PA_9, 0x4E, TextLCD::LCD16x2);

//rtc object
Ds3231 rtc(PA_10, PA_9);

void btn_debounce(unsigned char sel_read, unsigned char enter_read, bool * sel_out, bool * enter_out) {
    static unsigned char loc_select = 0;
    static unsigned char loc_enter = 0;

    const unsigned char mask = 0x0F;

    loc_select = (loc_select << 1) | (sel_read & 0x01);
    loc_enter = (loc_enter << 1) | (enter_read & 0x01);


    if ((loc_select&mask) == mask)
        *sel_out = true;
    else
        *sel_out = false;

    if ((loc_enter&mask) == mask)
        *enter_out = true;
    else
        *enter_out = false;
}


void get_user_input(char* message, uint8_t min, uint8_t max, uint32_t* member);
void get_user_input(char* message, uint8_t min, uint8_t max, bool* member);

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
    
    time_t epoch_time;

    //DS3231 rtc variables

    //default, use bit masks in ds3231.h for desired operation
    ds3231_cntl_stat_t rtc_control_status = {0,0};
    ds3231_time_t rtc_time;
    ds3231_calendar_t rtc_calendar;

    lcd.printf("Hi mbed World!\nSetup time.");

rtc.set_cntl_stat_reg(rtc_control_status);


#ifdef FORCE_TIME_SETUP_MANUAL
    //get day from user
    get_user_input("\nPlease enter day of week, 1 for Sunday (1-7): ", 1,
                    7, &rtc_calendar.day);

    //get day of month from user
    get_user_input("\nPlease enter day of month (1-31): ", 1, 31, &rtc_calendar.date);

    //get month from user
    get_user_input("\nPlease enter the month, 1 for January (1-12): ", 1, 12, &rtc_calendar.month);

    //get year from user
    get_user_input("\nPlease enter the year (0-99): ",0, 99, &rtc_calendar.year);

    //Get time mode
    get_user_input("\nWhat time mode? 1 for 12hr 0 for 24hr: ", 0, 1, &rtc_time.mode);
    if(rtc_time.mode)
    {
        //Get AM/PM status
        get_user_input("\nIs it AM or PM? 0 for AM 1 for PM: ", 0, 1, &rtc_time.am_pm);
        //Get hour from user
        get_user_input("\nPlease enter the hour (1-12): ", 1, 12, &rtc_time.hours);
    }
    else
    {
        //Get hour from user
        get_user_input("\nPlease enter the hour (0-23): ", 0, 23, &rtc_time.hours);
    }

    //Get minutes from user
    get_user_input("\nPlease enter the minute (0-59): ", 0, 59, &rtc_time.minutes);

    //Get seconds from user
    get_user_input("\nPlease enter the second (0-59): ", 0, 59, &rtc_time.seconds);

    
    //Set the time, uses inverted logic for return value
    if(rtc.set_time(rtc_time))
    {
        printf("\nrtc.set_time failed!!\n");
        exit(0);
    }
    
    //Set the calendar, uses inverted logic for return value
    if(rtc.set_calendar(rtc_calendar))
    {
        printf("\nrtc.set_calendar failed!!\n");
        exit(0);
    }
#endif    

    char buffer[32];
    ds3231_time_t gl_time;

    ds3231_time_t watter_time1;

    watter_time1.hours = 22;
    watter_time1.minutes = 10;
    watter_time1.seconds = 07;

    bool trigger_manual;

    bool input_select, input_enter;

    int count=0;
    
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

        btn_debounce(btn_select.read(), btn_enter.read(), &input_select, &input_enter);
        //trigger_manual = button.read();

        if ((timenow - heartbeatTime) > HBLED_TIME_MS ) {
            red_led = !red_led;
            heartbeatTime = timenow;

            epoch_time = rtc.get_epoch();
            rtc.get_time(&gl_time);

            lcd.locate(0,0);
//            lcd.printf("epoch = %d\n", epoch_time);
//            lcd.locate(0,1);
//            lcd.printf("%s", ctime(&epoch_time));
            lcd.printf("time: %2d:%02d:%02d", gl_time.hours, gl_time.minutes, gl_time.seconds);
            lcd.locate(0,1);
            lcd.printf("next: %2d:%02d:%02d", watter_time1.hours, watter_time1.minutes, watter_time1.seconds);

            HAL_Delay(5);
            //new epoch time fx
        }

        white_led.write((int)input_enter);

        HAL_Delay(MAIN_LOOP_DELAY_MS);
        loopCount++;
    }
}




/**********************************************************************
* Function: get_user_input() 
* Parameters: message - user prompt
*             min - minimum value of input
*             max - maximum value of input
*             member - pointer to struct member              
* Returns: none
*
* Description: get time/date input from user
*
**********************************************************************/
void get_user_input(char* message, uint8_t min, uint8_t max, uint32_t* member)
{
    uint32_t temp;

    do
    {
        printf("\n%s", message);

        //for some reason mbed doesn't like a pointer to a member in scanf
        //term.scanf("%d", member); works with gcc on RPi
        scanf("%d", &temp);

        *member = temp;

        if((*(member)< min) || (*(member) > max))
        {
            printf("\nERROR-RTI");
        }
    }
    while((*(member) < min) || (*(member) > max));
}


void get_user_input(char* message, uint8_t min, uint8_t max, bool* member)
{
    uint32_t temp;

    do
    {
        printf("\n%s", message);

        //for some reason mbed doesn't like a pointer to a member in scanf
        //term.scanf("%d", member); works with gcc on RPi
        scanf("%d", &temp);

        *member = temp;

        if((*(member)< min) || (*(member) > max))
        {
            printf("\nERROR-RTI");
        }
    }
    while((*(member) < min) || (*(member) > max));
}

/**********************************************************************
* Function: compare_times
* Parameters: ds3231_time_t *A
*             ds3231_time_t *B
* Returns: int
*
* Description: compares 2 times
* -- if A > B (A is later in the day than B), returns 1
* -- if A = B (A is same time as B, including mode), returns 0
* -- if A < B (B is later in the day than A), returns 2
* -- on error returns -1
*
**********************************************************************/

int compare_times(ds3231_time_t *p_tA, ds3231_time_t *p_tB) {

    //if (p_tA->hours > p_tB)

return 0;
}