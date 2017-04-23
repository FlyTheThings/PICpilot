/*
 * File:   StateMachine.c
 * Author: Chris Hajduk
 *
 * Created on June 9, 2015, 9:00 PM
 */

#include "StateMachine.h"
#include "./Network/Datalink.h"
#include "./AttitudeManager.h"
#include "../Common/Utilities/Logger.h"
#include "../Common/Common.h"
#include "main.h"
#include "../Common/Interfaces/InterchipDMA.h"
#include "Drivers/Radio.h"
#include "ProgramStatus.h"
#include "../Common/Clock/Timer.h"
#include "../Common/Utilities/LED.h"

//State Machine Triggers (Mostly Timers)
static int dmaTimer = 0;
static int uplinkTimer = 0;
static int downlinkTimer = 0;
static int imuTimer = 0;
static int ledTimer = 0;
static long int stateMachineTimer = 0;
static int dTime = 0;

static char AMUpdate = 0;
static char flightUpdate = 0;

void StateMachine(char entryLocation){
    dTime = (int)(getTime() - stateMachineTimer);
    stateMachineTimer += dTime;
    uplinkTimer += dTime;
    downlinkTimer += dTime;
    imuTimer += dTime;
    ledTimer += dTime;
    dmaTimer += dTime;

    //Clear Watchdog timer
    asm("CLRWDT");
    //Feedback systems such as this autopilot are very sensitive to timing. In order to keep it consistent we should try to keep the timing between the calculation of error corrections and the output the same.
    //In other words, roll pitch and yaw control, mixing, and output should take place in the same step.
    if(AMUpdate){
        AMUpdate = 0;
        //Run - Angle control, and angular rate control
        flightUpdate = 1;
    }
    else if(IMU_UPDATE_FREQUENCY <= imuTimer && entryLocation != STATEMACHINE_IMU){
//        debug("IMU");
        imuTimer = 0;
        //Poll Sensor
        imuCommunication();

        flightUpdate = 1;
    }
    else if(newInterchipData() && checkDMA()){
        //Input from Controller
        flightUpdate = 1;
    }
    else{

    }

    if (entryLocation == STATEMACHINE_IDLE) {
        // If we're waiting to be armed, don't run the flight control
        flightUpdate = 0;
    }

    if (flightUpdate) {
        flightUpdate = 0;
        //Input from Controller
        inputCapture();
        highLevelControl();
        lowLevelControl();
    }

    if(UPLINK_CHECK_FREQUENCY <= uplinkTimer){
        uplinkTimer = 0;
        readDatalink();
    }

    if(downlinkTimer >= DOWNLINK_SEND_INTERVAL){
        downlinkTimer = 0;
        writeDatalink(getNextPacketType());
    }

    if (areGainsUpdated() || showGains()){
        queuePacketType(PACKET_TYPE_GAINS);
    }
    
    // Update status LED
    if (ledTimer >= LED_BLINK_LONG) {
        toggleLEDState();
        if (getProgramStatus() == UNARMED) {
            ledTimer -= LED_BLINK_LONG;
        } else if (getProgramStatus() == MAIN_EXECUTION) {
            ledTimer -= LED_BLINK_SHORT;
        }
    }
    
    parseDatalinkBuffer(); //read any incoming data from the Xbee and put in buffer
    sendQueuedDownlinkPacket(); //send any outgoing info
    
    asm("CLRWDT");
}

void forceStateMachineUpdate(){
    AMUpdate = 1;
}

void killPlane(char action){
    if (action){
        setProgramStatus(KILL_MODE);
    }
    else{
        setProgramStatus(MAIN_EXECUTION);
    }
}
