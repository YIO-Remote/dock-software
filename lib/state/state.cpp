#include "state.h"

State* State::s_instance = nullptr;

State::State()
{
    s_instance = this;
}