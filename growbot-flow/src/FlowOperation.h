#include <Arduino.h>
#include "SolenoidValve.h"
#include "WaterLevel.h"

#pragma once
enum FlowOpType { FillToPercent, DrainToPercent, Empty, Flush, FlushAndFillToPercent, Wait };
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


#define FILL_CHECK_MS 30000
class FillToPercentOp : public FlowOp {
    private:
        float targetPercent;
        SolenoidValve* inValve;
        WaterLevel* level;
        unsigned long startedAt = 0;

        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;
        bool done = false;
        bool aborted = false;
        bool failed = false;
    public:
        FlowOpType getType() { return FlowOpType::FillToPercent; }
        FillToPercentOp(WaterLevel* waterLevel, SolenoidValve* inValve, float targetPercent) {
            this->targetPercent = targetPercent;
            this->inValve = inValve;
            this->level = waterLevel;
        }
        void start() {
            dbg.println("Starting FillToPercentOp");
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
            if (levelNow >= targetPercent) {
                inValve->setOn(false);
                done = true;
                dbg.printf("Finished filling, at %f of target %f\n", levelNow, targetPercent);
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > FILL_CHECK_MS) {
                dbg.printf("Filling, %f of target %f\n", levelNow, targetPercent);
                if ((levelNow - lastCheck) < 0.5) {
                    dbg.printf("FAIL: water level %f hasn't gone up even half a percent since last check!\n", levelNow);
                    inValve->setOn(false);
                    failed = true;
                    return;
                }
                lastCheckPercent = levelNow;
                lastCheck = millis();
            }
            
        }
        void abort() {
            aborted = true;
            failed = true;
        }
        bool isDone() {
            return done || aborted || failed;
        }
};


#define DRAIN_CHECK_MS 30000
class DrainToPercentOp : public FlowOp {
    private:
        float targetPercent;
        SolenoidValve* outValve;
        WaterLevel* level;
        unsigned long startedAt = 0;

        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;
        bool done = false;
        bool aborted = false;
        bool failed = false;
    public:
        FlowOpType getType() { return FlowOpType::DrainToPercent; }
        DrainToPercentOp(WaterLevel* waterLevel, SolenoidValve* outValve, float targetPercent) {
            this->targetPercent = targetPercent;
            this->outValve = outValve;
            this->level = waterLevel;
        }
        void start() {
            dbg.println("Starting DrainToPercentOp");
            done = false;
            failed = false;
            aborted = false;
            startedAt = millis();
            lastCheck = millis() + DRAIN_CHECK_MS;
            lastCheckPercent = level->getImmediateLevel();
            if (lastCheckPercent <= targetPercent) {
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
            if (levelNow <= targetPercent) {
                outValve->setOn(false);
                done = true;
                dbg.printf("Finished draining, at %f of target %f\n", levelNow, targetPercent);
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > FILL_CHECK_MS) {
                dbg.printf("Draining, %f of target %f\n", levelNow, targetPercent);
                if ((lastCheck - levelNow) < 0.5) {
                    dbg.printf("FAIL: water level %f hasn't gone down even half a percent since last check!\n", levelNow);
                    outValve->setOn(false);
                    failed = true;
                    return;
                }
                lastCheckPercent = levelNow;
                lastCheck = millis();
            }
            
        }
        void abort() {
            aborted = true;
            failed = true;
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
    public:
        
        MetaFlowOp(): FlowOp() {}
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
        }

        virtual void start() {
            dbg.println("Starting MetaFlowOp");
            if (currentIndex >= 0) {
                dbg.println("meta op tried starting when current index is set!");
                return;
            }
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
            if (currentOp->isDone()) {
                if (currentOp->isFailed()) {
                    dbg.println("meta op has a failed op");
                    failed = true;
                    return;
                }
                if (currentIndex == this->opSteps.size() - 1) {
                    dbg.println("finished the last step in the meta op");
                    done = true;
                    return;
                }
                currentIndex++;
                dbg.printf("meta op moving to step %d\n", currentIndex);
                currentOp = opSteps.at(currentIndex);
                currentOp->start();
            }
        }

};

class EmptyOp : public MetaFlowOp {
    public:
        virtual FlowOpType getType() { return FlowOpType::Empty; }
        EmptyOp(WaterLevel* waterLevel, SolenoidValve* outValve) {
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
            this->opSteps.push_back(new WaitOp(20000));
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
        }
        virtual ~EmptyOp() {}
};

class FlushOp : public MetaFlowOp {
    protected:
        using MetaFlowOp::opSteps;
    public:
        virtual FlowOpType getType() { return FlowOpType::Flush; }
        FlushOp(WaterLevel* waterLevel, SolenoidValve* inValve, SolenoidValve* outValve, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300) {
            for (int i = 0; i < flushes; i++) {
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
                this->opSteps.push_back(new WaitOp(20000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
                this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, rinseFillPercent));
                this->opSteps.push_back(new WaitOp(rinseSec*1000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
                this->opSteps.push_back(new WaitOp(20000));
                this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, 0.0));
            }
        }
        virtual ~FlushOp() {}
};

class FlushAndFillToPercentOp : public FlushOp {
      public:
        virtual FlowOpType getType() { return FlowOpType::FlushAndFillToPercent; }
        FlushAndFillToPercentOp(WaterLevel* waterLevel, SolenoidValve* inValve, SolenoidValve* outValve, float fillToPercent, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300)
            : FlushOp(waterLevel, inValve, outValve, flushes, rinseFillPercent, rinseSec)
        {
            this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, fillToPercent));
        }
        virtual ~FlushAndFillToPercentOp() {}

};