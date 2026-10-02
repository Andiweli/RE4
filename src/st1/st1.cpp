#include "types.h"
#include "room_data.h"
#include <dolphin/os.h>
#include "db_log.h"

// Stage 1 room module entry (D:/Bio4/Prog/st1.cpp; the same object ends every st1_* REL): registers the room
// Init/Main pairs of every stage-1 room in the DOL's St1_data_tbl, then the SN REL entry points (like st2.cpp).


// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

void R100Init();
void R100Main();
void R101Init();
void R101Main();
void R102Init();
void R102Main();
void R103Init();
void R103Main();
void R104Init();
void R104Main();
void R105Init();
void R105Main();
void R106Init();
void R106Main();
void R107Init();
void R107Main();
void R108Init();
void R108Main();
void R109Init();
void R109Main();
void R10aInit();
void R10aMain();
void R10bInit();
void R10bMain();
void R10cInit();
void R10cMain();
void R10dInit();
void R10dMain();
void R10eInit();
void R10eMain();
void R10fInit();
void R10fMain();
void R111Init();
void R111Main();
void R112Init();
void R112Main();
void R113Init();
void R113Main();
void R117Init();
void R117Main();
void R118Init();
void R118Main();
void R119Init();
void R119Main();
void R11aInit();
void R11aMain();
void R11bInit();
void R11bMain();
void R11cInit();
void R11cMain();
void R11dInit();
void R11dMain();
void R11eInit();
void R11eMain();
void R11fInit();
void R11fMain();
void R120Init();
void R120Main();

// Store one room's Init/Main pair into the DOL's St1_data_tbl (index = room number & 0xFF).
void set(int no, void (*init)(), void (*main)())
{
    St1_data_tbl[no].pInit = init;
    St1_data_tbl[no].pMain = main;
}

// Fill the stage table with every room this module compiles (missing indices are rooms that do not exist).
void setTbl()
{
    set(0, R100Init, R100Main);
    set(1, R101Init, R101Main);
    set(2, R102Init, R102Main);
    set(3, R103Init, R103Main);
    set(4, R104Init, R104Main);
    set(5, R105Init, R105Main);
    set(6, R106Init, R106Main);
    set(7, R107Init, R107Main);
    set(8, R108Init, R108Main);
    set(9, R109Init, R109Main);
    set(10, R10aInit, R10aMain);
    set(11, R10bInit, R10bMain);
    set(12, R10cInit, R10cMain);
    set(13, R10dInit, R10dMain);
    set(14, R10eInit, R10eMain);
    set(15, R10fInit, R10fMain);
    set(17, R111Init, R111Main);
    set(18, R112Init, R112Main);
    set(19, R113Init, R113Main);
    set(23, R117Init, R117Main);
    set(24, R118Init, R118Main);
    set(25, R119Init, R119Main);
    set(26, R11aInit, R11aMain);
    set(27, R11bInit, R11bMain);
    set(28, R11cInit, R11cMain);
    set(29, R11dInit, R11dMain);
    set(30, R11eInit, R11eMain);
    set(31, R11fInit, R11fMain);
    set(32, R120Init, R120Main);
}

// REL entry point (called by the loader after linking): run the static constructors, then register the rooms.
extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    setTbl();
    OSReport("prolog...\n");
}

// REL exit point (before unlinking): run the static destructors.
extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

// Stub the loader binds unresolved imports to: reports and halts (the HALT line number is baked in).
extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 144 "D:/Bio4/Prog/st1.cpp"
    HALT();
}
