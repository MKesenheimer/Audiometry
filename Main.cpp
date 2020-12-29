#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <time.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <complex>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <SDL_ttf.h>

#include "SDLTools/Utilities.h"
#include "SDLTools/Timer.h"
#include "SDLTools/CommandLineParser.h"

// TODO: Usage mit --help ausgeben
// TODO: Anzahl der Kommandozeilenargumente prüfen
// TODO: Messpunkte als CSV exportieren
// TODO: Signaltöne vor dem Start und beim Wechsel auf das andere Ohr

enum channel {
    stereo, left, right
};

void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y, int w, int h) {
    //Setup the destination rectangle to be at the position we want
    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    SDL_RenderCopy(ren, tex, NULL, &dst);
}

void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y) {
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    renderTexture(tex, ren, x, y, w, h);
}

void renderText(const std::string &message, const std::string &fontFile,
    SDL_Color color, int alpha, int fontSize, SDL_Renderer *renderer, int x, int y) {
    TTF_Font *font = TTF_OpenFont(fontFile.c_str(), fontSize);
    if(font == nullptr) {
        std::cout << "renderText: Could not open font file. " << TTF_GetError() << std::endl;
        return;
    }
    
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, message.c_str(), color);
    if(surf == nullptr) {
        std::cout << "renderText: Could not render font." << std::endl;
        return;
    }
    
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    if(texture == nullptr) {
        std::cout << "renderText: Could not create texture." << std::endl;
        return;
    }
    
    SDL_SetTextureAlphaMod(texture, alpha);
    renderTexture(texture, renderer, x, y);
    
    //Clean up the surface and font
    SDL_FreeSurface(surf);
    TTF_CloseFont(font);
    SDL_DestroyTexture(texture);
}

void plot(SDL_Renderer* renderer, std::vector<std::pair<int, int>> points, 
    int x1, int y1, int x2, int y2,
    int xmin, int ymin, int xmax, int ymax,
    int r = 0, int g = 0, int b = 0, int a = 255) {

    const int offset = 20;
    std::pair<int, int> old = {-1, -1};
    SDL_Color black = {0, 0, 0};   
    // coordinate System
    lineRGBA(renderer, x1 + 15, y1 + 20, x2 - 20, y1 + 20, 0, 0, 0, 255);
    lineRGBA(renderer, x1 + 20, y1 + 15, x1 + 20, y2 - 20, 0, 0, 0, 255);
    // x-arrow
    lineRGBA(renderer, x2 - 20, y1 + 20, x2 - 30, y1 + 10, 0, 0, 0, 255);
    lineRGBA(renderer, x2 - 20, y1 + 20, x2 - 30, y1 + 30, 0, 0, 0, 255);
    // y-arrow
    lineRGBA(renderer, x1 + 20, y2 - 20, x1 + 10, y2 - 30, 0, 0, 0, 255);
    lineRGBA(renderer, x1 + 20, y2 - 20, x1 + 30, y2 - 30, 0, 0, 0, 255);
    // text
    renderText("Freq [Hz]", "monaco.ttf", black, 255, 10, renderer, x2 - 53, y1 + 5);
    renderText("Gain [dB]", "monaco.ttf", black, 255, 10, renderer, x1 + 5, y2 - 20);

    // coordinate transformation
    float mx = (float)(x2 - x1) / (xmax - xmin);
    float bx = (float)(x1 * xmax - xmin * x2) / (xmax - xmin);
    float my = (float)(y2 - y1) / (ymax - ymin);
    float by = (float)(y1 * ymax - ymin * y2) / (ymax - ymin);
 
    for (const auto& [x, y] : points) {
        //int xs = (float)(mx * x + bx);
        int ys = (float)(my * y + by);
        int logx = (std::log(x) - 4.3) * 1500;
        int logxs = (float)(mx * logx + bx);
        ellipseRGBA(renderer, logxs, ys + offset, 5, 5, r, g, b, a);
        if (old.first != -1 && old.second != -1) {
            lineRGBA(renderer, logxs, ys + offset, old.first, old.second + offset, r, g, b, a);
        }
        // ticks
        lineRGBA(renderer, logxs, offset, logxs, offset - 5, 0, 0, 0, 255);
        // lines
        lineRGBA(renderer, logxs, offset, logxs, y2 - 50, 0, 0, 0, 50);
        // marks
        renderText(std::to_string(x), "monaco.ttf", black, 255, 10, renderer, logxs - 10, 5);
        old.first = logxs;
        old.second = ys;
    }

    int intervall = (y2 - y1) / 10;
    for (int i = y1; i < y2; i += intervall) {
        lineRGBA(renderer, x1 + 20, i, x2 - 30, i, 0, 0, 0, 50);
    }
}

void tone(SDL_AudioDeviceID* audio_device, SDL_AudioSpec* audio_spec, 
    float freq, float gain, float duration) {
    float t = 0;
    const int sample_size = (int)(audio_spec->freq * duration * 0.002f);
    int16_t sample[sample_size]; // TODO: use heap
    for (int i = 0; i < sample_size; i++) {
        float omega = 2.0f * M_PI * freq;
        t += 1.0f / audio_spec->freq;
        sample[i] = sin(omega * t) * gain;
    }

    SDL_QueueAudio(*audio_device, sample, sizeof(int16_t) * sample_size);
    SDL_PauseAudioDevice(*audio_device, 0);
}

void tone_cresc(SDL_AudioDeviceID* audio_device, SDL_AudioSpec* audio_spec, 
    float freq, float maxgain, float duration, channel ch = stereo) {
    float t = 0;
    const int sample_size = (int)(audio_spec->freq * duration * 0.001f * audio_spec->channels);
    //int16_t sample[sample_size];
    int16_t* sample = new int16_t[sample_size];

    float gain = 0;
    float increase = maxgain / sample_size;
    for (int i = 0; i < sample_size / 2; i++) {
        float omega = 2.0f * M_PI * freq;
        t += 1.0f / audio_spec->freq;
        gain += increase;
        if (ch == stereo) {
            sample[2 * i] = sin(omega * t) * gain; // left
            sample[2 * i + 1] = sin(omega * t) * gain; // right
        } else if (ch == left) {
            sample[2 * i] = sin(omega * t) * gain;
            sample[2 * i + 1] = 0;
        } else if (ch == right) {
            sample[2 * i] = 0;
            sample[2 * i + 1] = sin(omega * t) * gain;
        }
    }

    SDL_QueueAudio(*audio_device, sample, sizeof(int16_t) * sample_size);
    SDL_PauseAudioDevice(*audio_device, 0);
    delete[] sample;
}

int main(int argc, char* args[])
{
    // Parse the command line arguments
    std::string set("SDLDisplay");
    if (sdl::auxiliary::CommandLineParser::cmdOptionExists(args, args + argc, "-s"))
        set = static_cast<std::string>(sdl::auxiliary::CommandLineParser::getCmdOption(args, args + argc, "-s"));

    int SCREEN_WIDTH = 800;
    std::string width;
    if (sdl::auxiliary::CommandLineParser::cmdOptionExists(args, args + argc, "-w"))
        SCREEN_WIDTH = std::stoi(static_cast<std::string>(sdl::auxiliary::CommandLineParser::getCmdOption(args, args + argc, "-w")));

    int SCREEN_HEIGHT = 600;
    if (sdl::auxiliary::CommandLineParser::cmdOptionExists(args, args + argc, "-h"))
        SCREEN_HEIGHT = std::stoi(static_cast<std::string>(sdl::auxiliary::CommandLineParser::getCmdOption(args, args + argc, "-h")));

    int FRAMES_PER_SECOND = 20;
    if (sdl::auxiliary::CommandLineParser::cmdOptionExists(args, args + argc, "-f"))
        FRAMES_PER_SECOND = std::stoi(static_cast<std::string>(sdl::auxiliary::CommandLineParser::getCmdOption(args, args + argc, "-f")));

    // Timer zum festlegen der FPS
    sdl::auxiliary::Timer fps;
    // Timer zum errechnen der weltweit vergangenen Zeit
    sdl::auxiliary::Timer worldtime;
    worldtime.start();
    int frame = 0; // take records of frame number
    bool cap = true; // Framecap an oder ausschalten

    // SDL stuff
    // Setup our window and renderer, this time let's put our window in the center of the screen
    SDL_Window *window = SDL_CreateWindow("Audiometry", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        sdl::auxiliary::Utilities::logSDLError(std::cout, "CreateWindow");
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        sdl::auxiliary::Utilities::logSDLError(std::cout, "CreateRenderer");
        sdl::auxiliary::Utilities::cleanup(window);
        SDL_Quit();
        return 1;
    }

    // SDL INIT
    SDL_Init(SDL_INIT_AUDIO);
    // the representation of our audio device in SDL:
    SDL_AudioDeviceID audio_device;
    // opening an audio device:
    SDL_AudioSpec audio_spec;
    SDL_zero(audio_spec);
    audio_spec.freq = 44100;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 2; // stereo = 2
    audio_spec.samples = 4096;
    audio_spec.callback = NULL;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    // Initialize SDL_ttf
    if (TTF_Init() != 0) {
        std::cout << "Error in TTF_Init" << std::endl;
        return -1;
    }

    // measurements
    srand((unsigned) time(0));
    std::vector<std::pair<int, int>> pointsL = {{125, 0}, {250, 0}, {500, 0}, {750, 0}, {1000, 0}, {1500, 0}, {2000, 0}, {3000, 0}, {4000, 0}, {6000, 0}, {8000, 0}};
    std::vector<std::pair<int, int>> pointsR = {{125, 0}, {250, 0}, {500, 0}, {750, 0}, {1000, 0}, {1500, 0}, {2000, 0}, {3000, 0}, {4000, 0}, {6000, 0}, {8000, 0}};

    bool quit = false;
    bool reset = true;
    int choice = 0;
    signed int counter = -1;
    channel ch = left;
    SDL_Event e;
    while (!quit)
    {
        // start the fps timer
        fps.start();

        // handle events
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                quit = true;
            else if (e.type == SDL_KEYDOWN) {
                if(e.key.keysym.sym == SDLK_c)
                    cap = !cap;
                if (e.key.keysym.sym == SDLK_SPACE)
                    reset = true;
            }

        // increment the frame number
        frame++;
        // apply the fps cap
        if((cap == true) && (fps.getTicks() < 1000.f/FRAMES_PER_SECOND)) {
            SDL_Delay( (1000/FRAMES_PER_SECOND) - fps.getTicks() );
        }

        // every second
        if(worldtime.getTicks() > 1000) {
            // update the window caption
            std::stringstream caption;
            caption << "Audiometry, FPS = " << 1000.f*frame/worldtime.getTicks();
            SDL_SetWindowTitle(window, caption.str().c_str());
            worldtime.start();
            frame = 0;
        }

        //SDL_PauseAudioDevice(audio_device, 1);
        //SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev)
        if (ch == left) {
            if (reset) {
                counter++;
                if (counter < pointsL.size()) {
                    // choose a frequency randomly
                    do {
                        choice = (rand() % pointsL.size());
                    } while(pointsL[choice].second != 0);
                    SDL_ClearQueuedAudio(audio_device);
                    tone_cresc(&audio_device, &audio_spec, pointsL[choice].first, 1000, 120000, left);
                    //tone(&audio_device, &audio_spec, pointsL[choice].first, 1000, 10000);
                    reset = false;
                }
                if (counter == pointsL.size()) {
                    SDL_ClearQueuedAudio(audio_device);
                    // go to right channel
                    ch = right;
                    counter = -1;
                    reset = true;
                }
            }
            if (counter < pointsL.size()) {
                pointsL[choice].second++;
            }
        } else if (ch == right) {
            if (reset) {
                counter++;
                if (counter < pointsR.size()) {
                    // choose a frequency randomly
                    do {
                        choice = (rand() % pointsR.size());
                    } while(pointsR[choice].second != 0);
                    SDL_ClearQueuedAudio(audio_device);
                    tone_cresc(&audio_device, &audio_spec, pointsR[choice].first, 1000, 10000, right);
                    //tone(&audio_device, &audio_spec, pointsL[choice].first, 1000, 1000);
                    reset = false;
                }
                if (counter == pointsR.size()) {
                    SDL_ClearQueuedAudio(audio_device);
                }
            }
            if (counter < pointsR.size()) {
                pointsR[choice].second++;
            }
        }

        //Rendering
        SDL_RenderClear(renderer);
        //Draw the background white
        boxRGBA(renderer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 255, 255, 255, 255);
        
#define OVERLAY
#ifdef OVERLAY
        // overlay
        plot(renderer, pointsL, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 
            0, 0, 8000, 500, 255, 0, 0, 255);
        plot(renderer, pointsR, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
            0, 0, 8000, 500, 0, 0, 255, 255);
#else
        // draw graphs (beside each other)
        plot(renderer, pointsL, 0, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT * 5 / 6.0f, 
            0, 0, 8000, 500, 255, 0, 0, 255);
        plot(renderer, pointsR, SCREEN_WIDTH / 2, 0, SCREEN_WIDTH, SCREEN_HEIGHT * 5 / 6.0f,
            0, 0, 8000, 500, 0, 0, 255, 255);
#endif

        SDL_RenderPresent(renderer);
    }

    //Destroy the various items
    SDL_CloseAudioDevice(audio_device);
    sdl::auxiliary::Utilities::cleanup(renderer, window);
    SDL_Quit();
    TTF_Quit();

    return 0;
}
