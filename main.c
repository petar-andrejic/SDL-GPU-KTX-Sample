#include "app_state.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    *appstate = Appstate_create();
    if (!*appstate) return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState_draw(appstate);
    return SDL_APP_CONTINUE;
};

bool isQuitEvent(SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_QUIT:
            return true;
        default:
            return false;
    }
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (isQuitEvent(event)) {
        return SDL_APP_SUCCESS;
    }

    Appstate_handleEvents(appstate, event);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (result == SDL_APP_SUCCESS) {
        Appstate_destroy(appstate);
    }
}
