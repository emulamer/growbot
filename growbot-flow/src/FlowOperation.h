#include <Arduino.h>
#include "SolenoidValve.h"
#include "WaterLevel.h"

#pragma once
enum FlowOpType { FillToPercentOnce, FillToPercent, DrainToPercent, Empty, Flush, FlushAndFillToPercent, Wait };
class FlowOp {
    public:
        FlowOp() {}
        virtual ~FlowOp() {};
        virtual FlowOpType getType() = 0;
        virtual void handle() = 0;
        virtual void start() = 0;
        virtual void abort() = 0;
        virtual bool isDone() = 0;
        virtual bool isFailed() = 0;
        float opOutFlow = NAN;
        float opInFlow = NAN;
        float opTotalDeltaFlow = NAN;
};

class WaitOp : public FlowOp {
    private:
        unsigned long startedAt = 0;
        int milliseconds;
        bool aborted = false;
        bool done = false;
    public:
        FlowOpType getType() { return FlowOpType::Wait; }
        WaitOp(int milliseconds) {
            this->milliseconds = milliseconds;
        }
        void start() {
            startedAt = millis();
            dbg.println("Starting WaitOp");
        }
        void handle() {
            if (done || aborted) {
                return;
            }
            if (startedAt > 0 && (startedAt + milliseconds) > millis()) {
                done = true;
                startedAt = 0;
            }
        }

        bool isDone() {
            return done || aborted;
        }
        bool isFailed() {
            return aborted;
        }
        void abort() {
            aborted = true;
        }
};


#define FILL_CHECK_MS 45000
class FillToPercentOp : public FlowOp {
    private:
        float targetPercent;
        SolenoidValve* inValve;
        WaterLevel* level;
        unsigned long startedAt = 0;
        float inStartingLiters = 0;
        float outStartingLiters = 0;

        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;
        bool done = false;
        bool aborted = false;
        bool failed = false;
        FlowMeter* inFlow;
        FlowMeter* outFlow;
    public:

        FlowOpType getType() { return FlowOpType::FillToPercentOnce; }
        FillToPercentOp(WaterLevel* waterLevel, SolenoidValve* inValve, FlowMeter* inFlow, FlowMeter* outFlow, float targetPercent) {
            this->targetPercent = targetPercent;
            this->inValve = inValve;
            this->level = waterLevel;
            this->inFlow = inFlow;
            this->outFlow = outFlow;
        }
        void start() {
            dbg.println("Starting FillToPercentOp");
            if (startedAt != 0) {
                dbg.println("fill to percent op tried to start but was already started");
                return;
            }
            inStartingLiters = inFlow->getLitersSinceReset();
            outStartingLiters = outFlow->getLitersSinceReset();
            done = false;
            failed = false;
            aborted = false;
            startedAt = millis();
            lastCheck = millis() + FILL_CHECK_MS;
            lastCheckPercent = level->getImmediateLevel();
            if (lastCheckPercent >= targetPercent) {
                done = true;
            } else {
                inValve->setOn(true);
            }
        }
        bool isFailed() {
            return failed || aborted;
        }
        void handle() {
            if (failed || aborted || done) {
                return;
            }
            float levelNow = level->getImmediateLevel();
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            if (levelNow >= targetPercent) {
                inValve->setOn(false);
                done = true;
                dbg.printf("Finished filling, at %f of target %f\n", levelNow, targetPercent);
                opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
                opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
                opTotalDeltaFlow = opInFlow - opOutFlow;
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > FILL_CHECK_MS) {
                dbg.printf("Filling, %f of target %f\n", levelNow, targetPercent);
                if ((levelNow - lastCheckPercent) < 0.1) {
                    dbg.printf("FAIL: water level %f hasn't gone up even a little since last check of %f!\n", levelNow, lastCheckPercent);
                    inValve->setOn(false);
                    failed = true;
                    opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
                    opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
                    opTotalDeltaFlow = opInFlow - opOutFlow;
                    return;
                }
                lastCheckPercent = levelNow;
                lastCheck = millis();
            }
            
        }
        void abort() {
            inValve->setOn(false);
            if (aborted || done || failed) {
                return;
            }
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            aborted = true;
            failed = true;
        }
        bool isDone() {
            return done || aborted || failed;
        }
};


#define DRAIN_CHECK_MS 45000
class DrainToPercentOp : public FlowOp {
    private:
        float targetPercent;
        SolenoidValve* outValve;
        WaterLevel* level;
        unsigned long startedAt = 0;
        float inStartingLiters = 0;
        float outStartingLiters = 0;
        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;
        bool done = false;
        bool aborted = false;
        bool failed = false;
        FlowMeter* inFlow;
        FlowMeter* outFlow;
    public:
        FlowOpType getType() { return FlowOpType::DrainToPercent; }
        DrainToPercentOp(WaterLevel* waterLevel, SolenoidValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, float targetPercent) {
            this->targetPercent = targetPercent;
            this->outValve = outValve;
            this->level = waterLevel;
            this->inFlow = inFlow;
            this->outFlow = outFlow;
        }
        void start() {
            if (startedAt != 0) {
                dbg.println("tried starting drain op that's already started");
                return;
            }
            inStartingLiters = inFlow->getLitersSinceReset();
            outStartingLiters = outFlow->getLitersSinceReset();
            dbg.println("Starting DrainToPercentOp");
            done = false;
            failed = false;
            aborted = false;
            startedAt = millis();
            lastCheck = millis() + DRAIN_CHECK_MS;
            lastCheckPercent = level->getImmediateLevel();
            if (lastCheckPercent <= targetPercent) {
                dbg.printf("current level %f is already below target level %f, already done\n", lastCheckPercent, targetPercent);
                done = true;
            } else {
                outValve->setOn(true);
            }
        }
        bool isFailed() {
            return failed || aborted;
        }
        void handle() {
            if (failed || aborted || done) {
                return;
            }
            float levelNow = level->getImmediateLevel();
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            if (levelNow <= targetPercent) {
                outValve->setOn(false);
                done = true;
                opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
                opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
                opTotalDeltaFlow = opInFlow - opOutFlow;
                dbg.printf("Finished draining, at %f of target %f\n", levelNow, targetPercent);
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > DRAIN_CHECK_MS) {
                dbg.printf("Draining, %f of target %f\n", levelNow, targetPercent);
                if ((lastCheckPercent - levelNow) < 0.1) {
                    dbg.printf("FAIL: water level %f hasn't gone down even a little last check of %f!\n", levelNow, lastCheckPercent);
                    outValve->setOn(false);
                    failed = true;
                    return;
                }
                lastCheckPercent = levelNow;
                lastCheck = millis();
            }
            
        }
        void abort() {
            outValve->setOn(false);
            if (aborted || done || failed) {
                return;
            }
            aborted = true;
            failed = true;
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
        }
        bool isDone() {
            return done || aborted || failed;
        }
};



class MetaFlowOp : public FlowOp {
    protected:        
        std::vector<FlowOp*> opSteps;
        int currentIndex = -1;
        FlowOp* currentOp = NULL;
        bool aborted = false;
        bool failed = false;
        bool done = false;
        bool started = false;
        FlowMeter* inFlow;
        FlowMeter* outFlow;
        float inStartingLiters = 0;
        float outStartingLiters = 0;
    public:
        MetaFlowOp(FlowMeter* inFlow, FlowMeter* outFlow): FlowOp() {
            this->inFlow = inFlow;
            this->outFlow = outFlow;
        }
        virtual ~MetaFlowOp() {
            dbg.println("Destructing MetaFlowOp");
            while (opSteps.size() > 0) {
                FlowOp* op = opSteps.back();
                op->abort();
                delete op;
                opSteps.pop_back();
            }
        }
        virtual bool isDone() {
            return done || failed || aborted;
            // for (auto step : this->opSteps) {
            //     if (!step->isDone()) {
            //         return false;
            //     }
            // }
            // return true;
        }

        virtual bool isFailed() {
            return failed || aborted;
            // for (auto step : this->opSteps) {
            //     if (step->isFailed()) {
            //         return true;
            //     }
            // }
            // return false;
        }

        virtual void abort() {
            aborted = true;
            for (auto step : this->opSteps) {
                step->abort();
            }
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
        }

        virtual void start() {
            dbg.println("Starting MetaFlowOp");
            if (currentIndex >= 0) {
                dbg.println("meta op tried starting when current index is set!");
                return;
            }
            inStartingLiters = inFlow->getLitersSinceReset();
            outStartingLiters = outFlow->getLitersSinceReset();
            currentIndex = 0;
            currentOp = this->opSteps.at(currentIndex);
            currentOp->start();
            started = true;
        }

        virtual void handle() {
            if (!started || done || aborted || failed || currentOp == NULL) {
                return;
            }
            currentOp->handle();
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            if (currentOp->isDone()) {
                if (currentOp->isFailed()) {
                    dbg.println("meta op has a failed op");
                    failed = true;
                } else if (currentIndex == this->opSteps.size() - 1) {
                    dbg.println("finished the last step in the meta op");
                    done = true;
                } else {
                    currentIndex++;
                    dbg.printf("meta op moving to step %d\n", currentIndex);
                    currentOp = opSteps.at(currentIndex);
                    currentOp->start();
                }
            }
        }

};

class EmptyOp : public MetaFlowOp {
    private:
        SolenoidValve* outValve;
    public:
        virtual FlowOpType getType() { return FlowOpType::Empty; }
        EmptyOp(WaterLevel* waterLevel, SolenoidValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow)
            : MetaFlowOp(inFlow, outFlow) {
                this->outValve = outValve;
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
            this->opSteps.push_back(new WaitOp(45000));
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
        }
        virtual void start() {
            inStartingLiters = inFlow->getLitersSinceReset();
            outStartingLiters = outFlow->getLitersSinceReset();
            MetaFlowOp::start();
        }
        virtual void handle() {
            MetaFlowOp::handle();
            if (isDone()) {

            }
        }
        virtual void abort() {
            outValve->setOn(false);
            MetaFlowOp::abort();
        }
        virtual ~EmptyOp() {}
};

class FlushOp : public MetaFlowOp {
    private:
        SolenoidValve* outValve;
        SolenoidValve* inValve;
    protected:
        using MetaFlowOp::opSteps;
    public:
        virtual FlowOpType getType() { return FlowOpType::Flush; }
        FlushOp(WaterLevel* waterLevel, SolenoidValve* inValve, SolenoidValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300) 
            : MetaFlowOp(inFlow, outFlow) {
                this->inValve = inValve;
                this->outValve = outValve;
            for (int i = 0; i < flushes; i++) {
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
                this->opSteps.push_back(new WaitOp(45000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
                this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, rinseFillPercent));
                this->opSteps.push_back(new WaitOp(rinseSec*1000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
                this->opSteps.push_back(new WaitOp(45000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, 0.0));
            }
        }
        virtual void abort() {
            this->inValve->setOn(false);
            this->outValve->setOn(false);
            MetaFlowOp::abort();
        }
        virtual ~FlushOp() {}
};

class FlushAndFillToPercentOp : public FlushOp {
      public:
        virtual FlowOpType getType() { return FlowOpType::FlushAndFillToPercent; }
        FlushAndFillToPercentOp(WaterLevel* waterLevel, SolenoidValve* inValve, SolenoidValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, float fillToPercent, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300)
            : FlushOp(waterLevel, inValve, outValve, inFlow, outFlow, flushes, rinseFillPercent, rinseSec)
        {
            this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, fillToPercent));
        }
        virtual ~FlushAndFillToPercentOp() {}

};

class FillToPercentEnsured : public MetaFlowOp {
    private:
        SolenoidValve* inValve;
    public:
    virtual FlowOpType getType() {return FlowOpType::FillToPercent;}
    FillToPercentEnsured(WaterLevel* waterLevel, SolenoidValve* inValve, FlowMeter* inFlow, FlowMeter* outFlow, float percent) 
        : MetaFlowOp(inFlow, outFlow) {
            this->inValve = inValve;
                this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, percent));
                this->opSteps.push_back(new WaitOp(45000));
                this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, percent));
    }
    virtual void abort() {
        this->inValve->setOn(false);
        MetaFlowOp::abort();
    }
    virtual ~FillToPercentEnsured() {}

};