#ifndef __MAIN_TYPES_H__
#define __MAIN_TYPES_H__

enum e_SUIJIN_STATE {
    WaitingForNextCycle = 0,
    InitSetup,
    RunningPump_A,
    Pause_A,
    RunningPump_B,
    Pause_B,
    Appendix
};

enum e_BTN_EVENT {
    BtnNone = 0,
    BtnPressedSelect,
    BtnPressedEnter,
    BtnPressedBoth
};

enum e_EVENT {
    EventNone = 0,
    EventTriggerWattering,
    EventOverheating,
    EventStopCmd
};

enum e_MENU_SCREEN {
    ScrHome = 0,
    ScrManualTrigger
};

#define MOTOR_ENABLE 1
#define MOTOR_DISABLE 0

#endif