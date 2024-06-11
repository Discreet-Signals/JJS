/*
  ==============================================================================

    ScopeTrackedFunctions.h
    Created: 17 Jun 2023 4:43:42pm
    Author:  Gavin

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

namespace JJS
{

/**
 Namespace for RAII protected functions.
 Lambdas with reference captures can be moved around / pushed into a container.
 ScopedFunctions will be removed from the ScopedFunctionContainer when their FunctionScope deconstructs!
 
 Usage:
 1. Create a ScopedFunctionContainer on your long life object.
 2. Create a FunctionScope in your short life object.
 3. Push a lambda with reference captured short life object to the ScopedFunctionContainer, alongside its FunctionScope.
 4. Worry no more! Your long life object won't access the short life object if it goes out of scope!
 */
namespace ScopeTrackedFunctions
{

template <typename fn = void()>
class ScopedFunctionContainer;

template <typename fn = void()>
class ScopedFunction
{
public:
    ScopedFunction(const ScopedFunctionContainer<fn>* _container, const std::function<fn>&& _function) : container(_container), function(_function) { }
    const ScopedFunctionContainer<fn>* container;
    const std::function<fn> function;
};

template <typename fn = void()>
class FunctionScope
{
public:
    FunctionScope() { DBG("Scope Created: " << std::to_string(reinterpret_cast<uintptr_t>(this))); }
    ~FunctionScope();
    
private:
    friend class ScopedFunctionContainer<fn>;
    std::vector<ScopedFunctionContainer<fn>*> containers;
    std::vector<ScopedFunction<fn>> scopedFunctions;
};

template <typename fn>
class ScopedFunctionContainer
{
public:
    ScopedFunctionContainer() = default;
    void add(FunctionScope<fn>* scope, std::function<fn>&& function)
    {
        juce::ScopedLock lock(criticalSection);
        addFunctionToScope(scope, std::move(function));
        registerContainerToScope(scope);
    }
    void remove(FunctionScope<fn>* scope)
    {
        juce::ScopedLock lock(criticalSection);
        auto iterator = std::remove_if(scopes.begin(), scopes.end(), [scope](FunctionScope<fn>* s)
                                       {
            bool removed = s == scope;
            if (removed)
                DBG("Removed Scope: " << std::to_string(reinterpret_cast<uintptr_t>(scope)));
            return s == scope;
        });
        scopes.erase(iterator, scopes.end());
        DBG("Scopes: " << scopes.size());
    }
    template<typename... Args>
    void triggerFunctions(Args... args) const
    {
        juce::ScopedLock lock(criticalSection);
        for (FunctionScope<fn>* scope : scopes)
            for (const ScopedFunction<fn>& scopedFunction : scope->scopedFunctions)
                if (scopedFunction.container == this)
                    scopedFunction.function(args...);
    }
    int getNumFunctions() const
    {
        int num = 0;
        for (FunctionScope<fn>* scope : scopes)
            num += scope->scopedFunctions.size();
        return num;
    }
    int getNumScopes() const { return static_cast<int>(scopes.size()); }
private:
    void addFunctionToScope(FunctionScope<fn>* scope, std::function<fn>&& function)
    {
        ScopedFunction<fn> scopedFunction(this, std::move(function));
        scope->scopedFunctions.push_back(std::move(scopedFunction));
    }
    void registerContainerToScope(FunctionScope<fn>* scope)
    {
        auto iterator = std::find(scopes.begin(), scopes.end(), scope);
        if (iterator == scopes.end())
        {
            scope->containers.push_back(this);
            scopes.push_back(scope);
        }
    }
    std::vector<FunctionScope<fn>*> scopes;
    juce::CriticalSection criticalSection;
};

template <typename fn>
FunctionScope<fn>::~FunctionScope()
{
    for (ScopedFunctionContainer<fn>* container : containers)
        container->remove(this);
    DBG("Scope Destroyed: " << std::to_string(reinterpret_cast<uintptr_t>(this)));
};

} // ScopeTrackedFunctions

} // JJS
