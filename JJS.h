/** BEGIN_JUCE_MODULE_DECLARATION

ID: JJS
vendor: DiscreetSignals
version: 1.0.1
name: Juce Job System
description: Job System wrapper for juce thread pool with dedicated schedule thread. Scheduler pushes job main function to thread pool, and on completion pushes callback functions to message thread. Callbacks can be added from any component on message thread and will run safely through RAII even if component / GUI is deconstructed.
website:
license: MIT

dependencies: juce_core, juce_events

END_JUCE_MODULE_DECLARATION
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <JuceHeader.h>
#include "Source/JobSystem.h"
#include "Source/CallbackMap.h"
