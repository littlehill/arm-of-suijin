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
#include <cstdio>

#include "TextLCD.h"
#include "ds3231.h"

#include "main_types.h"

#define VERSION_MAJOR 2
#define VERSION_MINOR 0

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
#define FIRST_LOOP 10
#define PAUSE_TIME 1
#define A_RUNTIME_STROMEK 25
#define B_RUNTIME_KVETINAC 36

// Standardized LED and button names
#define LED1_PIN        PC_13   // blackpill on-board led
#define RED_LED_PIN     PC_8
#define WHITE_LED_PIN   PC_6
#define GREEN_LED_PIN   PB_7
#define BLUE_LED_PIN    PB_8

#define FAN_EN_PIN    PB_15

// #define HW_SERIAL_TX_PIN PA_2
// #define HW_SERIAL_RX_PIN PA_3

#define SELECT_BTN_PIN  PA_15
#define ENTER_BTN_PIN   PB_5



DigitalOut red_led(RED_LED_PIN);
DigitalOut blue_led(BLUE_LED_PIN);

DigitalIn btn_select(SELECT_BTN_PIN);
DigitalIn btn_enter(ENTER_BTN_PIN);
DigitalOut motor_A(GREEN_LED_PIN); //stromecek
DigitalOut motor_B(WHITE_LED_PIN); //kvetinace
DigitalOut fan_en(FAN_EN_PIN);
/*---------------------------*/

bool flag_wattering_in_progress = false;
e_MENU_SCREEN gl_menu_screen = e_MENU_SCREEN::ScrHome;


// SDA // SCL // addr // type           
TextLCD lcd(PA_10, PA_9, 0x4E, TextLCD::LCD16x2);

//rtc object
Ds3231 rtc(PA_10, PA_9);

float rtcTempC = -120;

void btn_debounce(unsigned char sel_read, unsigned char enter_read, bool * sel_out, bool * enter_out);
void get_user_input(char* message, uint8_t min, uint8_t max, uint32_t* member);
void get_user_input(char* message, uint8_t min, uint8_t max, bool* member);
int compare_times(ds3231_time_t *p_tA, ds3231_time_t *p_tB);
void add2times(ds3231_time_t *p_target, uint32_t h_add, uint32_t m_add, uint32_t s_add);
void process_state(e_EVENT event);
void process_fan(float temp);
void update_screen(e_BTN_EVENT btn_input, ds3231_time_t *p_now, ds3231_time_t *p_target );
void set_next_time(ds3231_time_t *p_target);


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

    lcd.printf("Suijin v%d.%d\nENTER for menu", VERSION_MAJOR, VERSION_MINOR);

            
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

    ds3231_time_t next_wattering_time;

    next_wattering_time.hours = 23;
    next_wattering_time.minutes = 12;
    next_wattering_time.seconds = 35;

    bool trigger_manual;

    bool input_select, input_enter;
    bool previous_select, previous_enter;

    previous_enter = 0;
    previous_select = 0;
    input_select = 0;
    input_enter = 0;

    int count=0;
    
    e_EVENT main_event = e_EVENT::EventNone;

    rtc.get_time(&gl_time);
    next_wattering_time = gl_time;
    set_next_time(&next_wattering_time);

    HAL_Delay(4000);
    lcd.cls();
    //lcd.locate(1,2);

    while (true)
    {
        timenow = HAL_GetTick();

        btn_debounce(btn_select.read(), btn_enter.read(), &input_select, &input_enter);
        //trigger_manual = button.read();

        main_event = e_EVENT::EventNone;

        if ((timenow - heartbeatTime) > HBLED_TIME_MS ) {
            red_led = !red_led;
            heartbeatTime = timenow;

            rtc.get_time(&gl_time);

            update_screen(e_BTN_EVENT::BtnNone, &gl_time, &next_wattering_time);
            
            //printf("select debug: %d\r\n", btn_select.read());

            lcd.locate(15,1);
            if (compare_times(&gl_time, &next_wattering_time) == 0) { //gl_time has passed the wattering time
                blue_led.write(true);
                main_event = e_EVENT::EventTriggerWattering;
                
                set_next_time(&next_wattering_time);

            } else {
                blue_led.write(false);
            }

            rtcTempC = ((rtc.get_temperature()>>6) / 4.0);
            process_fan(rtcTempC);

            process_state(main_event);
            HAL_Delay(5);
            //new epoch time fx
        }

        if ((previous_enter != input_enter)) {
            previous_enter = input_enter;
            printf("ENTER: %d\n",input_enter);
            //enter pressed
            if (input_enter==1)
                update_screen(e_BTN_EVENT::BtnPressedEnter, &gl_time, &next_wattering_time);
        }
        if (previous_select != input_select) {
            previous_select = input_select;
            printf("SELECT: %d\n",input_select);
            //select pressed
            if (input_select==1)
                update_screen(e_BTN_EVENT::BtnPressedSelect, &gl_time, &next_wattering_time);
        }

        HAL_Delay(MAIN_LOOP_DELAY_MS);
        loopCount++;
    }
}




void btn_debounce(unsigned char sel_read, unsigned char enter_read, bool * sel_out, bool * enter_out) {
    static unsigned char loc_select = 0;
    static unsigned char loc_enter = 0;

    const unsigned char mask = 0xFF;

    loc_select = (loc_select << 1) | (sel_read & 0x01);
    loc_enter = (loc_enter << 1) | (enter_read & 0x01);


    if (loc_select == mask)
        *sel_out = true;
    else
        *sel_out = false;

    if (loc_enter == mask)
        *enter_out = true;
    else
        *enter_out = false;
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
* -- if A = B (A is same time as B), returns 0
* -- if A < B (B is later in the day than A), returns -1
*
* TODO: add support for different modes, function now assumes same format on both parameters
* TODO: add error detection on "impossible time"
**********************************************************************/

int compare_times(ds3231_time_t *p_tA, ds3231_time_t *p_tB) {

    if ( p_tA->hours > p_tB->hours) return 1;
    if ( p_tA->hours < p_tB->hours) return -1;

    // now hours can be assumed equal
    if ( p_tA->minutes > p_tB->minutes) return 1;
    if ( p_tA->minutes < p_tB->minutes) return -1;

    // now hours&minutes can be assumed equal
    if ( p_tA->seconds > p_tB->seconds) return 1;
    if ( p_tA->seconds < p_tB->seconds) return -1;

//all is equal
return 0;
}

/**********************************************************************
* Function: add2times
* Parameters: ds3231_time_t *target
*             ds3231_time_t offset
* Returns: --
*
* Description: adds offset time to the target, wraps values around
*
* TODO: add support for am/pm modes
**********************************************************************/
void add2times(ds3231_time_t *p_target, uint32_t h_add, uint32_t m_add, uint32_t s_add) {

    p_target->seconds += s_add;
    if (p_target->seconds > 59) {
        p_target->seconds = p_target->seconds%60;
        m_add += 1;
    };

    p_target->minutes += m_add;
    if (p_target->minutes > 59) {
        p_target->minutes = p_target->minutes%60;
        h_add += 1;
    };

    p_target->hours += h_add;
    if (p_target->hours > 23) {
        p_target->hours = p_target->hours%24;
    };

}


void process_state(e_EVENT event) {
    static e_SUIJIN_STATE state = e_SUIJIN_STATE::WaitingForNextCycle;

    static time_t time_transition = 0;

    time_t time_now = rtc.get_epoch();

    switch (state) {
        case e_SUIJIN_STATE::InitSetup:
            time_transition = time_now + A_RUNTIME_STROMEK;
            state = e_SUIJIN_STATE::RunningPump_A;
            flag_wattering_in_progress = true;
            fan_en.write(MOTOR_ENABLE);
            printf("SMinf: Exit InitSetup\r\n");
            //break;

        case e_SUIJIN_STATE::RunningPump_A:
            motor_A.write(MOTOR_ENABLE);
            if (time_now > time_transition) {
                time_transition = time_now + PAUSE_TIME;
                state = e_SUIJIN_STATE::Pause_A;
                printf("SMinf: Exit Running PumpA\r\n");
            }
            break;

        case e_SUIJIN_STATE::Pause_A:
            motor_A.write(MOTOR_DISABLE);
            if (time_now > time_transition) {
                time_transition = time_now + B_RUNTIME_KVETINAC;
                state = e_SUIJIN_STATE::RunningPump_B;
                printf("SMinf: Exit Pause_A\r\n");
            }
            break;

        case e_SUIJIN_STATE::RunningPump_B:
            motor_B.write(MOTOR_ENABLE);
            if (time_now > time_transition) {
                time_transition = time_now + PAUSE_TIME;
                state = e_SUIJIN_STATE::Pause_B;
                printf("SMinf: Exit Running PumpB\r\n");
            }
            break;

        case e_SUIJIN_STATE::Pause_B:
            motor_B.write(MOTOR_DISABLE);
            if (time_now > time_transition) {
                //time_transition = time_now + PAUSE_TIME;
                state = e_SUIJIN_STATE::Appendix;
                printf("SMinf: Exit Pause_B\r\n");
            }
            break;

        case e_SUIJIN_STATE::Appendix:
            state = e_SUIJIN_STATE::WaitingForNextCycle;
            flag_wattering_in_progress = false;
            fan_en.write(MOTOR_DISABLE);
            printf("SMinf: Exit Appendix\r\n");
            //break;

        case e_SUIJIN_STATE::WaitingForNextCycle:
            if (event == e_EVENT::EventTriggerWattering) {
                state = e_SUIJIN_STATE::InitSetup;
                printf("SMinf: Exit waiting\r\n");
            }
            break;
    };

return;
}

void process_fan(float tempC) {
    bool fstatus = fan_en.read();
    bool ftarget;

    if (flag_wattering_in_progress) {
        return;
    }
    ftarget = fstatus;
    if (tempC > 34.0) {
        ftarget = MOTOR_ENABLE;
    }
    if (tempC < 28.0) {
        ftarget = MOTOR_DISABLE;
    }

    if (fstatus != ftarget) {
        fan_en.write(ftarget);
        printf("Fan updated to: %d at temp %.2f\r\n", ftarget, tempC);
    }
}

void update_screen(e_BTN_EVENT btn_input, ds3231_time_t *p_now, ds3231_time_t *p_target ) {
    static e_MENU_SCREEN screen_set = e_MENU_SCREEN::ScrHome;

        // if (input_enter) {
        //     next_wattering_time = gl_time;
        //     add2times(&next_wattering_time , 0, 0, 21);
        // }

    switch (screen_set) {
        case e_MENU_SCREEN::ScrHome:
            lcd.locate(0,0);
            lcd.printf("time: %2d:%02d:%02d  ", p_now->hours, p_now->minutes, p_now->seconds);
            lcd.locate(0,1);
            lcd.printf("next: %2d:%02d:%02d  ", p_target->hours, p_target->minutes, p_target->seconds);

            if (btn_input == e_BTN_EVENT::BtnPressedEnter) {
                screen_set = e_MENU_SCREEN::ScrManualTrigger;
            }
            if (btn_input == e_BTN_EVENT::BtnPressedSelect) {
                //*p_target = *p_now;
                set_next_time(p_target);
            }
            break;

        case e_MENU_SCREEN::ScrManualTrigger:
            lcd.locate(0,0);
            lcd.printf("Entr= run in 15s");
            lcd.locate(0,1);
            lcd.printf("Esc = return    ");
            if (btn_input == e_BTN_EVENT::BtnPressedEnter) {
                //printf("Manual wattering trig.\r\n");
                *p_target = *p_now;
                add2times(p_target , 0, 0, 10);
                screen_set = e_MENU_SCREEN::ScrHome;
            }
            if (btn_input == e_BTN_EVENT::BtnPressedSelect) {
                screen_set = e_MENU_SCREEN::ScrHome;
            }
            break;
        default:
            printf("Screen update ERROR: unknown state\r\n");
            break;
    };

    //printf("temperature in C: %.2f\r\n", rtcTempC);

return;
}

void set_next_time(ds3231_time_t *p_target) {
    if (p_target->hours > 20){
        //after 9pm, next cycle is at 8:30am
        p_target->hours=8;
        p_target->minutes=30;
        p_target->seconds=00;
        return;
    }
    else if (p_target->hours > 13){
        //after 2pm, next cycle is at 9pm
        p_target->hours=21;
        p_target->minutes=00;
        p_target->seconds=00;
        return;
    }
    else if (p_target->hours > 7){
        //after Xam, next cycle is at 2:05pm
        p_target->hours=14;
        p_target->minutes=05;
        p_target->seconds=00;
        return;
    }
    return;
}