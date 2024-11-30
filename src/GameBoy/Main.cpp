#define SDL_MAIN_HANDLED

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>

#include "APU.hpp"
#include "Cartridge.hpp"
#include "Controller.hpp"
#include "ErrorHandling.hpp"
#include "FileDialog.hpp"
#include "GB.hpp"
#include "LocaleInitializer.hpp"
#include "PPU.hpp"
#include "SDLUtils.hpp"
#include "SM83.hpp"

int main() {
    try {

        LocaleInitializer localeInit;

        std::string romPath = OpenFileDialog();
        if (romPath.empty()) {
            ShowInfoMessage(L"Aucun fichier ROM sélectionné. Fermeture de l'application.", L"Information");
            return EXIT_SUCCESS;
        }

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
            throw std::runtime_error(std::string("Échec de l'initialisation de SDL: ") + SDL_GetError());
        }

        struct SDL_Quit_Scope {
            ~SDL_Quit_Scope() { SDL_Quit(); }
        } sdl_quit_scope;

        SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "best");

        auto controller = InitializeController();

        std::unique_ptr<Cartridge, decltype(&destroyCartridge)> cart(createCartridge(romPath.c_str()), &destroyCartridge);
        if (!cart) {
            throw std::runtime_error("Erreur lors du chargement de la ROM");
        }

        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            SDL_CreateWindow("Émulateur Game Boy",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                4 * SCREEN_WIDTH, 4 * SCREEN_HEIGHT,
                SDL_WINDOW_RESIZABLE),
            SDL_DestroyWindow
        );
        if (!window) {
            throw std::runtime_error(std::string("Échec de la création de la fenêtre SDL: ") + SDL_GetError());
        }

        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
            SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
            SDL_DestroyRenderer
        );
        if (!renderer) {
            throw std::runtime_error(std::string("Échec de la création du renderer SDL: ") + SDL_GetError());
        }

        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
            SDL_CreateTexture(renderer.get(),
                SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT),
            SDL_DestroyTexture
        );
        if (!texture) {
            throw std::runtime_error(std::string("Échec de la création de la texture SDL: ") + SDL_GetError());
        }

        SDL_AudioSpec desiredSpec = {};
        desiredSpec.freq = APUConstants::SAMPLE_FREQ;
        desiredSpec.format = AUDIO_F32;
        desiredSpec.channels = 2;
        desiredSpec.samples = APUConstants::SAMPLE_BUF_LEN / 2;

        SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, nullptr, 0);
        if (audioDevice == 0) {
            throw std::runtime_error("Échec de l'ouverture du périphérique audio");
        }
        SDL_PauseAudioDevice(audioDevice, 0);

        std::unique_ptr<GameBoy, decltype(&free)> gbSystem(
            reinterpret_cast<GameBoy*>(calloc(1, sizeof(GameBoy))),
            &free
        );
        if (!gbSystem) {
            throw std::runtime_error("Erreur d'allocation mémoire pour le système GB");
        }

        resetGameBoy(gbSystem.get(), cart.get());
        gbSystem->renderer = renderer.get();

        bool running = true;
        long currentCycle = 0, frame = 0;
        double fps = 0.0;
        auto lastTitleUpdate = std::chrono::steady_clock::now();
        int framesSinceLastUpdate = 0;

        int windowW, windowH;
        SDL_GetWindowSize(window.get(), &windowW, &windowH);
        SDL_Rect dst = { 0, 0, windowW, windowH };

        while (running) {

            if (gbSystem->CPU.illegalOpcode) {
                std::cerr << "Instruction illégale détectée, arrêt du programme\n";
                break;
            }

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }

                else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    windowW = event.window.data1;
                    windowH = event.window.data2;

                    if (windowW * SCREEN_HEIGHT > windowH * SCREEN_WIDTH) {
                        dst.h = windowH;
                        dst.w = (windowH * SCREEN_WIDTH) / SCREEN_HEIGHT;
                        dst.x = (windowW - dst.w) / 2;
                        dst.y = 0;
                    }
                    else {
                        dst.w = windowW;
                        dst.h = (windowW * SCREEN_HEIGHT) / SCREEN_WIDTH;
                        dst.x = 0;
                        dst.y = (windowH - dst.h) / 2;
                    }
                }

                handleGameBoyEvent(gbSystem.get(), &event);
            }

            void* pixels = nullptr;
            int frameBufferPitch = 0;
            if (SDL_LockTexture(texture.get(), nullptr, &pixels, &frameBufferPitch) != 0) {
                throw std::runtime_error(std::string("Échec de verrouillage de la texture SDL: ") + SDL_GetError());
            }

            gbSystem->ppu.frameBuffer = static_cast<uint32_t*>(pixels);
            gbSystem->ppu.frameBufferPitch = frameBufferPitch;

            while (!gbSystem->ppu.isFrameComplete) {
                emulateCycle(gbSystem.get());

                if (gbSystem->apu.isAudioBufferFull) {
                    SDL_QueueAudio(audioDevice, gbSystem->apu.audioSampleBuffer.data(), sizeof(float) * APUConstants::SAMPLE_BUF_LEN);
                    gbSystem->apu.isAudioBufferFull = false;
                }
                ++currentCycle;
            }
            gbSystem->ppu.isFrameComplete = false;

            SDL_UnlockTexture(texture.get());

            SDL_RenderClear(renderer.get());

            SDL_RenderCopy(renderer.get(), texture.get(), nullptr, &dst);

            SDL_RenderPresent(renderer.get());

            ++frame;
            ++framesSinceLastUpdate;

            while (SDL_GetQueuedAudioSize(audioDevice) > 4 * APUConstants::SAMPLE_BUF_LEN) {
                SDL_Delay(1);
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsedSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTitleUpdate).count();

            if (elapsedSinceLastUpdate >= 1000) {
                fps = framesSinceLastUpdate * 1000.0 / elapsedSinceLastUpdate;
                framesSinceLastUpdate = 0;
                lastTitleUpdate = now;

                std::string title = "Émulateur Game Boy | " + std::to_string(static_cast<int>(fps)) + " FPS";
                SDL_SetWindowTitle(window.get(), title.c_str());
            }
        }

        SDL_CloseAudioDevice(audioDevice);
    }
    catch (const std::exception& e) {

        std::wstring errorMsg = L"Erreur : ";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, NULL, 0);
        std::wstring wstrError(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, &wstrError[0], size_needed);
        errorMsg += wstrError;

        ShowErrorMessage(errorMsg, L"Erreur");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

