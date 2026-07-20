#include "error_check.h"

#include <stdlib.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_error.h>

void checkStatus(bool status) {
    if (!status) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
        abort();
    }
}

void checkKTX(ktxResult result) {
    if (result != KTX_SUCCESS) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "%s\nKTX error: %s",
            SDL_GetError(),
            ktxErrorString(result)
        );
        abort();
    }
}