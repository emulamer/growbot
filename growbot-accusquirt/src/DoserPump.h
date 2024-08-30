#include <Arduino.h>
#include "FastAccelStepper.h"
///#include <BasicStepperDriver.h>

/**** Doser pump ****
 * Provides a functions for interacting with a single stepper-based doser pump.
 * Handles "retraction" where the pump will run backwards after dispensing to keep the liquid being dosed
 *  from the end of the output tube and help prevent dripping while automatically adjusting for the 
 *  retracted liquid volume when it next dispenses
 * 
 * /

/** DoserPump is base "interface" class extended by specific implementations for a stepper driver
 * DoserPumpA4988 is an implementation of DoserPump for the A4988 stepper driver
 * 
 * 
 * 
 */
#pragma once

bool engineInitted = false;
FastAccelStepperEngine engine = FastAccelStepperEngine();

#define DEFAULT_STEPS_PER_ML 4790

#define DEFAULT_RETRACTION_ML 1.5

#define DEFAULT_MAX_RPM 140

#define TUBE_PURGE_STEPS 150000 
//150000

#define CALIB_MEDIUM_DIVISOR 2
#define CALIB_SLOW_DIVISOR 3
#define CALIB_VERY_SLOW_DIVISOR 6

#define NUMBER_OF_STEPS_PER_ROTATION 200


//callback definition for when dosing or continuous pumping ends
//  doserId: the ID of the doser provided when constructing the object
//  aborted: true if the dosing ended because it was aborted
//  stepsDosed: the total number of steps that were taken on the doser stepper.  Note: with continuous pumping, this won't be accurate if the total number of steps goes over max value of a long
//  mlDosed: the total number of mililiters that were dosed.  Note: with continuous pumping, this won't be accurate if the total number of steps goes over max value of a long
//  isRetracted: true if the pump is in a retracted state
//  wasContinuousPump: true if the dose end was a continuous pump

//dose types will be:
//  1: dose
//  2: continuous pump
//  3: retract
typedef void (*DoseEndCallback)(int doserId, int doseType, bool aborted, long stepsDosed, float mlDosed);

typedef void (*DoseCalibrateEndCallback)(int doserId, bool success, long bottle2SensorSteps, long sensor2DoseSteps, int errorCode, String errorMessage);

class DoserPump {
    protected:
        int doserId = 0;
        //bool isDosing = false;
        int dosingStage = 0;
        int calibrationStage = 0;
        long calibBottle2Sensor = 0;
        long calibSensor2Dose = 0;
        bool isContinuousPumping = false;
        bool isRetracting = false;
       // bool retracted = true;
        //bool retractionEnabled = true;
        unsigned long stepsPermL = DEFAULT_STEPS_PER_ML;
        //float retractionmL = DEFAULT_RETRACTION_ML;
        float maxRpm = DEFAULT_MAX_RPM;
        DoseEndCallback doseEndCallback;
    public:
        DoserPump(int doserId) {
            this->doserId = doserId;
        }
        DoserPump(int doserId, unsigned long stepsPerMl, float maxRpm) {
            this->doserId = doserId;
            this->stepsPermL = stepsPerMl;
            this->maxRpm = maxRpm;
        }
        virtual void init() {

        }
    //gets the calibration as the number of steps the motor takes for 1 mL of output
    unsigned long getCalibration() {
        return stepsPermL;
    }

    // //gets the number of mililiters that the pump retracts after dosing
    // float getRetractionMl() {
    //     return retractionmL;
    // }

    //gets a value indicating whether the pump turns backwards after dosing to retract the dosing liquid
    // bool getRetractionEnabled() {
    //     return retractionEnabled;
    // }

    // //gets a value indicating whether the liquid is currently in a retracted state
    

    //gets the max RPM
    float getMaxRpm() {
        return maxRpm;
    }

    //gets the ID of the doser provided in the constructor
    int getDoserId() {
        return doserId;
    }


    // //when dosing is in progress, gets the mL pumped so far
    // virtual float getDoseInProgressMl() = 0;

    // //when dosing is in progress, get the number of steps so far
    // virtual long getDoseInProgressSteps() = 0;

    // //when dosing is in progress, gets the percentage complete of the dosing operation
    // virtual float getDoseInProgressPercent() = 0;

    //returns true if the pump is currently dosing or pumping continuously
    virtual bool isPumping() {
        return dosingStage > 0 || isRetracting || isContinuousPumping || calibrationStage > 0;
    }

    //sets the calibration as the number of steps the motor takes for 1 mL of output.
    virtual void setCalibration(long bottleToSensorCalib, long sensorToDoseCalib) {
        this->calibBottle2Sensor = bottleToSensorCalib;
        this->calibSensor2Dose = sensorToDoseCalib;
    }

    // //sets the number of mililiters that the pump retracts after dosing
    // virtual void setRetractionMl(float retractionMl) {
    //     this->retractionmL = retractionMl;
    // }

    // //sets whether the pump turns backwards after dosing to retract the dosing liquid and whether the liquid is currently in a retracted state
    // virtual void setRetractionEnabled(bool enableRetraction, bool isRetracted) {
    //     this->retractionEnabled = retractionEnabled;
    //     this->retracted = isRetracted;
    // }

    //sets the callback called when dosing ends either due to completion or due to being aborted
    //it is passed a flag indicating if it was aborted along with the total number of steps dosed and the total ML dosed
    void setDoseEndCallback(DoseEndCallback callback) {
        this->doseEndCallback = callback;
    }

    //set the maximum speed in RPM that the pump will turn
    virtual void setMaxRPM(float maxRpm) {
        this->maxRpm = maxRpm;
    }

    //starts pumping continuously at the max RPM until stop() is called, typically for priming the pumps
    //if skipRetract is false and retraction is enabled, the pump will retract retractionmL after being stopped
    virtual bool startContinuousPump() = 0;

    //should be called in the main loop()
    virtual void update() = 0;

    //immediately stop the doser and abort any dosing in progress.  returns true if there was dosing in progress, false if there was not.
    virtual bool stop() = 0;

    //begin dosing the specified number of mL.  returns true if dosing was started, returns false if dosing did not start (e.g. if another dosing is in progress)
    virtual bool startDosing(float doseml) = 0;

    //blocking call.  Immediately doses the number of steps, does no retraction or unretraction.
    //if retraction is enabled and provided a positive number of steps, sets the retraction state to not retracted.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
    //virtual bool doseStepsImmediate(long steps) = 0;

    //blocking call.  If not retracted, immediately retracts to the set number of ml.
    //if retraction is disabled or already in a retracted state, returns false.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
    virtual bool retract() = 0;

    //blocking call.  If retracted, immediately un-retracts to the set number of ml.
    //if retraction is disabled or already not in a retracted state, returns false.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
   // virtual bool unretractImmediate() = 0;
    virtual bool startCalibrate(int startStep, DoseCalibrateEndCallback callback) = 0;

};

//implementation of DoserPump for an A4988 stepper driver
class DoserPumpA4988 : public DoserPump {
    private:
        //BasicStepperDriver *stepper;
        FastAccelStepper *stepperF;
        long actualLastDosedSteps = 0;
        long totalDoseSteps = 0;
        long dirInversion = 1;
        //sensor related stuff 
        int sensorPin = -1;
        int onSensorInterruptAction = 0;
        unsigned long onSensorDetectDelay = 0;
        static void IRAM_ATTR sensorChange(void* ref);
        bool sensorChangedSinceLast = false;
        bool sensorState = false;
        long sensorInterruptStepCount = 0;
        

        DoseCalibrateEndCallback calibrationCallback = 0;
        void setRpmF(float rpm) {
            if (stepperF) {          
                int32_t hz =      (rpm / 60.0) * NUMBER_OF_STEPS_PER_ROTATION;
                hz *= 16; //microsteps
                dbg.printf("Setting speed to %d\n", hz);
                stepperF->setSpeedInHz(hz );
            }
        }
        void doMove(long steps, bool block = false) {
            if (stepperF) {
                stepperF->setAcceleration(50000);
                stepperF->move(0, true);
                stepperF->setCurrentPosition(0);
                stepperF->moveTo(dirInversion * steps, block);
            } else {
                dbg.println("stepper not initialized");
            }
        }
        void slowMove(long steps, bool block = false) {
            if (stepperF) {
                stepperF->setAcceleration(1000);
                stepperF->setCurrentPosition(0);
                stepperF->moveTo(dirInversion * steps, block);
            } else {
                dbg.println("stepper not initialized");
            }
        }
        void easyMove(long steps, bool block = false) {
            if (stepperF) {
                stepperF->setAcceleration(10000);
                stepperF->setCurrentPosition(0);
                stepperF->moveTo(dirInversion * steps, block);
            } else {
                dbg.println("stepper not initialized");
            }
        }
        void disableStepper() {
            if (stepperF) {
                stepperF->disableOutputs();
            }
        }
        void enableStepper() {
            if (stepperF) {
                stepperF->enableOutputs();
            }
        }
        void stopStepper(bool easy = false) {
            if (stepperF) {
                stepperF->setAcceleration(easy?10000:50000);
                stepperF->stopMove();
                int ctr = 0;
                while (stepperF->isStopping() && ctr < 100) {
                    delay(1);
                }
                if (ctr >= 100) {
                    dbg.println("Failed to stop stepper");
                    stepperF->forceStop();
                }
            }
        }
        void initStepper(int dirPin, int stepPin, int sleepPin) {
            if (!engineInitted) {
                engine.init();
                engineInitted = true;
            }
            stepperF = engine.stepperConnectToPin(stepPin);
            if (!stepperF) {
                dbg.printf("Failed to connect stepper to pin %d\n", stepPin);
            } else {
                stepperF->setDirectionPin(dirPin);
                stepperF->setEnablePin(sleepPin, false);
                stepperF->setAutoEnable(true);
                stepperF->setDelayToDisable(5000);
                //stepperF->disableOutputs();
                
                
                //stepperF->setSpeedInHz(100);
                setRpmF(maxRpm);
                stepperF->setAcceleration(50000);
                

            }

            // this->stepper = new BasicStepperDriver(NUMBER_OF_STEPS_PER_ROTATION, dirPin, stepPin, sleepPin);
            // this->stepper->begin(this->maxRpm, 16);
            // this->stepper->setSpeedProfile(BasicStepperDriver::CONSTANT_SPEED, 1000, 1000);
            // this->stepper->setEnableActiveState(HIGH);
            // dbg.printf("stepper disable!");
            // this->stepper->disable();
        }

            int32_t getSteps() {
                return dirInversion * stepperF->getCurrentPosition();
            }

        // long adjustedStepsCompleted() {
        //     long totalSteps = stepper->getStepsCompleted() - calibSensor2Dose;
        //     if (totalSteps < 0) {
        //         totalSteps = 0;
        //     }
        //     return totalSteps;
        // }
        // void invokeDoneCallback(bool wasContinuousPump) {
        //     if (this->doseEndCallback != NULL) {
                
        //         this->doseEndCallback(doserId, isAbort, doseEndStepCount, (float)doseEndStepCount/stepsPermL, wasContinuousPump);
        //     }
        // }

        // void retractDone() {
        //     bool isCont = isContinuousPumping;
        //     isRetracting = false;
        //     isDosing = false;
        //     isContinuousPumping = false;
        //     retracted = true;
        //     stepper->disable();
        //     invokeDoneCallback(isCont);
        // }

        // void moveDone(bool skipRetract = false) {
        //     if (retractionEnabled && !skipRetract) {
        //         isRetracting = true;
        //         doseEndStepCount = adjustedStepsCompleted();
        //         stepper->startMove(-(retractionmL * stepsPermL));
        //     } else {
        //         bool isCont = isContinuousPumping;
        //         isDosing = false;
        //         isRetracting = false;
        //         isContinuousPumping = false;
        //         doseEndStepCount = adjustedStepsCompleted();
        //         stepper->disable();
        //         invokeDoneCallback(isCont);
        //     }
        // }



        // void resetForDose() {
        //     isAbort = false;
        //     isContinuousPumping = false;
        //     //isDosing = false;
        //     isRetracting = false;
        //     doseEndStepCount = 0;
        //     totalDoseSteps = 0;
        // }
    public:
        DoserPumpA4988(int doserId, int dirPin, int stepPin, int sleepPin, bool reverseDirection) : DoserPump(doserId) {
            initStepper(dirPin, stepPin, sleepPin);
            if (reverseDirection) {
                dirInversion = -1;
            } else {
                dirInversion = 1;
            }
        }
        //doserId: an arbitrary identifier for this doser, useful for sharing a callback between multiple instances
        //dirPin: the pin that is wired to the direction pin on the A4988
        //stepPin: the pin that is wired to the step pin on the A4988
        //sleepPin: the pin that is wired to the sleep+reset pins on the A4988
        //stepsPerMl: how many individual steps it takes to dispense exactly 1 ml of liquid
        //maxRpm: the maximum RPM that the pump head can turn
        //retractionEnabled: true to enable retraction functionality to reverse the pump after dispensing to keep the liquid from dripping
        //retractionML: the number of mililiters to retract the fluid after dispensing
        //isRetracted: the initial state of whether it is starting with fluid retracted
        DoserPumpA4988(int doserId, int dirPin, int stepPin, int sleepPin, int sensorPin, unsigned long stepsPerMl, float maxRpm, bool reverseDirection, DoseEndCallback endCallback) : DoserPump(doserId, stepsPerMl, maxRpm) {
            setDoseEndCallback(endCallback);
            initStepper(dirPin, stepPin, sleepPin);     
            this->sensorPin = sensorPin;
            if (reverseDirection) {
                dirInversion = -1;
            } else {
                dirInversion = 1;
            }
        }

        void init() {
            DoserPump::init();
            if (sensorPin > -1) {
                dbg.printf("Setting up sensor pin %d\n", sensorPin);
                pinMode(sensorPin, INPUT);
                sensorState = !digitalRead(sensorPin);
                lastSensorState = sensorState;
                sensorChangedSinceLast = false;
                //attachInterruptArg(this->sensorPin, DoserPumpA4988::sensorChange, this, FALLING);
                //attachInterruptArg(this->sensorPin, DoserPumpA4988::sensorChange, this, RISING);
            }            
        }
        bool isRetracted() {
            return sensorState;
        }

/*
    Calibrate will first reverse the pump for a bit to make sure the line is empty
    Then it will run the pump relatively slowly until the sensor pin triggers, counting steps
    Then it will reverse the pump all the way to make sure it is empty again.
    Then it will run the pump quick most of the way, then go extra slowly until the sensor pin triggers again.
    (maybe repeat that stuff a couple times?)  maybe do a test run here to make sure it's where it was expected
    it'll store the number of steps (or mL maybe) that it took to get the liquid up to the sensor. this will be used to go fast 3/4 of the way to the sensor then slow down.  also for how far to retratct


    then, get liquid up to sensor point
    run the pump forward for a while to assure it's being dosed
    then stop completely
    reverse slowly until sensor turns off.
    repeat this a few times to get a solid value.
    This is the amount of steps it will take from the sensor turning on until it is actually being dosed.
    

    with the bottle -> sensor value and the sensor -> dose values, the dosing will be

    fast fwd to (3/4?) bottle->sesor
        (error handle, if it trips early, or if it doesn't trip within... 10%? of the value, go to fail state)
    slow fwd until sensor fires

    fast fwd to (sensor->dose) + intended ml minus a little
    slow fwd, slow down dose at end
    retract (bottle->sensor) + (sensor->dose) + 20% or so



*/

       
    //0 is none, 1 is stop, 2 is store step value (or maybe reset it?)

        /*
            dose stage
            dose , 1: fast fwd out to the calib point (enable sensor)
            step 2: slow fwd until sensor triggers (enable sensor)
            step 3: take the total dose steps, add in the calib sensor to dose steps, and run the pump forward that many steps
            step 4: if sensor is triggered, stop immediately, take the number of steps that completed minus the calib sensor to dose steps, report aborted with that steps/ml
            step 5: retract first sensor /4 * 5 + the sensor to dose steps + probably 20%

        */
        long getPurgeSteps() {
            if (calibBottle2Sensor == 0 || calibSensor2Dose == 0) {
                return -TUBE_PURGE_STEPS;
            } else {
                return -(long)((double)((calibBottle2Sensor / 4) * 5 + calibSensor2Dose) * (double)1.5);
            }
        }

        void abortDosing(int errCode, String msg, long lastStepsDosed = 0, float actualMlDosed = 0) {
            stopStepper();
            //stepper->stop();
            //stepper->disable();
            onSensorInterruptAction = 0;
            dosingStage = 0;            
            totalDoseSteps = 0;
            dbg.printf("Aborting dosing, error code: %d, message: %s\n", errCode, msg.c_str());
            doseEndCallback(doserId, 1, true, lastStepsDosed, actualMlDosed);
            //retract on dose fail
            retract();
            
        }
//takes ~500ms for sensor to update
#define STABLE_SENSOR_TIME_WAIT 650
#define ADJ_STEP_AMT 64
void watchdogHack() {
            //this is shitty code to put this here instead of making it non blocking
        #ifdef ARDUINO_ARCH_ESP8266
            ESP.wdtFeed();
        #elif defined(ARDUINO_ARCH_ESP32)
            esp_task_wdt_reset();
        #endif
}
    bool isSensorStable(int loopCount = 1, bool forceValue = false, bool desiredValue = false) {
        bool startingState = sensorState;
        unsigned long endStamp = millis() + (STABLE_SENSOR_TIME_WAIT * loopCount);
        sensorState = !digitalRead(sensorPin);
        while ( millis() < endStamp) {
            sensorState = !digitalRead(sensorPin);
            
            lastSensorState = sensorState;
            if (forceValue && sensorState != desiredValue) {
                return false;
            }
            if (sensorState != startingState) {
                return false;
            }
            watchdogHack();
            delay(1);
        }
        return sensorState == startingState;
    }
    #define STABILIZE_MAX_WAIT_BLOCK_MS 15000



    bool stabilizeAlign() {
        dbg.println("Stabilizing alignment off point");
        if (!stabilizeOff()) {
            return false;
        }

watchdogHack();

        dbg.println("Stabilizing alignment on point");
        if (!stabilizeOn()) {
            return false;
        }
        return true;
    }
bool stabilizeOn() {
    int lastSensorMode = onSensorInterruptAction;
    onSensorInterruptAction = 0;
    unsigned long endStamp = millis() + STABILIZE_MAX_WAIT_BLOCK_MS;
    while (!isSensorStable(6, true, true) && millis() < endStamp) {
        slowMove(ADJ_STEP_AMT, true);
    }
    lastSensorState = sensorState;
    onSensorInterruptAction = lastSensorMode;
    bool res =  !(millis() > endStamp);        
    dbg.printf("Stabilize on result: %d\n", res);
    return res;
}

bool stabilizeOff() {
    int lastSensorMode = onSensorInterruptAction;
    onSensorInterruptAction = 0;
    unsigned long endStamp = millis() + STABILIZE_MAX_WAIT_BLOCK_MS;
    while (!isSensorStable(6, true, false) && millis() < endStamp) {
        slowMove(-ADJ_STEP_AMT, true);
    }
    lastSensorState = sensorState;
    onSensorInterruptAction = lastSensorMode;
    bool res =  !(millis() > endStamp);        
    dbg.printf("Stabilize off result: %d\n", res);
    return res;
}
        bool retract() {
            if (isPumping()) {
                return false;
            }
            isRetracting = true;
            onSensorInterruptAction = 0;
            //stepper->setRPM(maxRpm);
            setRpmF(maxRpm);
            //stepper->enable();
            //stepperF->en
            doMove(getPurgeSteps());
            //stepper->startMove(getPurgeSteps());
            return true;
        }

        void doseNextStep(byte triggeredBy) {
            //triggeredBy: 0 = user?,  1 = move finished, 2 = sensor changed state
            dosingStage++;
            if (dosingStage == 1) {
                //there's stuff in the tube, purge it.
                if (sensorState) {
                    onSensorInterruptAction = 0;
                    //after move is done here, do all the completion stuff
                    dbg.println("There's fluid in the line, purging it.");
                    setRpmF(maxRpm);
                    //stepper->setRPM(maxRpm);
                    //stepper->enable();
                    //stepper->startMove(getPurgeSteps());
                    slowMove(getPurgeSteps());
                } else {
                    dosingStage++;
                }
            }
            
            if (dosingStage == 2) {
                if (sensorState) {
                    dbg.println("Sensor is already indicating liquid, can't dose.");
                    abortDosing(1, "Liquid in tube when not expected.");
                    return;
                }
                onSensorInterruptAction = 1;
                //first step, purge the line backwards, fill it with air
                dbg.printf("Dose stage 1: Fast forward to calib point of %d\n", calibBottle2Sensor);
                //stepper->setRPM(maxRpm);
                setRpmF(maxRpm);
                //stepper->enable();
                //stepper->startMove(calibBottle2Sensor);
                doMove(calibBottle2Sensor);
            } else if (dosingStage == 3) {
                if (triggeredBy == 1) {
                    int32_t completedSteps = getSteps();//stepperF->getCurrentPosition();
                    dbg.printf("Starting slow dose alignment, step count pref of %d, next of %d\n", completedSteps, calibBottle2Sensor);
                    onSensorInterruptAction = 1;
                    setRpmF(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->setRPM(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->enable();
                    //stepper->startMove(calibBottle2Sensor);
                    doMove(calibBottle2Sensor);
                } else if (triggeredBy == 2) {
                    
                    abortDosing(1, "Sensor toggled in an unexpected way.");
                    return;
                }
                
            } else if (dosingStage == 4) {
                onSensorInterruptAction = 1;
                if (triggeredBy == 1) {
                    int32_t completedSteps = getSteps();//stepperF->getCurrentPosition();//stepper->getStepsCompleted();
                    dbg.printf("Slow dose failed, sensor didn't trip after %d\n", completedSteps);
                    abortDosing(1, "Sensor didn't trip on slow align stage 3");
                } else if (triggeredBy == 2) {
                    if (!stabilizeAlign()) {
                        abortDosing(2, "Sensor didn't stabilize.");
                        return;
                    }
                    if (!sensorState) {
                        //sensor triggered early, abort
                        abortDosing(1, "Sensor state is bad, shows empty");
                        return;
                    }
                    dbg.printf("Aligned for dosing, running forward %d steps plus sensor to dose calib of %d\n", totalDoseSteps, calibSensor2Dose);
                    onSensorInterruptAction = 1;
                    onSensorDetectDelay = millis() + 1000;
                    setRpmF(maxRpm);
                    //stepper->setRPM(maxRpm);
                    // stepper->enable();
                    // stepper->startMove(totalDoseSteps+calibSensor2Dose);
                    easyMove(totalDoseSteps+calibSensor2Dose);
                }
            } else if (dosingStage == 5) {
                onSensorInterruptAction = 0;
                if (triggeredBy == 1) {                    
                    long completedSteps = getSteps();//stepperF->getCurrentPosition();//stepper->getStepsCompleted();
                    dbg.printf("step 4 raw stepper completed steps %d\n", completedSteps);
                    completedSteps = completedSteps - calibSensor2Dose;
                    dbg.printf("step 4 adjusted completed steps %d\n", completedSteps);
                    if (completedSteps < 0) {
                        completedSteps = 0;
                    } 
                    actualLastDosedSteps = completedSteps;
                    float dosed = (float)((double)(completedSteps) / (double)stepsPermL);
                    dbg.printf("Sensor didn't trip on stage 4, dosing completed all the way successfully, %d steps %f ml dosed\n", completedSteps, dosed);
                    //disable the interrupt and reverse purge it all back out plus some padding
                    onSensorInterruptAction = 0;
                    //after move is done here, do all the completion stuff
                    dbg.println("Dose done, purging line.");
                    setRpmF(maxRpm);
                    //stepper->setRPM(maxRpm);
                    //stepper->enable();
                    //stepper->startMove(getPurgeSteps());
                    doMove(getPurgeSteps());
                    
                } else if (triggeredBy == 2) {
                    //if (!sensorState) { //does it matter?
                    long completedSteps = sensorInterruptStepCount - calibSensor2Dose;
                    if (completedSteps < 0) {
                        completedSteps = 0;
                    } 
                    float dosed = (float)((double)(completedSteps) / (double)stepsPermL);
                    actualLastDosedSteps = completedSteps;
                    if (dosed < 0) {
                        dosed = 0;
                    }
                    dbg.printf("Sensor tripped on stage 4 with state %d, %f already dosed\n", sensorState, dosed);
                    if (!sensorState) {
                      abortDosing(1, "Fluid sensor is reporting empty before dosing is complete!", actualLastDosedSteps, dosed);  
                    } else {
                      abortDosing(1, "Fluid sensor changed state during dosing before complete!", actualLastDosedSteps, dosed);  
                    }
                    return;
                }
            } else if (dosingStage == 6) {
                dosingStage = 0;
                float dosed = (float)((double)(actualLastDosedSteps) / (double)stepsPermL);
                if (dosed < 0) {
                    dosed = 0;
                }
               
                //stepper->disable();
                doseEndCallback(doserId, 1, true, actualLastDosedSteps, dosed);
            }
        }

        bool startCalibrate(int startStep, DoseCalibrateEndCallback callback) {
            if (isPumping()) {
                dbg.println("Pump is already pumping, cannot calibrate");
                return false;
            }
            this->calibrationCallback = callback;
            calibrationStage = startStep;
            calibrateNextStep(0);
            return true;
        }

        //step 1: reverse purge tube
        //step 2: run pump semi slowly until sensor triggers
        //      store to an intermediate value "tempb2s"
        //step 3: reverse pump full purge steps full speed
        //step 4: run pump fast until 3/4 of tempb2s
        //step 5: run pump VERY slow until sensor triggers
        //step 6: back up pump VERY slow until sensor turns off
        //step 7: run pump back VERY slow until sensor triggers 
        //               store the total number of steps since step 4 to a hopefully valid "bottleToSensorCalib"
        //step 8: run pump fast forward another bottleToSensorCalib x3, make the assumption that the tube is not more than 3x assymetrical length, count steps to "tmpOverflowSteps"
        //step 9: run pump back VERY slow until sensor turns off, count steps to "tmps2d"
        //step 10: run pump forward VERY slow until sensor triggers
        //step 11: run pump forward slow for tmps2d+10% steps
        //step 12: run pump backward VERY slow until sensor turns off, make sure steps were about tmps2d and store steps since 11 to "sensorToDoseCalib"
        //step 13: run pump backward fast for (sensorToDoseCalib + bottleToSensorCalib) + 20% to purge it

        //the first to-sensor calib will just be used to know roughly when to slow down for the sensor accurately, the sensor to dose calib will be very important
        

                //          store to an intermediate value "temps2d"


//i don't even need the fine adjust really


//error codes, how about:
// 0: unknown
// 1: sensor didn't trip when it should have
// 2: too high of variance

        void abortCalibration(int errCode, String msg) {
            onSensorInterruptAction = 0;
            calibrationStage = 0;
            //stepper->stop();
            stopStepper();
            dbg.printf("Calibration aborted, error code: %d, message: %s\n", errCode, msg.c_str());
            //stepper->disable();
            if (calibrationCallback != NULL) {
                calibrationCallback(doserId, false, 0, 0, errCode, msg);
                calibrationCallback = NULL;
            }
        }

int calibFineTuneLoops = 0;
int calibFineTuneStep = 0;
bool calibFineTuneFwd = false;

    void setCalibration(long bottleToSensorCalib, long sensorToDoseCalib) {
        this->calibBottle2Sensor = bottleToSensorCalib;
        this->calibSensor2Dose = sensorToDoseCalib;
    }



        //need to know if the sensor tripped or what
        void calibrateNextStep(byte triggeredBy) {
            //triggeredBy: 0 = user?,  1 = move finished, 2 = sensor changed state
            calibrationStage++;
            if (calibrationStage == 1) {
                
                //first step, purge the line backwards, fill it with air
                dbg.println("Calibration step 1: Purging tube");
                setRpmF(maxRpm);
                //stepper->setRPM(maxRpm);
                //stepper->enable();
                //stepper->startMove(-TUBE_PURGE_STEPS);
                doMove(-TUBE_PURGE_STEPS);
                dbg.printf("Calibration step 1: started the stepper on startMove at rpm %f\n", maxRpm);
            } else if (calibrationStage == 2) {
                // move slower forward, aim will be for sensor to catch it with the sensor
                dbg.println("Calibration step 2: Medium forward until sensor triggers");
                //set the interrupt to stop the pump immediately when triggered
                onSensorInterruptAction = 1;
                //stepper->setRPM(maxRpm/CALIB_MEDIUM_DIVISOR);
                setRpmF(maxRpm/CALIB_MEDIUM_DIVISOR);
                //stepper->enable();
                //stepper->startMove(TUBE_PURGE_STEPS);
                doMove(TUBE_PURGE_STEPS);
            } else if (calibrationStage == 3) {
                //see if triggered by mode finished or sensor changed state
                dbg.printf("Calibration step 3: triggered by %d\n", triggeredBy);
                if (triggeredBy == 1) {
                    dbg.println("Ttriggered at step 3 by step count, no sensor");
                    //ERROR STATE- didn't detect liquid but should have, abort
                    abortCalibration(1, "Sensor detected no liquid when it should have.  Check intake tube is immersed.");
                    return;
                } else if (triggeredBy == 2) {
                    onSensorInterruptAction = 0;
                    dbg.printf("Sensor triggered at step 3 with step count %d, purge tube\n", this->sensorInterruptStepCount);
                    //calibrationTempB2S = this->sensorInterruptStepCount;
                    calibBottle2Sensor = (int)((double)sensorInterruptStepCount * (double)0.8);
                    dbg.printf("stored bottle calib step count is %l\n", calibBottle2Sensor);
                    setRpmF(maxRpm);
                    //stepper->setRPM(maxRpm);
                    //stepper->enable();
                    //stepper->startMove(-TUBE_PURGE_STEPS);
                    doMove(-TUBE_PURGE_STEPS);
                } else {
                    //uh, should have been triggered by one of the above
                    abortCalibration(0, "Unknown error");
                    return;
                }
            } else if (calibrationStage == 4) {
                dbg.println("Step 4, fast load from bottle");
                //stepper->setRPM(maxRpm);
                setRpmF(maxRpm);
                //stepper->enable();
                //stepper->startMove(calibBottle2Sensor);
                //stepperF->setCurrentPosition(0);
                slowMove(calibBottle2Sensor);
                //stepperF->move(calibBottle2Sensor);
            } else if (calibrationStage == 5) {
                dbg.println("Step 5, slow load from bottle to sensor...");
                onSensorInterruptAction = 1;
                //stepper->setRPM(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                setRpmF(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                //stepper->enable();
                //stepper->startMove(calibBottle2Sensor);                
                //stepperF->setCurrentPosition(0);
                //stepperF->move(calibBottle2Sensor);
                doMove(calibBottle2Sensor);
            } else if (calibrationStage == 6) {
                onSensorInterruptAction = 0;
                if (triggeredBy == 1) {
                    dbg.println("Sensor didn't trip when expected");
                    abortCalibration(1, "Sensor didn't trigger when it should have on step 6");
                } else if (triggeredBy == 2) {
                    dbg.printf("Set up, did it with stepcount %d\n", sensorInterruptStepCount + calibBottle2Sensor);
                    //this is where it gets messy, just fast flood it out the end for a lot
                    if (!stabilizeAlign()) {
                        abortCalibration(2, "Sensor didn't stabilize.");
                        return;
                    }
                    if (!sensorState) {
                        //sensor triggered early, abort
                        abortCalibration(1, "Test dose Sensor state is bad, shows empty");
                        return;
                    }
                    setRpmF(maxRpm);
                    //stepper->enable();
                    //stepper->startMove(TUBE_PURGE_STEPS);
                    stepperF->setCurrentPosition(0);
                    easyMove(TUBE_PURGE_STEPS);

                }
            } else if (calibrationStage == 7) {
                if (triggeredBy == 1) {
                    onSensorInterruptAction = 1;
                    setRpmF(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->setRPM(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->enable();
                    //stepper->startMove(-TUBE_PURGE_STEPS);
                    // stepperF->setCurrentPosition(0);
                    // stepperF->move(-TUBE_PURGE_STEPS);
                    doMove(-TUBE_PURGE_STEPS);
                    
                } else if (triggeredBy == 2) {
                    dbg.println("Sensor ran out of juice on step 7?");
                    abortCalibration(1, "Sensor triggered on step 7.  Out of juice?");
                }
            } else if (calibrationStage == 8) {
                onSensorInterruptAction = 0;
                if (triggeredBy == 1) {
                    dbg.println("Sensor never triggered on stage 8");
                    abortCalibration(1, "Sensor did not trigger on stage 8.");                    
                } else if (triggeredBy == 2) {
                    dbg.printf("there are %d steps from sensor to dose\n", sensorInterruptStepCount);
                    calibSensor2Dose = -sensorInterruptStepCount;
                    dbg.println("Calibration almost done, double checking measure.  Starting with purge line.");
                    setRpmF(maxRpm);
                    doMove(getPurgeSteps());
                }
            } else if (calibrationStage == 9) {
                onSensorInterruptAction = 1;
                dbg.println("Line purged, test dosing zero");
                setRpmF(maxRpm);
                //stepper->enable();
                //stepper->startMove(calibBottle2Sensor);
                doMove(calibBottle2Sensor);
            } else if (calibrationStage == 10) {
                calibFineTuneLoops = 0;
                calibFineTuneStep = 0;
                 if (triggeredBy == 1) {
                    int32_t completedSteps = getSteps();//stepperF->getCurrentPosition();
                    dbg.printf("Calib starting test slow dose alignment, step count pref of %d, next of %d\n", completedSteps, calibBottle2Sensor);
                    onSensorInterruptAction = 1;
                    setRpmF(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->setRPM(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->enable();
                    //stepper->startMove(calibBottle2Sensor);
                    doMove(calibBottle2Sensor);
                } else if (triggeredBy == 2) {
                    
                    abortCalibration(1, "Test dose Sensor toggled in an unexpected way.");
                    return;
                }
            } else if (calibrationStage == 11) {
                onSensorInterruptAction = 1;
                if (triggeredBy == 1) {
                    int32_t completedSteps = getSteps();//stepperF->getCurrentPosition();//stepper->getStepsCompleted();
                    dbg.printf("Calib test slow dose failed, sensor didn't trip after %d\n", completedSteps);
                    abortCalibration(1, "Sensor didn't trip on slow align test dose stage 11");
                } else if (triggeredBy == 2) {
                    if (!stabilizeAlign()) {
                        abortCalibration(2, "Sensor didn't stabilize.");
                        return;
                    }
                    if (!sensorState) {
                        //sensor triggered early, abort
                        abortCalibration(1, "Test dose Sensor state is bad, shows empty");
                        return;
                    }
                    dbg.printf("Aligned for test dosing, running forward 0 steps plus sensor to dose calib of %d\n", calibSensor2Dose);
                    onSensorInterruptAction = 1;
                    onSensorDetectDelay = millis() + 1000;
                    setRpmF(maxRpm);
                    //stepper->setRPM(maxRpm);
                    // stepper->enable();
                    // stepper->startMove(totalDoseSteps+calibSensor2Dose);
                    easyMove(calibSensor2Dose);
                }
            } else if (calibrationStage == 12) {
                onSensorInterruptAction = 0;
                if (triggeredBy == 1) {                    
                    
                    dbg.printf("calib dose adjusted completed, no trip steps %d\n");
                    
                    
                    calibFineTuneLoops = 0;
                    //now turn off the interrupt and pump it back the exact number of steps and see if it trips.  
                    onSensorInterruptAction = 0;
                    //after move is done here, do all the completion stuff
                    dbg.println("test dose deployed, slowly rolling back.");
                   
                    setRpmF(maxRpm/CALIB_VERY_SLOW_DIVISOR);
                    //stepper->setRPM(maxRpm);
                    //stepper->enable();
                    //stepper->startMove(getPurgeSteps());
                    doMove(-calibSensor2Dose);
                    
                } else if (triggeredBy == 2) {
                    dbg.printf("Calib tube empty on test dose!\n");
                    if (!sensorState) {
                      abortCalibration(1, "Fluid sensor is reporting empty before test dosing is complete!");  
                    } else {
                      abortCalibration(1, "Fluid sensor changed state during test dosing before complete!");  
                    }
                    return;
                }
            } else if (calibrationStage == 13) {
                
                bool stable = isSensorStable();
                dbg.printf("fine tune loop %d, offset steps %d\n", calibFineTuneLoops, calibFineTuneStep);
                if (calibFineTuneLoops == 0) {
                    if (sensorState && stable) {
                        dbg.println("Sensor is still tripped first time, let's step back a little at a time and recheck.");
                        calibFineTuneFwd = false;
                        calibFineTuneStep -= ADJ_STEP_AMT;
                        slowMove(-ADJ_STEP_AMT, false);
                    } else {
                        dbg.println("Sensor not tripped first time, let's step fwd a little at a time and recheck.");
                        calibFineTuneFwd = true;
                        calibFineTuneStep += ADJ_STEP_AMT;
                        slowMove(ADJ_STEP_AMT), true;
                    }
                } else {
                    if  (!calibFineTuneFwd && (!stable || sensorState)) {
                        dbg.printf("Sensor not tripped or isnt stable (stable: %d, state %d), stepping back a little at a time and recheck %d.\n", stable, sensorState, calibFineTuneStep);
                        calibFineTuneFwd = false;
                        calibFineTuneStep -= ADJ_STEP_AMT;
                        slowMove(-ADJ_STEP_AMT, false);
                    } else if (calibFineTuneFwd && (!stable || !sensorState)) {
                        dbg.printf("Sensor tripped or isnt stable (stable: %d, state %d), stepping fwd a little at a time and recheck %d.\n", stable, sensorState, calibFineTuneStep);
                        calibFineTuneFwd = true;
                        calibFineTuneStep += ADJ_STEP_AMT;
                        slowMove(ADJ_STEP_AMT, false);
                    } else if (stable) {
                        //if it's still the same, we're done
                        if (calibFineTuneFwd && sensorState) {
                            dbg.printf("Moved forward, now turning it off");
                            calibFineTuneFwd = false;
                            calibFineTuneStep -= ADJ_STEP_AMT;
                            slowMove(-ADJ_STEP_AMT, false);
                            calibFineTuneLoops = 1;
                        }else {
                            dbg.printf("Fine tune complete, sensor is now in the right spot, fine tune steps %d.  dose before adjust: %d d\n", calibFineTuneStep, calibSensor2Dose);
                            calibSensor2Dose -= calibFineTuneStep;
                            return;
                        }
                    } else {
                        dbg.println("Sensor is not stable, uhh.. i think shouldn't be here maybe?");
                        abortCalibration(1, "don't know what happened!");  
                    }
                }
                calibFineTuneLoops++;
                if (calibFineTuneLoops > 100) {
                    dbg.println("Fine tune loop limit reached, aborting.");
                    abortCalibration(2, "Fine tune loop limit reached.");
                    return;
                }
                calibrationStage = 12;
            } else if (calibrationStage == 14) {
                dbg.println("Retracting after calibration success");
                setRpmF(maxRpm);
                doMove(getPurgeSteps());
            }            
             else if (calibrationStage == 15) {
                dbg.printf("Calibration complete with bottle to sensor of %d and sensor to dose of %d\n", calibBottle2Sensor, calibSensor2Dose);
               // stepper->disable();
                calibrationStage = 0;
                if (calibrationCallback != NULL) {
                    calibrationCallback(doserId, true,calibBottle2Sensor, calibSensor2Dose, 0, "");
                    calibrationCallback = NULL;
                }
            }
        }

        bool startContinuousPump() {
            if (isPumping()) {
                return false;
            }
            actualLastDosedSteps = 0;
            totalDoseSteps = 0;
            isContinuousPumping = true;
            //stepper->enable();
            //stepper->startMove(__LONG_MAX__);
            setRpmF(maxRpm);
            doMove(__LONG_MAX__);
            
            return true;
        }

        //set the maximum speed in RPM that the pump will turn
        void setMaxRPM(float maxRpm) {
            DoserPump::setMaxRPM(maxRpm);
            //stepper->setRPM(maxRpm);
            setRpmF(maxRpm);
        }
        
        bool lastSensorState = false;
        void handleSensorChanged() {
            if (calibrationStage > 0) {
                //let calib handle it
                calibrateNextStep(2);
            } else if (dosingStage > 0) {
                doseNextStep(2);
            }            
        }
           //wish interrupts would work
        void detectSensorChange() {
            if (onSensorDetectDelay > 0) {
                if (millis() < onSensorDetectDelay) {
                    return;
                }
                onSensorDetectDelay = 0;
            }
             sensorState = !digitalRead(sensorPin);
            if (lastSensorState != sensorState) {
                lastSensorState = sensorState;
                DoserPumpA4988::sensorChange(this);
            }
        }

        bool stepperGoin() {
            return stepperF->isRunning() || stepperF->isStopping();
        }

        void update() {
            detectSensorChange();
            if (sensorChangedSinceLast) {
                sensorChangedSinceLast = false;
                dbg.printf("Update loop found sensor changed to %d\n", sensorState);
                handleSensorChanged();
                //this->sensorChangedSinceLast = 0;
                //dbg.printf("sensor change last set to %d\n", sensorChangedSinceLast);
            }
            if (calibrationStage > 0) {
                //do calibration stuff
                if (!stepperGoin()) {
                //if (stepper->nextAction() < 1) {
                    dbg.printf("Calibration stage %d finished via stepper nextAction being 0\n", calibrationStage);
                    calibrateNextStep(1);
                }
            } else if (dosingStage > 0) {
                //if (stepper->nextAction() < 1) {
                if (!stepperGoin()) {
                    dbg.printf("Dosing stage %d finished via stepper nextAction being 0\n", dosingStage);
                    doseNextStep(1);
                }
            } else if (isContinuousPumping) {
                //if (stepper->nextAction() < 1) {
                if (!stepperGoin()) {
                    long stepsDone = getSteps();//stepperF->getCurrentPosition(); // stepper->getStepsCompleted();
                    
                    //stepper->disable();
                    float dosed = (float)((double)(stepsDone) / (double)stepsPermL);
                    if (dosed < 0) {
                        dosed = 0;
                    }
                    isContinuousPumping = false;
                    doseEndCallback(doserId, 2, false, stepsDone, dosed);
                }
            } else if (isRetracting) {
                //if (stepper->nextAction() < 1) {
                if (!stepperGoin()) {
                    
                    //stepper->disable();
                    isRetracting = false;
                    doseEndCallback(doserId, 3, false, 0, 0);
                }
            }
        }

        bool startDosing(float doseml) {
            if (isPumping() || doseml <= 0) {
                return false;
            }
            float doseStepsFloat = (doseml * stepsPermL);
            totalDoseSteps = (long)doseStepsFloat;
            if (totalDoseSteps <= 0) {
                return false;
            }
            dbg.dprintf("Starting dose of %f ml, %d steps\n", doseml, totalDoseSteps);
            actualLastDosedSteps = 0;
            dosingStage = 0;
            doseNextStep(0);
            return true;
        }

        bool stop() {
            if (calibrationStage > 0) {
                abortCalibration(0, "Stopped by user");
                return true;
            }
            if (dosingStage > 0) {
                float dosed = 0;
                if (dosingStage == 3) {
                    //if it aborted on stage 3, it's in the process of dosing and we need to get the steps it has done so far
                    long completedSteps = getSteps() - calibSensor2Dose; //stepper->getStepsCompleted() - calibSensor2Dose;
                    float dosed = (float)((double)(completedSteps) / (double)stepsPermL);
                    if (dosed < 0) {
                        dosed = 0;
                    }
                } else if (dosingStage >= 4) {
                    //if it aborted in stage 4+, then the total completed dose is stored to a variable
                    float dosed = (float)((double)(actualLastDosedSteps) / (double)stepsPermL);
                    if (dosed < 0) {
                        dosed = 0;
                    }
                }
                abortDosing(0, "Stopped by user", actualLastDosedSteps, dosed);
                retract();
                return true;
            }
            if (isRetracting) {
                //stepper->stop();
                stopStepper();
                //stepper->disable();
                isRetracting = false;
                doseEndCallback(doserId, 3, true, 0, 0);
                return true;
            }
            if (isContinuousPumping) {
                //stepper->stop();
                stopStepper();
                //stepper->disable();
                long stepsDone = getSteps();//stepperF->getCurrentPosition(); //stepper->getStepsCompleted();
                float dosed = (float)((double)(stepsDone) / (double)stepsPermL);
                if (dosed < 0) {
                    dosed = 0;
                }
                isContinuousPumping = false;
                doseEndCallback(doserId, 2, true, stepsDone, dosed);
                return true;
            }
            return false;
        }
        // float getDoseInProgressMl() {
        //     if ( isRetracting || (!isDosing && !isContinuousPumping)) {
        //         return (float)doseEndStepCount / stepsPermL;
        //     }

        //     return (float) adjustedStepsCompleted() / stepsPermL;
        // }

        // long getDoseInProgressSteps() {
        //     if (isRetracting || (!isDosing && !isContinuousPumping)) {
        //         return doseEndStepCount;
        //     }
            
        //     return adjustedStepsCompleted();
        // }

        // float getDoseInProgressPercent() {
        //     if (isContinuousPumping) {
        //         return 0;
        //     }
        //     if ((!isDosing && !isContinuousPumping) || isRetracting) {
        //         if (isAbort) {
        //             if (totalDoseSteps == 0) {
        //                 return 0;
        //             }
        //             return ((float)doseEndStepCount / (float)totalDoseSteps) * 100.0;
        //         }
        //         return 100;
        //     }
        //     long totalStepsComplete = adjustedStepsCompleted();
        //     float stepsTotal = (stepper->getStepsCompleted() + (float)stepper->getStepsRemaining()) - retractionStepCount;
        //     if (stepsTotal <= 0) {
        //         return 0;
        //     }
        //     return ((float)totalStepsComplete/stepsTotal)*100.0;
        // }

        // bool doseStepsImmediate(long steps) {
        //     if (isPumping() || steps == 0) {
        //         return false;
        //     }
        //     resetForDose();
        //     if (steps > 0 && retractionEnabled && retracted) {
        //         retracted = false;
        //     }
        //     stepper->enable();
        //     stepper->move(steps);
        //     stepper->disable();
        //     return true;
        // }
        
        // bool retract() {
        //     if (isPumping()) {
        //         return false;
        //     }
        //     doMove
            
        //     stepper->enable();
        //     stepper->move(-(retractionmL * stepsPermL));
        //     retracted = true;
        //     stepper->disable();
        //     return true;
        // }
        
        // bool unretractImmediate() {
        //     if (isPumping() || !retractionEnabled || !retracted) {
        //         return false;
        //     }
        //     stepper->enable();
        //     stepper->move(retractionmL * stepsPermL);
        //     retracted = false;
        //     stepper->disable();
        //     return true;
        // }
};

void IRAM_ATTR DoserPumpA4988::sensorChange(void* ref) {
    DoserPumpA4988* pump = (DoserPumpA4988*)ref;
    pump->sensorState = !digitalRead(pump->sensorPin);
   // dbg.println("sensor changed");
    if (pump->onSensorInterruptAction == 0) {
        //don't trigger change event if negative value
    } else {
        if (pump->sensorChangedSinceLast) {
            //hmm.. not ideal, means it was not handled and interrupt will overwrite
        }
        pump->sensorChangedSinceLast = true;

        
        switch (pump->onSensorInterruptAction) {
            case 1:  //1 is pump stop immediate and store number of steps
                pump->sensorInterruptStepCount = pump->getSteps();//stepperF->getCurrentPosition(); // pump->stepper->getStepsCompleted();
                //pump->stepper->stop();
                pump->stopStepper();    
                break;
            case 2: //2 is just store number of steps
                pump->sensorInterruptStepCount =pump->getSteps();//stepperF->getCurrentPosition(); // pump->stepper->getStepsCompleted();
                break;
        }
    }

}