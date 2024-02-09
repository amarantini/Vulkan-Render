#include "input_controller.h"

StateBase * StateBase::event_handling_instance;

void StateBase::setEventHandling() { event_handling_instance = this; }

