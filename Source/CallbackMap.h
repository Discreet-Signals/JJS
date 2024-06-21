/*
  ==============================================================================

    CallbackMap.h
    Created: 20 Jun 2024 3:02:42pm
    Author:  Gavin

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "ScopeTrackedFunctions.h"

namespace JJS
{

template <typename fn = void()>
class CallbackMap
{
public:
    void add(const juce::Identifier& callback_id, FunctionScope<void()>* scope, std::function<void()>&& function)
    {
        if (map.find(callback_id) == map.end())
            map.emplace(callback_id, std::make_unique<JJS::ScopedFunctionContainer<void()>>());
        map[callback_id]->add(scope, std::move(function));
    }
    template <typename... Args>
    void trigger(const juce::Identifier& callback_id, Args... args)
    {
        auto it = map.find(callback_id);
        if (it != map.end())
            if (it->second)
                it->second->triggerFunctions(args...);
    }
private:
    std::map<juce::Identifier, std::unique_ptr<JJS::ScopedFunctionContainer<fn>>> map;
};

} // JJS
