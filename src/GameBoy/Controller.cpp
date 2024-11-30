#include "Controller.hpp"
#include "ErrorHandling.hpp"
#include "LocaleInitializer.hpp"
#include <windows.h>
#include <iostream>

SDLControllerPtr InitializeController() {
    SDLControllerPtr controller(nullptr, SDLDestroyer());

    if (SDL_NumJoysticks() > 0) {
        controller.reset(SDL_GameControllerOpen(0));
            if (controller) {
                std::cout << "Manette connectée -> " << SDL_GameControllerName(controller.get()) << std::endl;
            }
            else {
                std::wstring errorMsg = L"Échec de l'ouverture de la manette : ";
                std::wstring sdlError = std::wstring(SDL_GetError(), SDL_GetError() + strlen(SDL_GetError()));
                errorMsg += sdlError;
                ShowErrorMessage(errorMsg, L"Erreur");
                SDL_Quit();
                exit(EXIT_FAILURE);
            }
    }
    else {
        ShowWarningMessage(L"Merci de connecter une manette pour pouvoir jouer.", L"Aucune manette détectée");
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    return controller;
}
