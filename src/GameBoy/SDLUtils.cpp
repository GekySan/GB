#include "SDLUtils.hpp"

void SDLDestroyer::operator()(SDL_Window* window) const {
    if (window) SDL_DestroyWindow(window);
}

void SDLDestroyer::operator()(SDL_Renderer* renderer) const {
    if (renderer) SDL_DestroyRenderer(renderer);
}

void SDLDestroyer::operator()(SDL_Texture* texture) const {
    if (texture) SDL_DestroyTexture(texture);
}

void SDLDestroyer::operator()(SDL_AudioDeviceID* audio) const {
    if (audio) {
        SDL_CloseAudioDevice(*audio);
        delete audio;
    }
}

void SDLDestroyer::operator()(SDL_GameController* controller) const {
    if (controller) SDL_GameControllerClose(controller);
}

void checkSDLInit(int result, const std::string& errorMessage) {
    if (result != 0) {
        throw std::runtime_error(errorMessage + ": " + SDL_GetError());
    }
}
