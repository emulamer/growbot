#include <Arduino.h>
#include <BasicStepperDriver.h>

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

#define DEFAULT_STEPS_PER_ML 4790

#define DEFAULT_RETRACTION_ML 1.5

#define DEFAULT_MAX_RPM 160

#define NUMBER_OF_STEPS_PER_ROTATION 200

//callback definition for when dosing or continuous pumping ends
//  doserId: the ID of the doser provided when constructing the object
//  aborted: true if the dosing ended because it was aborted
//  stepsDosed: the total number of steps that were taken on the doser stepper.  Note: with continuous pumping, this won't be accurate if the total number of steps goes over max value of a long
//  mlDosed: the total number of mililiters that were dosed.  Note: with continuous pumping, this won't be accurate if the total number of steps goes over max value of a long
//  isRetracted: true if the pump is in a retracted state
//  wasContinuousPump: true if the dose end was a continuous pump
typedef void (*DoseEndCallback)(int doserId, bool aborted, long stepsDosed, float mlDosed, bool isRetracted, bool wasContinuousPump);

class DoserPump {
    protected:
        int doserId = 0;
        bool isDosing = false;
        bool isContinuousPumping = false;
        bool isRetracting = false;
        bool retracted = true;
        bool retractionEnabled = true;
        unsigned long stepsPermL = DEFAULT_STEPS_PER_ML;
        float retractionmL = DEFAULT_RETRACTION_ML;
        float maxRpm = DEFAULT_MAX_RPM;
        DoseEndCallback doseEndCallback;
    public:
        DoserPump(int doserId) {
            this->doserId = doserId;
        }
        DoserPump(int doserId, unsigned long stepsPerMl, float maxRpm, bool retractionEnabled, float retractionmL, bool isRetracted) {
            this->doserId = doserId;
            this->stepsPermL = stepsPerMl;
            this->maxRpm = maxRpm;
            this->retractionEnabled = retractionEnabled;
            this->retracted = isRetracted;
            this->retractionmL = retractionmL;
        }
    //gets the calibration as the number of steps the motor takes for 1 mL of output
    unsigned long getCalibration() {
        return stepsPermL;
    }

    //gets the number of mililiters that the pump retracts after dosing
    float getRetractionMl() {
        return retractionmL;
    }

    //gets a value indicating whether the pump turns backwards after dosing to retract the dosing liquid
    bool getRetractionEnabled() {
        return retractionEnabled;
    }

    //gets a value indicating whether the liquid is currently in a retracted state
    bool isRetracted() {
        return retracted;
    }

    //gets the max RPM
    float getMaxRpm() {
        return maxRpm;
    }

    //gets the ID of the doser provided in the constructor
    int getDoserId() {
        return doserId;
    }


    //when dosing is in progress, gets the mL pumped so far
    virtual float getDoseInProgressMl() = 0;

    //when dosing is in progress, get the number of steps so far
    virtual long getDoseInProgressSteps() = 0;

    //when dosing is in progress, gets the percentage complete of the dosing operation
    virtual float getDoseInProgressPercent() = 0;

    //returns true if the pump is currently dosing or pumping continuously
    virtual bool isPumping() {
        return isDosing || isRetracting || isContinuousPumping;
    }

    //sets the calibration as the number of steps the motor takes for 1 mL of output.
    virtual void setCalibration(unsigned long stepsPerML) {
        this->stepsPermL = stepsPermL;
    }

    //sets the number of mililiters that the pump retracts after dosing
    virtual void setRetractionMl(float retractionMl) {
        this->retractionmL = retractionMl;
    }

    //sets whether the pump turns backwards after dosing to retract the dosing liquid and whether the liquid is currently in a retracted state
    virtual void setRetractionEnabled(bool enableRetraction, bool isRetracted) {
        this->retractionEnabled = retractionEnabled;
        this->retracted = isRetracted;
    }

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
    virtual bool startContinuousPump(bool skipRetract = false) = 0;

    //should be called in the main loop()
    virtual void update() = 0;

    //immediately stop the doser and abort any dosing in progress.  returns true if there was dosing in progress, false if there was not.
    virtual bool stop(bool skipRetract = false) = 0;

    //begin dosing the specified number of mL.  returns true if dosing was started, returns false if dosing did not start (e.g. if another dosing is in progress)
    virtual bool startDosing(float doseml) = 0;

    //blocking call.  Immediately doses the number of steps, does no retraction or unretraction.
    //if retraction is enabled and provided a positive number of steps, sets the retraction state to not retracted.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
    virtual bool doseStepsImmediate(long steps) = 0;

    //blocking call.  If not retracted, immediately retracts to the set number of ml.
    //if retraction is disabled or already in a retracted state, returns false.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
    virtual bool retractImmediate() = 0;

    //blocking call.  If retracted, immediately un-retracts to the set number of ml.
    //if retraction is disabled or already not in a retracted state, returns false.
    //returns false if dosing or pumping is already in progress.
    //does not call dose end callback.
    virtual bool unretractImmediate() = 0;

};

//implementation of DoserPump for an A4988 stepper driver
class DoserPumpA4988 : public DoserPump {
    private:
        BasicStepperDriver *stepper;
        long doseEndStepCount = 0;
        bool isAbort = false;
        long retractionStepCount = 0;
        long totalDoseSteps = 0;

        void initStepper(int dirPin, int stepPin, int sleepPin) {
            this->stepper = new BasicStepperDriver(NUMBER_OF_STEPS_PER_ROTATION, dirPin, stepPin, sleepPin);
            this->stepper->begin(this->maxRpm, 16);
            this->stepper->setSpeedProfile(BasicStepperDriver::LINEAR_SPEED, 500, 500);
            this->stepper->setEnableActiveState(HIGH);
            this->stepper->disable();
        }
        long adjustedStepsCompleted() {
            long totalSteps = stepper->getStepsCompleted() - retractionStepCount;
            if (totalSteps < 0) {
                totalSteps = 0;
            }
            return totalSteps;
        }
        void invokeDoneCallback(bool wasContinuousPump) {
            if (this->doseEndCallback != NULL) {
                
                this->doseEndCallback(doserId, isAbort, doseEndStepCount, (float)doseEndStepCount/stepsPermL, retracted, wasContinuousPump);
            }
        }

        void retractDone() {
            bool isCont = isContinuousPumping;
            isRetracting = false;
            isDosing = false;
            isContinuousPumping = false;
            retracted = true;
            stepper->disable();
            invokeDoneCallback(isCont);
        }

        void moveDone(bool skipRetract = false) {
            if (retractionEnabled && !skipRetract) {
                isRetracting = true;
                doseEndStepCount = adjustedStepsCompleted();
                stepper->startMove(-(retractionmL * stepsPermL));
            } else {
                bool isCont = isContinuousPumping;
                isDosing = false;
                isRetracting = false;
                isContinuousPumping = false;
                doseEndStepCount = adjustedStepsCompleted();
                stepper->disable();
                invokeDoneCallback(isCont);
            }
        }



        void resetForDose() {
            isAbort = false;
            isContinuousPumping = false;
            isDosing = false;
            isRetracting = false;
            doseEndStepCount = 0;
            totalDoseSteps = 0;
        }
    public:
        DoserPumpA4988(int doserId, int dirPin, int stepPin, int sleepPin) : DoserPump(doserId) {
            initStepper(dirPin, stepPin, sleepPin);
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
        DoserPumpA4988(int doserId, int dirPin, int stepPin, int sleepPin, unsigned long stepsPerMl, float maxRpm, bool retractionEnabled, float retractionmL, bool isRetracted, DoseEndCallback endCallback) : DoserPump(doserId, stepsPerMl, maxRpm, retractionEnabled, retractionmL, isRetracted) {
            setDoseEndCallback(endCallback);
            initStepper(dirPin, stepPin, sleepPin);            
        }

        bool startContinuousPump(bool skipRetract = false) {
            if (isPumping()) {
                return false;
            }
            resetForDose();
            isContinuousPumping = true;
            if (retracted) {
                retractionStepCount = retractionmL * stepsPermL;
                retracted = false;
            } else {
                retractionStepCount = 0;
            }
            stepper->enable();
            stepper->startMove(__LONG_MAX__);
            return true;
        }

        //set the maximum speed in RPM that the pump will turn
        void setMaxRPM(float maxRpm) {
            DoserPump::setMaxRPM(maxRpm);
            stepper->setRPM(maxRpm);
        }
        
        void update() {
            if (isDosing) {
                if (stepper->nextAction() < 1) {
                    if (isRetracting) {
                        retractDone();
                    } else {
                        moveDone();
                    }
                }
            } else if (isContinuousPumping) {
                if (stepper->nextAction() < 1) {
                    if (isRetracting) {
                        retractDone();
                    } else {
                        stepper->startMove(__LONG_MAX__);
                    }
                }
            }
        }

        bool startDosing(float doseml) {
            if (isPumping() || doseml <= 0) {
                return false;
            }
            resetForDose();
            isDosing = true;
            float doseStepsFloat = (doseml * stepsPermL);
            totalDoseSteps = (long)doseStepsFloat;
            if (retracted) {
                retractionStepCount = retractionmL* stepsPermL;
                doseStepsFloat += retractionStepCount;
            } else {
                retractionStepCount = 0;
            }
            retracted = false;
            unsigned long doseSteps = (unsigned long) doseStepsFloat;
            isDosing = true;            
            stepper->enable();
            stepper->startMove(doseSteps);
            return true;
        }

        bool stop(bool skipRetract = false) {
            if (isRetracting && !skipRetract) {
                isAbort = false;
                return false;
            }
            stepper->stop();
            stepper->disable();
            if (isDosing || isContinuousPumping) {
                isAbort = true;
                if (isRetracting) {
                    doseEndStepCount = adjustedStepsCompleted();
                    retractDone();
                } else {
                    moveDone(skipRetract);
                }
                return true;
            } else {
                return false;
            }
        }
        float getDoseInProgressMl() {
            if (isRetracting || (!isDosing && !isContinuousPumping)) {
                return (float)doseEndStepCount / stepsPermL;
            }

            return (float) adjustedStepsCompleted() / stepsPermL;
        }

        long getDoseInProgressSteps() {
            if (isRetracting || (!isDosing && !isContinuousPumping)) {
                return doseEndStepCount;
            }
            
            return adjustedStepsCompleted();
        }

        float getDoseInProgressPercent() {
            if (isContinuousPumping) {
                return 0;
            }
            if ((!isDosing && !isContinuousPumping) || isRetracting) {
                if (isAbort) {
                    if (totalDoseSteps == 0) {
                        return 0;
                    }
                    return ((float)doseEndStepCount / (float)totalDoseSteps) * 100.0;
                }
                return 100;
            }
            long totalStepsComplete = adjustedStepsCompleted();
            float stepsTotal = (stepper->getStepsCompleted() + (float)stepper->getStepsRemaining()) - retractionStepCount;
            if (stepsTotal <= 0) {
                return 0;
            }
            return ((float)totalStepsComplete/stepsTotal)*100.0;
        }

        bool doseStepsImmediate(long steps) {
            if (isPumping() || steps == 0) {
                return false;
            }
            resetForDose();
            if (steps > 0 && retractionEnabled && retracted) {
                retracted = false;
            }
            stepper->enable();
            stepper->move(steps);
            stepper->disable();
            return true;
        }
        
        bool retractImmediate() {
            if (isPumping() || !retractionEnabled || retracted) {
                return false;
            }
            stepper->enable();
            stepper->move(-(retractionmL * stepsPermL));
            retracted = true;
            stepper->disable();
            return true;
        }
        
        bool unretractImmediate() {
            if (isPumping() || !retractionEnabled || !retracted) {
                return false;
            }
            stepper->enable();
            stepper->move(retractionmL * stepsPermL);
            retracted = false;
            stepper->disable();
            return true;
        }
};