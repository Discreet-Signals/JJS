/*
  ==============================================================================

    JobSystem.h
    Created: 10 May 2023 3:09:08pm
    Author:  Gavin

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "LockFreeFifo.h"
#include "Job.h"

namespace JJS
{

class JobQueue : public std::priority_queue<Job*>
{
public:
    ~JobQueue()
    {
        std::unique_ptr<Job> job = popJob();
        while(job != nullptr)
        {
            job.reset();
            job = popJob();
        }
    };
    void pushJob(std::unique_ptr<Job>&& job)
    {
        push(job.release());
    }
    std::unique_ptr<Job> popJob()
    {
        if(empty())
            return nullptr;
        
        std::unique_ptr<Job> job(top());
        pop();
        return job;
    }
};

template<typename JobSystem>
class SharedJobSystemPointer : public juce::SharedResourcePointer<JobSystem>
{
public:
    FunctionScope<void()> callbackScope;
    FunctionScope<void(float)> progressCallbackScope;
};

class JobSystem : juce::Thread, juce::Timer
{
public:
    /**
     Use Callbacks: This will run a jobs callback function on the main message thread, through a timer popping a FIFO. The jobs will lock as they are pushing their callbacks to the FIFO on job completion, so only enable callbacks if you need them!
     */
    JobSystem(const juce::String& scheduler_thread_name, int num_concurrent_jobs) : juce::Thread(scheduler_thread_name), threadPool(num_concurrent_jobs)
    {
        startThread(juce::Thread::Priority::highest);
        startTimer(2);
    }
    ~JobSystem() { stopSystem(); }
    
    /**
     This will flush out all jobs and callbacks in the pipe.
     */
    void flush()
    {
        abort.store(true);
        threadPool.removeAllJobs(true, 1000);
        finishedJobs.clear();
        abort.store(false);
    }
    /**
     This will terminate the thread, and flush out all jobs in the pipe. The callback timer will stop and terminated jobs won't finish.
     */
    void stopSystem()
    {
        stopThread(1000);
        stopTimer();
        flush();
        abort.store(true);
    }
    
    int size() { return threadPool.getNumThreads(); };
    
    void pushJob(std::unique_ptr<Job> job, ScopedFunctionContainer<void()>* callbacks = nullptr, ScopedFunctionContainer<void(float)>* progressCallbacks = nullptr)
    {
        job->linkSystem([&](){ return abort.load(); }, callbacks, progressCallbacks, [&](std::function<void()>&& progressCallback)
        {
            juce::ScopedLock lock(criticalSection);
            progressCallbackFIFO.push(std::move(progressCallback));
        });
        job->jobSetup();
        inputJobs.push(std::move(job));
    }
    
    void pushJob(std::unique_ptr<Job> job, const juce::Identifier& callback_id, const juce::Identifier& progress_callback_id = juce::Identifier())
    {
        JJS::ScopedFunctionContainer<void()>* callbacks = nullptr;
        JJS::ScopedFunctionContainer<void(float)>* progressCallbacks = nullptr;

        auto cit = callbackMap.find(callback_id);
        if (cit != callbackMap.end() && cit->second)
            callbacks = cit->second.get();

        if (progress_callback_id.isValid())
        {
            auto pit = progressCallbackMap.find(progress_callback_id);
            if (pit != progressCallbackMap.end() && pit->second)
            {
                progressCallbacks = pit->second.get();
            }
        }

        pushJob(std::move(job), callbacks, progressCallbacks);
    }
    
    void addCallback(const juce::Identifier& callback_id, FunctionScope<void()>* scope, std::function<void()>&& function)
    {
        if (callbackMap.find(callback_id) == callbackMap.end())
            callbackMap.emplace(callback_id, std::make_unique<JJS::ScopedFunctionContainer<void()>>());
        callbackMap[callback_id]->add(scope, std::move(function));
    }
    
    void addProgressCallback(const juce::Identifier& callback_id, FunctionScope<void(float)>* scope, std::function<void(float)>&& function)
    {
        if (progressCallbackMap.find(callback_id) == progressCallbackMap.end())
            progressCallbackMap.emplace(callback_id, std::make_unique<JJS::ScopedFunctionContainer<void(float)>>());
        progressCallbackMap[callback_id]->add(scope, std::move(function));
    }
    
    void triggerCallbacks(ScopedFunctionContainer<void()>* callbacks)
    {
        if (!callbacks)
            return;
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            callbacks->triggerFunctions();
        else
            finishedJobs.push(std::make_shared<Job>([](){}, [callbacks]()
            {
                callbacks->triggerFunctions();
            }));
    }
    
    void triggerCallbacks(const juce::Identifier& callback_id)
    {
        JJS::ScopedFunctionContainer<void()>* callbacks = nullptr;
        auto cit = callbackMap.find(callback_id);
        if (cit != callbackMap.end() && cit->second)
            callbacks = cit->second.get();
        triggerCallbacks(callbacks);
    }
    
private:
    bool processJobs()
    {
        // Prioritize Jobs
        while (inputJobs.getNumItems() > 0)
        {
            std::unique_ptr<Job> jobToPrioritize = inputJobs.pop();
            jobToPrioritize->queuePosition = queueCounter++;
            prioritizedJobs.pushJob(std::move(jobToPrioritize));
        }
        
        // Ensure JobSystem is Ready for New Job
        if (threadPool.getNumJobs() >= threadPool.getNumThreads() || prioritizedJobs.empty())
            return true;
        
        // Run Highest Priority Job
        std::unique_ptr<Job> jobToRun = prioritizedJobs.popJob();
        threadPool.addJob([&, job = std::shared_ptr<Job>(jobToRun.release())]() mutable
        {
            job->executeAction();
            if (abort.load())
                return;
            juce::ScopedLock lock(criticalSection);
            finishedJobs.push(job);
        });
        if (prioritizedJobs.empty())
            queueCounter = 0;
        
        return false;
    }
    
    void run() override
    {
        while (!threadShouldExit())
            if (processJobs())
                wait(2);
    }
    
    void timerCallback() override
    {
        while (progressCallbackFIFO.getNumItems() > 0)
            progressCallbackFIFO.pop()();
        while (finishedJobs.getNumItems() > 0)
            finishedJobs.pop()->executeCallback();
    }
    
    juce::ThreadPool threadPool;
    LockFreeFifo<std::unique_ptr<Job>> inputJobs { 2048 };
    JobQueue prioritizedJobs;
    std::vector<std::unique_ptr<Job>> runningJobs;
    LockFreeFifo<std::function<void()>> progressCallbackFIFO { 2048 };
    LockFreeFifo<std::shared_ptr<Job>> finishedJobs { 2048 };
    juce::CriticalSection criticalSection;
    long queueCounter { 0 };
    std::atomic<bool> abort { false };
    
    std::map<juce::Identifier, std::unique_ptr<JJS::ScopedFunctionContainer<void()>>> callbackMap;
    std::map<juce::Identifier, std::unique_ptr<JJS::ScopedFunctionContainer<void(float)>>> progressCallbackMap;
};

} // JJS
