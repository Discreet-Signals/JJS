/*
  ==============================================================================

    Job.h
    Created: 16 Jun 2023 5:50:40pm
    Author:  Gavin

  ==============================================================================
*/

#pragma once
#include "ScopeTrackedFunctions.h"

namespace JJS
{
using namespace ScopeTrackedFunctions;

class Job
{
public:
    enum Priority
    {
        Normal,
        Urgent
    };
    
    Job(std::function<void()>&& job_action, std::function<void()>&& job_callback = std::function<void()>(), Priority job_priority = Priority::Normal)
    : action(std::move(job_action)), callback(std::move(job_callback)), priority(job_priority) {}
    virtual ~Job() { }
    virtual void jobSetup() { }
    virtual void jobAction() { };
    virtual void jobCallback() { };
    
    bool operator<(const Job& other) const
    {
        if (priority == other.priority)
            return queuePosition > other.queuePosition;  // Lower position (pushed earlier) is considered "greater"
        return priority < other.priority; // Higher priority is considered "greater"
    }
protected:
    Job(Job::Priority job_priority = Job::Priority::Normal) : priority(job_priority) { }
    
    bool shouldAbort() { if (shouldAbortFn) return shouldAbortFn(); return false; }
    void executeUpdate(float progress)
    {
        if (sendUpdateFn && scopedProgressCallbacks)
            sendUpdateFn([progressCallbacks = scopedProgressCallbacks, progress]()
                         {
                if (progressCallbacks) progressCallbacks->triggerFunctions(progress);
            });
    }
private:
    friend class JobSystem;
    void executeAction()
    {
        executeUpdate(0);
        jobAction();
        if (action) action();
    };
    void executeCallback()
    {
        jobCallback();
        if(callback) callback();
        if (scopedCallbacks) scopedCallbacks->triggerFunctions();
    };
    void linkSystem(std::function<bool()>&& shouldAbortFN, ScopedFunctionContainer<void()>* callbacks, ScopedFunctionContainer<void(float)>* progressCallbacks, std::function<void(std::function<void()>&&)>&& sendUpdateFN)
    {
        shouldAbortFn = shouldAbortFN;
        scopedCallbacks = callbacks;
        scopedProgressCallbacks = progressCallbacks;
        sendUpdateFn = std::move(sendUpdateFN);
    }
    std::function<void()> action;
    std::function<void()> callback;
    
    Priority priority { Priority::Normal };
    long queuePosition { 0 };
    
    std::function<bool()> shouldAbortFn;
    std::function<void(std::function<void()>&&)> sendUpdateFn;
    ScopedFunctionContainer<void()>* scopedCallbacks { nullptr };
    ScopedFunctionContainer<void(float)>* scopedProgressCallbacks { nullptr };
};

} // JJS
