#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <stdexcept>

struct SDLDestroyer {
    void operator()(SDL_Window* window) const;
    void operator()(SDL_Renderer* renderer) const;
    void operator()(SDL_Texture* texture) const;
    void operator()(SDL_AudioDeviceID* audio) const;
    void operator()(SDL_GameController* controller) const;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLDestroyer>;
using SDLRendererPtr = std::unique_ptr<SDL_Renderer, SDLDestroyer>;
using SDLTexturePtr = std::unique_ptr<SDL_Texture, SDLDestroyer>;
using SDLAudioPtr = std::unique_ptr<SDL_AudioDeviceID, SDLDestroyer>;
using SDLControllerPtr = std::unique_ptr<SDL_GameController, SDLDestroyer>;

void checkSDLInit(int result, const std::string& errorMessage);
