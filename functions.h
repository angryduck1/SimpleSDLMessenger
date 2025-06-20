#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <SDL2/SDL_mixer.h>

using namespace std;

class Menu {
private:
    SDL_Texture* texture;
    SDL_Rect dstrect;
public:
    void InititTexture(SDL_Texture* texture, SDL_Rect dstrect) {
        this->texture = texture;
        this->dstrect = dstrect;
    }

    int PressToPlay() {
        int x;
        int y;

        SDL_GetMouseState(&x, &y);

        if (x >= dstrect.x && x < dstrect.x + dstrect.w &&
            y >= dstrect.y && y < dstrect.y + dstrect.h)
        {
            return 0;
        } else {
            return 1;
        }
    }
};

int renderText(const std::string message, const std::string path_to_font, SDL_Color color, int fontSize, SDL_Renderer *renderer, int x, int y) {
    TTF_Font *font = TTF_OpenFont(path_to_font.c_str(), fontSize);

    if (font == nullptr) {
        std::cout << "Error open font: " << TTF_GetError() << std::endl;
        return 1;
    }

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, message.c_str(), color);

    if (surf == nullptr) {
        TTF_CloseFont(font);
        std::cout << "Error create surf: " << TTF_GetError() << std::endl;
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);

    if (texture == nullptr) {
        std::cout << "Error create texture: " << TTF_GetError() << std::endl;
        return 1;
    }

    SDL_FreeSurface(surf);
    TTF_CloseFont(font);

    SDL_Rect dstrect = {x, y};
    SDL_QueryTexture(texture, nullptr, nullptr, &dstrect.w, &dstrect.h);
    SDL_RenderCopy(renderer, texture, nullptr, &dstrect);

    SDL_DestroyTexture(texture);

    return 0;
}

SDL_Texture* createFontTexture(const std::string message, const std::string path_to_font, SDL_Color color, int fontSize, SDL_Renderer *renderer) {
    TTF_Font *font = TTF_OpenFont(path_to_font.c_str(), fontSize);

    if (font == nullptr) {
        std::cout << "Error open font: " << TTF_GetError() << std::endl;
        return nullptr;
    }

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, message.c_str(), color);

    if (surf == nullptr) {
        TTF_CloseFont(font);
        std::cout << "Error create surf: " << TTF_GetError() << std::endl;
        return nullptr;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);

    if (texture == nullptr) {
        std::cout << "Error create texture: " << TTF_GetError() << std::endl;
        return nullptr;
    }

    SDL_FreeSurface(surf);
    TTF_CloseFont(font);

    return texture;
}

void play_music(Mix_Music* music) {
    if (music && Mix_PlayMusic(music, -1) == -1) {
        cerr << "Failed to play music: " << Mix_GetError() << endl;
    }
}

void clear(SDL_Renderer *&renderer, SDL_Window *&window) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    Mix_CloseAudio();
    SDL_Quit();
}


#endif
