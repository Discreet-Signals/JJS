/** BEGIN_JUCE_MODULE_DECLARATION

ID:            JuceJobSystem
vendor: DiscreetSignals
version: 0.0.1
name: Juce Job System
description: Job System wrapper for juce thread pool with dedicated schedule thread. Scheduler pushes job main function to thread pool, and on completion pushes callback functions to message thread. Callbacks can be added from any component on message thread and will run safely through RAII even if component / GUI is deconstructed.
website:
license: MIT

dependencies: juce_core

END_JUCE_MODULE_DECLARATION
*/

#pragma once
#include "Source/JobSystem.h"
