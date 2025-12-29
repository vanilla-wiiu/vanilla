#include "InputGamepad.h"
#include <SDL3/SDL.h>

/*
  These functions are expected to already exist
  in your pipe / backend code.
*/
void Pipe_SendButton(int button, int pressed);
void Pipe_SendAxis(int axis, int value);

void InputGamepad_Init(void)
{
    SDL_Log("InputGamepad initialized");
}

void InputGamepad_Handle(const SDL_Event *e)
{
    switch (e->type) {

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            Pipe_SendButton(
                e->gbutton.button,
                e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
            );
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            Pipe_SendAxis(
                e->gaxis.axis,
                e->gaxis.value
            );
            break;

        default:
            break;
    }
}
