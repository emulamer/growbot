#include <Arduino.h>
#include "FlowValve.h"
#include "WaterLevel.h"

#pragma once
struct FlowOpType {
     static const String FillToPercentOnce;
     static const String FillToPercent;
     static const String DrainToPercent;
     static const String Empty;
     static const String Flush;
     static const String FlushAndFillToPercent;
     static const String Wait;
};
const String FlowOpType::FillToPercentOnce = "FillToPercentOnce";
const String FlowOpType::FillToPercent = "FillToPercent";
const String FlowOpType::DrainToPercent = "DrainToPercent";
const String FlowOpType::Empty = "Empty";
const String FlowOpType::Flush = "Flush";
const String FlowOpType::FlushAndFillToPercent = "FlushAndFillToPercent";
const String FlowOpType::Wait  = "Wait";
class FlowOp {
    protected:
        String errorMessage;
        bool done = false;
        bool aborted = false;
        bool failed = false;
        std::vector<float> params;
        String id;
    public:
        FlowOp() {
            uint8_t uuid_array[16];
            ESPRandom::uuid4(uuid_array);
            this->id = ESPRandom::uuidToString(uuid_array);
        }
        virtual ~FlowOp() {};
        virtual String getType() = 0;
        virtual void handle() = 0;
        virtual void start() = 0;
        virtual void abort() = 0;
        bool isDone() {
            return done || aborted || failed;
        }
        bool isFailed() {
            return failed || aborted;
        }
        bool isAborted() {
            return aborted;
        }
        float opOutFlow = 0;
        float opInFlow = 0;
        float opTotalDeltaFlow = 0;
        String getError() {
            return errorMessage;
        }
        String getId() {
            return this->id;
        }

        
        virtual std::vector<float> getParams() {
            return params;
        }
};

class WaitOp : public FlowOp {
    private:
        unsigned long startedAt = 0;
        int milliseconds;
    public:
        String getType() { return FlowOpType::Wait; }
        WaitOp(int milliseconds) {
            this->milliseconds = milliseconds;
            params.push_back(milliseconds);
        }
        void start() {
            startedAt = millis();
            dbg.println("Starting WaitOp");
        }
        void handle() {
            if (done || aborted) {
                return;
            }
            if (startedAt > 0 && (millis() - startedAt) > milliseconds) {
                dbg.println("waitop done");
                done = true;
                startedAt = 0;
            }
        }
        void abort() {
            aborted = true;
        }
        
};


#define FILL_CHECK_MS 60000
class FillToPercentOp : public FlowOp {
    private:
        float targetPercent;
        FlowValve* inValve;
        WaterLevel* level;
        unsigned long startedAt = 0;
        float inStartingLiters = 0;
        float outStartingLiters = 0;

        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;

        FlowMeter* inFlow;
        FlowMeter* outFlow;
    public:

        String getType() { return FlowOpType::FillToPercentOnce; }
        FillToPercentOp(WaterLevel* waterLevel, FlowValve* inValve, FlowMeter* inFlow, FlowMeter* outFlow, float targetPercent) {
            this->targetPercent = targetPercent;
            this->inValve = inValve;
            this->level = waterLevel;
            this->inFlow = inFlow;
            this->outFlow = outFlow;
            params.push_back(targetPercent);
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
                inValve->setOpen(true);
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
                inValve->setOpen(false);
                done = true;
                dbg.printf("Finished filling, at %f of target %f\n", levelNow, targetPercent);
                opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
                opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
                opTotalDeltaFlow = opInFlow - opOutFlow;
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > FILL_CHECK_MS) {
                dbg.printf("Filling, %f of target %f\n", levelNow, targetPercent);
                if ((levelNow - lastCheckPercent) < 0.05) {
                    dbg.printf("FAIL: water level %f hasn't gone up even a little since last check of %f!\n", levelNow, lastCheckPercent);
                    errorMessage = "Water level hasn't gone up even a little!";
                    inValve->setOpen(false);
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
            inValve->setOpen(false);
            if (aborted || done || failed) {
                return;
            }
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            aborted = true;
            failed = true;
        }
};


#define DRAIN_CHECK_MS 60000
class DrainToPercentOp : public FlowOp {
    private:
        float targetPercent;
        FlowValve* outValve;
        WaterLevel* level;
        unsigned long startedAt = 0;
        float inStartingLiters = 0;
        float outStartingLiters = 0;
        unsigned long lastCheck = 0;
        float lastCheckPercent = 0;
        FlowMeter* inFlow;
        FlowMeter* outFlow;
    public:
        String getType() { return FlowOpType::DrainToPercent; }
        DrainToPercentOp(WaterLevel* waterLevel, FlowValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, float targetPercent) {
            this->targetPercent = targetPercent;
            this->outValve = outValve;
            this->level = waterLevel;
            this->inFlow = inFlow;
            this->outFlow = outFlow;
            params.push_back(targetPercent);
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
                outValve->setOpen(true);
            }
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
                outValve->setOpen(false);
                done = true;
                opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
                opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
                opTotalDeltaFlow = opInFlow - opOutFlow;
                dbg.printf("Finished draining, at %f of target %f\n", levelNow, targetPercent);
                return;
            }
            if (millis() > lastCheck && (millis() - lastCheck) > DRAIN_CHECK_MS) {
                dbg.printf("Draining, %f of target %f\n", levelNow, targetPercent);
                if ((lastCheckPercent - levelNow) < 0.05) {
                    dbg.printf("FAIL: water level %f hasn't gone down even a little last check of %f!\n", levelNow, lastCheckPercent);
                    errorMessage = "Water level hasn't gone down even a little!";
                    outValve->setOpen(false);
                    failed = true;
                    return;
                }
                lastCheckPercent = levelNow;
                lastCheck = millis();
            }
            
        }
        void abort() {
            outValve->setOpen(false);
            if (aborted || done || failed) {
                return;
            }
            aborted = true;
            failed = true;
            opInFlow = inFlow->getLitersSinceReset() - inStartingLiters;
            opOutFlow = outFlow->getLitersSinceReset() - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
        }
};



class MetaFlowOp : public FlowOp {
    protected:        
        std::vector<FlowOp*> opSteps;
        int currentIndex = -1;
        FlowOp* currentOp = NULL;
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
            while (opSteps.size() > 0) {
                FlowOp* op = opSteps.back();
                op->abort();
                delete op;
                opSteps.pop_back();
            }
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
            float outLiters = outFlow->getLitersSinceReset();
            float inLiters = inFlow->getLitersSinceReset();
            opInFlow = inLiters - inStartingLiters;
            opOutFlow = outLiters - outStartingLiters;
            opTotalDeltaFlow = opInFlow - opOutFlow;
            if (currentOp->isDone()) {
                if (currentOp->isFailed()) {
                    dbg.println("meta op has a failed op");
                    errorMessage = "Sub operation " + currentOp->getType() + " failed: " + currentOp->getError();
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
        FlowValve* outValve;
    public:
        virtual String getType() { return FlowOpType::Empty; }
        EmptyOp(WaterLevel* waterLevel, FlowValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow)
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
            outValve->setOpen(false);
            MetaFlowOp::abort();
        }
        virtual ~EmptyOp() {}
};

class FlushOp : public MetaFlowOp {
    private:
        FlowValve* outValve;
        FlowValve* inValve;
    protected:
        using MetaFlowOp::opSteps;
    public:
        virtual String getType() { return FlowOpType::Flush; }
        FlushOp(WaterLevel* waterLevel, FlowValve* inValve, FlowValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300) 
            : MetaFlowOp(inFlow, outFlow) {
                this->inValve = inValve;
                this->outValve = outValve;
                params.push_back(flushes);
                params.push_back(rinseFillPercent);
                params.push_back(rinseSec);
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
            this->inValve->setOpen(false);
            this->outValve->setOpen(false);
            MetaFlowOp::abort();
        }
        virtual ~FlushOp() {}
};

class FillToPercentEnsured : public MetaFlowOp {
    private:
        FlowValve* inValve;
    public:
    virtual String getType() {return FlowOpType::FillToPercent;}
    FillToPercentEnsured(WaterLevel* waterLevel, FlowValve* inValve, FlowMeter* inFlow, FlowMeter* outFlow, float percent) 
        : MetaFlowOp(inFlow, outFlow) {
            this->inValve = inValve;
            this->params.push_back(percent);
            this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, percent));
            this->opSteps.push_back(new WaitOp(45000));
            this->opSteps.push_back(new FillToPercentOp(waterLevel, inValve, inFlow, outFlow, percent));
    }
    virtual void abort() {
        this->inValve->setOpen(false);
        MetaFlowOp::abort();
    }
    virtual ~FillToPercentEnsured() {}

};

#define DRAIN_OVERSHOOT_PERCENT_ESTIMATE 1.2
class DrainToPercentEnsured : public MetaFlowOp {
    private:
        FlowValve* outValve;
    public:
    virtual String getType() {return FlowOpType::DrainToPercent;}
    DrainToPercentEnsured(WaterLevel* waterLevel, FlowValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, float percent) 
        : MetaFlowOp(inFlow, outFlow) {
            this->outValve = outValve;
            this->params.push_back(percent);
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, percent + DRAIN_OVERSHOOT_PERCENT_ESTIMATE));
            this->opSteps.push_back(new WaitOp(30000));
            this->opSteps.push_back(new DrainToPercentOp(waterLevel, outValve, inFlow, outFlow, percent + (DRAIN_OVERSHOOT_PERCENT_ESTIMATE/1.5)));
    }
    virtual void abort() {
        this->outValve->setOpen(false);
        MetaFlowOp::abort();
    }
    virtual ~DrainToPercentEnsured() {}

};
class FlushAndFillToPercentOp : public MetaFlowOp {
      public:
        virtual String getType() { return FlowOpType::FlushAndFillToPercent; }
        FlushAndFillToPercentOp(WaterLevel* waterLevel, FlowValve* inValve, FlowValve* outValve, FlowMeter* inFlow, FlowMeter* outFlow, float fillToPercent, int flushes = 1, float rinseFillPercent = 20.0, int rinseSec = 300)
            : MetaFlowOp(inFlow, outFlow)
        {
            params.push_back(fillToPercent);
            params.push_back(flushes);
            params.push_back(rinseFillPercent);
            params.push_back(rinseSec);
            this->opSteps.push_back(new FlushOp(waterLevel, inValve, outValve, inFlow, outFlow, flushes, rinseFillPercent, rinseSec));
            this->opSteps.push_back(new FillToPercentEnsured(waterLevel, inValve, inFlow, outFlow, fillToPercent));
            
        }
        virtual ~FlushAndFillToPercentOp() {}

};

