#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <string>

//-------------------------------------------------------
//                       CONSTANTS
//-------------------------------------------------------
const int SCREEN_WIDTH       = 800;
const int SCREEN_HEIGHT      = 600;
const int GRID_SIZE          = 20;

// Default snake speed (ms per movement).
const int DEFAULT_SNAKE_SPEED = 100;

// Grass wave defaults.
const float DEFAULT_GRASS_WAVE_SPEED     = 0.05f;  
const float DEFAULT_GRASS_WAVE_AMPLITUDE = 15.0f; 

// Flashlight game mode: how many grid blocks from snake head are visible.
const int FLASHLIGHT_RADIUS_BLOCKS = 5;

// Enumeration of possible game states.
enum class GameState {
    MAIN_MENU,
    CONFIG_MENU,
    MODE_MENU,
    PLAYING,
    PAUSED,
    QUIT
};

// Enumeration of some possible config items.
enum class ConfigOption {
    SPEED,
    NUM_FOOD,
    NUM_OBSTACLES,
    AMPLITUDE,
    WAVE_SPEED,
    EXIT
};

// Enumeration for game modes.
enum class GameMode {
    NORMAL,
    FLASHLIGHT
};

struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct GrassBlade {
    float x;
    float y;
    float height;
    float waveOffset;
    float randomAmplitude; // Varied amplitude for each blade
};

struct Sparkle {
    float x, y;
    float life;
};

class Application {
public:
    Application() 
        : window(nullptr),
          renderer(nullptr),
          font(nullptr),
          scoreFont(nullptr),
          collisionSound(nullptr),
          eatsound(nullptr),
          state(GameState::MAIN_MENU),
          gameMode(GameMode::NORMAL),
          direction({1, 0}),
          lastMoveTime(0),
          running(true),
          score(0),
          highScore(0),
          animationTime(0.0f),
          selectedOption(0),
          pauseMenuOption(0),
          configOption(0),
          modeMenuOption(0),
          snakeSpeed(DEFAULT_SNAKE_SPEED),
          numFoodItems(10),
          numObstacles(15),
          grassWaveSpeed(DEFAULT_GRASS_WAVE_SPEED),
          grassWaveAmplitude(DEFAULT_GRASS_WAVE_AMPLITUDE)
    {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

        window = SDL_CreateWindow(
            "Snake + Procedural Grass + Flashlight Mode",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN
        );

        // Use accelerated rendering if available.
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        font      = TTF_OpenFont("COMIC.TTF", 24);
        scoreFont = TTF_OpenFont("COMIC.TTF", 28);
        collisionSound = Mix_LoadWAV("colision.wav"); // Provide your .wav file
        eatsound = Mix_LoadWAV("eat.wav"); // Provide your .wav file

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        generateGrass();
    }

    ~Application() {
        if (scoreFont) {
            TTF_CloseFont(scoreFont);
            scoreFont = nullptr;
        }
        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        if (collisionSound) {
            Mix_FreeChunk(collisionSound);
        }
        if (eatsound) {
            Mix_FreeChunk(eatsound);
        }
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
    }

    // Main application loop.
    void run() {
        while (running && state != GameState::QUIT) {
            handleEvents();
            update();
            render();
            SDL_Delay(1); 
        }
    }

private:
    //---------------------------------------------------
    //               SDL MEMBERS & GAME STATE
    //---------------------------------------------------
    SDL_Window*   window;
    SDL_Renderer* renderer;
    TTF_Font*     font;
    TTF_Font*     scoreFont;
    Mix_Chunk*    collisionSound;
    Mix_Chunk*    eatsound;

    GameState state;
    GameMode  gameMode;
    bool      running;

    //---------------------------------------------------
    //               SNAKE & GAMEPLAY VARIABLES
    //---------------------------------------------------
    std::vector<Point> snake;
    std::vector<Point> foodItems;
    std::vector<Point> obstacles;  
    Point  direction;
    Uint32 lastMoveTime;
    int    score;
    int    highScore;

    //---------------------------------------------------
    //           PROCEDURAL GRASS & TIMING
    //---------------------------------------------------
    float animationTime;
    std::vector<GrassBlade> grassBlades;
    std::vector<Sparkle> sparkles;

    //---------------------------------------------------
    //           MENU & CONFIG VARIABLES
    //---------------------------------------------------
    int selectedOption;    // main menu
    int pauseMenuOption;   // pause menu
    int configOption;      // config menu
    int modeMenuOption;    // mode menu (for gameMode selection)

    // Configurable options.
    int   snakeSpeed;          // Lower value => faster snake
    int   numFoodItems;
    int   numObstacles;
    float grassWaveSpeed;
    float grassWaveAmplitude;

    //---------------------------------------------------
    //                    EVENTS
    //---------------------------------------------------
    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                state   = GameState::QUIT;
            } else if (event.type == SDL_KEYDOWN) {
                onKeyDown(event.key.keysym.sym);
            }
        }
    }

    void onKeyDown(SDL_Keycode key) {
        switch (key) {
            case SDLK_ESCAPE: {
                if (state == GameState::PLAYING) {
                    state = GameState::PAUSED;
                } else if (state == GameState::PAUSED) {
                    state = GameState::PLAYING;
                } else if (state == GameState::CONFIG_MENU || 
                           state == GameState::MODE_MENU) {
                    state = GameState::MAIN_MENU;
                }
                break;
            }
            case SDLK_UP:
            case SDLK_w:
                if (state == GameState::MAIN_MENU) {
                    // Suppose we have 4 items. We cycle upward
                    selectedOption = (selectedOption + 3) % 4; 
                } else if (state == GameState::PLAYING && direction.y == 0) {
                    direction = {0, -1};
                } else if (state == GameState::PAUSED) {
                    pauseMenuOption = (pauseMenuOption == 0) ? 1 : 0;
                } else if (state == GameState::CONFIG_MENU) {
                    configOption = (configOption == 0) 
                        ? (int)ConfigOption::EXIT 
                        : configOption - 1;
                } else if (state == GameState::MODE_MENU) {
                    // Only 2 modes: cycle between them
                    modeMenuOption = (modeMenuOption == 0) ? 1 : 0;
                }
                break;
            case SDLK_DOWN:
            case SDLK_s:
                if (state == GameState::MAIN_MENU) {
                    selectedOption = (selectedOption + 1) % 4;
                } else if (state == GameState::PLAYING && direction.y == 0) {
                    direction = {0, 1};
                } else if (state == GameState::PAUSED) {
                    pauseMenuOption = (pauseMenuOption == 0) ? 1 : 0;
                } else if (state == GameState::CONFIG_MENU) {
                    configOption = (configOption + 1) 
                        % ((int)ConfigOption::EXIT + 1);
                } else if (state == GameState::MODE_MENU) {
                    // Also just 2 modes, cycle
                    modeMenuOption = (modeMenuOption == 0) ? 1 : 0;
                }
                break;
            case SDLK_LEFT:
            case SDLK_a:
                if (state == GameState::PLAYING && direction.x == 0) {
                    direction = {-1, 0};
                } else if (state == GameState::CONFIG_MENU) {
                    adjustConfigOption(false);
                }
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                if (state == GameState::PLAYING && direction.x == 0) {
                    direction = {1, 0};
                } else if (state == GameState::CONFIG_MENU) {
                    adjustConfigOption(true);
                }
                break;
            case SDLK_RETURN:
                if (state == GameState::MAIN_MENU) {
                    mainMenuSelection();
                } else if (state == GameState::PAUSED) {
                    if (pauseMenuOption == 0) {
                        state = GameState::PLAYING; 
                    } else {
                        state = GameState::MAIN_MENU;
                    }
                } else if (state == GameState::MODE_MENU) {
                    modeSelection();
                }
                break;
            default:
                break;
        }
    }

    //---------------------------------------------------
    //              MAIN MENU SELECTION
    //---------------------------------------------------
    void mainMenuSelection() {
        // Let's define 4 main options:
        // 0: Start Game
        // 1: Config
        // 2: Game Mode
        // 3: Quit
        switch (selectedOption) {
            case 0:
                // Start game with current config.
                startGame();
                break;
            case 1:
                // Go to config menu.
                state = GameState::CONFIG_MENU;
                break;
            case 2:
                // Go to game mode menu.
                state = GameState::MODE_MENU;
                break;
            case 3:
                // Quit
                state = GameState::QUIT;
                break;
        }
    }

    //---------------------------------------------------
    //            CONFIG MENU SELECTION
    //---------------------------------------------------
    void adjustConfigOption(bool increase) {
        switch ((ConfigOption)configOption) {
            case ConfigOption::SPEED:
                if (increase) {
                    // Decrease snakeSpeed to speed up the snake
                    snakeSpeed = (snakeSpeed > 1) ? snakeSpeed - 1 : -20;
                } else {
                    // Increase snakeSpeed to slow down
                    snakeSpeed += 1;
                }
                break;
            case ConfigOption::NUM_FOOD:
                if (increase) numFoodItems++;
                else if (numFoodItems > 0) numFoodItems--;
                break;
            case ConfigOption::NUM_OBSTACLES:
                if (increase) numObstacles++;
                else if (numObstacles > 0) numObstacles--;
                break;
            case ConfigOption::AMPLITUDE:
                if (increase) grassWaveAmplitude += 1.f;
                else grassWaveAmplitude = std::max(0.0f, grassWaveAmplitude - 1.f);
                break;
            case ConfigOption::WAVE_SPEED:
                if (increase) grassWaveSpeed += 0.01f;
                else grassWaveSpeed = std::max(0.0f, grassWaveSpeed - 0.01f);
                break;
            case ConfigOption::EXIT:
                // Exit config menu
                state = GameState::MAIN_MENU;
                break;
        }
    }

    //---------------------------------------------------
    //                GAME MODE MENU
    //---------------------------------------------------
    void modeSelection() {
        // 0 -> NORMAL, 1 -> FLASHLIGHT
        if (modeMenuOption == 0) {
            gameMode = GameMode::NORMAL;
        } else {
            gameMode = GameMode::FLASHLIGHT;
        }
        state = GameState::MAIN_MENU;
    }

    //---------------------------------------------------
    //             GAME UPDATE & LOGIC
    //---------------------------------------------------
    void update() {
        animationTime += 0.02f;

        if (state != GameState::PLAYING) {
            return;
        }
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastMoveTime < (Uint32)snakeSpeed) {
            return;
        }
        lastMoveTime = currentTime;

        if (!snake.empty()) {
            Point newHead = {
                snake[0].x + direction.x * GRID_SIZE,
                snake[0].y + direction.y * GRID_SIZE
            };

            // Wrap horizontally
            if (newHead.x < 0) {
                newHead.x = SCREEN_WIDTH - GRID_SIZE;
            } else if (newHead.x >= SCREEN_WIDTH) {
                newHead.x = 0;
            }

            // Wrap vertically
            if (newHead.y < 0) {
                newHead.y = SCREEN_HEIGHT - GRID_SIZE;
            } else if (newHead.y >= SCREEN_HEIGHT) {
                newHead.y = 0;
            }

            // Collision with itself
            for (const auto& segment : snake) {
                if (newHead == segment) {
                    handleCollision();
                    return;
                }
            }

            // Collision with obstacles
            for (const auto& obs : obstacles) {
                if (newHead == obs) {
                    handleCollision();
                    return;
                }
            }

            // Insert new head
            snake.insert(snake.begin(), newHead);

            // Check for food collision
            auto it = std::find(foodItems.begin(), foodItems.end(), newHead);
            if (it != foodItems.end()) {
                // Score up
                score++;
                if (score > highScore) {
                    highScore = score;
                }
                Mix_PlayChannel(-1, eatsound, 0);
                // Clear old food and spawn new set
                foodItems.clear();
                spawnFood();
                spawnObstacles(5);
            } else {
                // Remove tail
                snake.pop_back();
            }
        }

        for (auto &sp : sparkles) {
            sp.life -= 0.05f;
        }
        sparkles.erase(std::remove_if(sparkles.begin(), sparkles.end(), 
            [](const Sparkle &s){return s.life <= 0.f;}), sparkles.end());
    }

    void handleCollision() {
        if (collisionSound) {
            Mix_PlayChannel(-1, collisionSound, 0);
        }
        for (int i = 0; i < 20; ++i) {
            Sparkle sp;
            sp.x = (float)snake[0].x + GRID_SIZE / 2;
            sp.y = (float)snake[0].y + GRID_SIZE / 2;
            sp.life = 1.0f;
            sparkles.push_back(sp);
        }
        if (score > highScore) {
            highScore = score;
        }
        resetGame();
    }

    //---------------------------------------------------
    //                      RENDER
    //---------------------------------------------------
    void render() {
        // Black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Grass first
        renderGrass();

        // Render state
        switch (state) {
            case GameState::MAIN_MENU:
                renderMainMenu();
                break;
            case GameState::CONFIG_MENU:
                renderConfigMenu();
                break;
            case GameState::MODE_MENU:
                renderModeMenu();
                break;
            case GameState::PLAYING:
                renderGame();
                renderScore();
                break;
            case GameState::PAUSED:
                renderGame();
                renderScore();
                renderPauseMenu();
                break;
            case GameState::QUIT:
                // do nothing
                break;
        }
        SDL_RenderPresent(renderer);
    }

    // Render the playing field (snake, obstacles, food).
    void renderGame() {
        // Draw grid lines for additional visual
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        for (int x = 0; x < SCREEN_WIDTH; x += GRID_SIZE) {
            SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
        }
        for (int y = 0; y < SCREEN_HEIGHT; y += GRID_SIZE) {
            SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
        }

        // Translucent overlay for atmosphere
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 128);
        SDL_Rect overlayRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderFillRect(renderer, &overlayRect);

        // Draw obstacles (blue squares).
        SDL_SetRenderDrawColor(renderer, 38, 143, 185, 255);
        for (const auto& obs : obstacles) {
            SDL_Rect rect = {obs.x, obs.y, GRID_SIZE, GRID_SIZE};
            SDL_RenderFillRect(renderer, &rect);
        }

        // Draw snake (green squares).
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        for (int i = 0; i < (int)snake.size(); i++) {
            SDL_Rect rect = {snake[i].x, snake[i].y, GRID_SIZE, GRID_SIZE};
            SDL_RenderFillRect(renderer, &rect);
        }

        // Draw food (red squares).
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for (const auto& food : foodItems) {
            SDL_Rect foodRect = {food.x, food.y, GRID_SIZE, GRID_SIZE};
            SDL_RenderFillRect(renderer, &foodRect);
        }

        // If in FLASHLIGHT mode, draw a "fog" except for a radius around head.
        if (gameMode == GameMode::FLASHLIGHT && !snake.empty()) {
            renderFlashlight();
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (auto &sp : sparkles) {
            int size = (int)(5 * sp.life);
            SDL_Rect r = {(int)sp.x - size/2, (int)sp.y - size/2, size, size};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // The "Flashlight" effect: draw a dark overlay over everything except 
    // around the snake head up to a certain number of blocks.
    void renderFlashlight() {
        // We'll determine all the visible cells in a radius around head.
        Point head = snake.front();
        int radiusPixels = FLASHLIGHT_RADIUS_BLOCKS * GRID_SIZE;

        // Dark overlay
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        // We'll do a simple approach: compute distance from head center 
        // to each cell center, if it's beyond radius, fill with black.

        // Head center position
        int headCenterX = head.x + GRID_SIZE / 2;
        int headCenterY = head.y + GRID_SIZE / 2;
        
        // Use a smaller grid-based approach to skip overhead
        for (int y = 0; y < SCREEN_HEIGHT; y += GRID_SIZE) {
            for (int x = 0; x < SCREEN_WIDTH; x += GRID_SIZE) {
                // Center of this cell
                int cx = x + GRID_SIZE / 2;
                int cy = y + GRID_SIZE / 2;

                // Euclidian distance
                int dx = cx - headCenterX;
                int dy = cy - headCenterY;
                int distSquared = dx*dx + dy*dy;

                // If beyond radius, fill black rectangle
                if (distSquared > (radiusPixels * radiusPixels)) {
                    SDL_Rect block = { x, y, GRID_SIZE, GRID_SIZE };
                    SDL_RenderFillRect(renderer, &block);
                }
            }
        }
    }

    //---------------------------------------------------
    //    PROCEDURAL GRASS: GENERATE & RENDER
    //---------------------------------------------------
    void generateGrass() {
        grassBlades.clear();
        int totalBlades = 3000;
        grassBlades.reserve(totalBlades);

        for (int i = 0; i < totalBlades; ++i) {
            GrassBlade blade;
            blade.x              = static_cast<float>(std::rand() % SCREEN_WIDTH);
            // random bottom half
            blade.y              = SCREEN_HEIGHT - (std::rand() % (SCREEN_HEIGHT / 2));
            blade.height         = 10.0f + static_cast<float>(std::rand() % 40);
            blade.waveOffset     = static_cast<float>(std::rand() % 100) * 0.1f;
            blade.randomAmplitude= 0.5f + static_cast<float>(std::rand() % 10);
            grassBlades.push_back(blade);
        }
    }

    void renderGrass() {
        // Grass color
        SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
        for (auto& blade : grassBlades) {
            float wave = std::sin(animationTime * grassWaveSpeed + blade.waveOffset) 
                         * grassWaveAmplitude 
                         * blade.randomAmplitude 
                         * 0.1f;
            float tipX = blade.x + wave;
            float tipY = blade.y - blade.height;

            SDL_RenderDrawLine(
                renderer,
                static_cast<int>(blade.x),
                static_cast<int>(blade.y),
                static_cast<int>(tipX),
                static_cast<int>(tipY)
            );
        }
    }

    //---------------------------------------------------
    //                   MENUS
    //---------------------------------------------------
    // Main menu (4 items).
    void renderMainMenu() {
        renderButton("Start Game",     0, selectedOption);
        renderButton("Config Menu",    1, selectedOption);
        renderButton("Game Mode",      2, selectedOption);
        renderButton("Quit",           3, selectedOption);

        renderText(
            "Use UP/DOWN to select, ENTER to confirm. ESC to pause/return",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT / 2 + 150,
            16,
            SDL_Color{200, 200, 200, 255}
        );
    }

    // Pause menu (2 items).
    void renderPauseMenu() {
        renderPauseButton("Resume",     0);
        renderPauseButton("Main Menu",  1);

        renderText(
            "Game Paused",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT / 2 - 80,
            32,
            SDL_Color{255, 255, 0, 255}
        );
        renderText(
            "Use ENTER to select, ESC to resume",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT / 2 + 90,
            16,
            SDL_Color{200, 200, 200, 255}
        );
    }

    // Configuration menu to adjust gameplay settings.
    void renderConfigMenu() {
        renderText("CONFIG MENU", SCREEN_WIDTH / 2, 50, 28, SDL_Color{255, 255, 0, 255});

        SDL_Color highlight = {255, 255, 0, 255};
        SDL_Color normal    = {255, 255, 255, 255};

        renderConfigLine(
            "SnakeSpeed (smaller = faster): " + std::to_string(snakeSpeed), 
            150, 
            (configOption == (int)ConfigOption::SPEED) ? highlight : normal
        );

        renderConfigLine(
            "Num Food: " + std::to_string(numFoodItems),
            200, 
            (configOption == (int)ConfigOption::NUM_FOOD) ? highlight : normal
        );

        renderConfigLine(
            "Num Obstacles: " + std::to_string(numObstacles),
            250,
            (configOption == (int)ConfigOption::NUM_OBSTACLES) ? highlight : normal
        );

        renderConfigLine(
            "GrassAmplitude: " + std::to_string(grassWaveAmplitude),
            300,
            (configOption == (int)ConfigOption::AMPLITUDE) ? highlight : normal
        );

        renderConfigLine(
            "GrassWaveSpeed: " + std::to_string(grassWaveSpeed),
            350,
            (configOption == (int)ConfigOption::WAVE_SPEED) ? highlight : normal
        );

        renderConfigLine(
            "Back to Main Menu",
            400,
            (configOption == (int)ConfigOption::EXIT) ? highlight : normal
        );

        renderText(
            "Use UP/DOWN to select, LEFT/RIGHT to adjust. ESC = back",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT - 40,
            16,
            SDL_Color{200, 200, 200, 255}
        );
    }

    // Mode menu to select NORMAL or FLASHLIGHT.
    void renderModeMenu() {
        renderText("CHOOSE GAME MODE", SCREEN_WIDTH / 2, 60, 28, SDL_Color{255, 255, 0, 255});

        // We'll define 0->Normal, 1->Flashlight in modeMenuOption.
        SDL_Color selColor   = {255, 255, 0, 255};
        SDL_Color otherColor = {255, 255, 255, 255};

        // For index=0 => Normal
        SDL_Color colorNormal = (modeMenuOption == 0) ? selColor : otherColor;
        SDL_Surface* surfaceNormal = TTF_RenderText_Solid(font, "NORMAL MODE", colorNormal);
        SDL_Texture* textureNormal = SDL_CreateTextureFromSurface(renderer, surfaceNormal);

        SDL_Rect normalRect {
            (SCREEN_WIDTH - surfaceNormal->w) / 2,
            200,
            surfaceNormal->w,
            surfaceNormal->h
        };
        SDL_RenderCopy(renderer, textureNormal, nullptr, &normalRect);
        SDL_FreeSurface(surfaceNormal);
        SDL_DestroyTexture(textureNormal);

        // For index=1 => Flashlight
        SDL_Color colorFlash = (modeMenuOption == 1) ? selColor : otherColor;
        SDL_Surface* surfaceFlash = TTF_RenderText_Solid(font, "FLASHLIGHT MODE", colorFlash);
        SDL_Texture* textureFlash = SDL_CreateTextureFromSurface(renderer, surfaceFlash);

        SDL_Rect flashRect {
            (SCREEN_WIDTH - surfaceFlash->w) / 2,
            260,
            surfaceFlash->w,
            surfaceFlash->h
        };
        SDL_RenderCopy(renderer, textureFlash, nullptr, &flashRect);
        SDL_FreeSurface(surfaceFlash);
        SDL_DestroyTexture(textureFlash);

        renderText(
            "Use UP/DOWN to highlight, ENTER to confirm. ESC to return",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT - 40,
            16,
            SDL_Color{200, 200, 200, 255}
        );
    }

    //---------------------------------------------------
    //             SCORE & TEXT RENDERING
    //---------------------------------------------------
    void renderScore() {
        std::string scoreMsg = "Score: " + std::to_string(score);
        renderDynamicText(scoreMsg.c_str(), 10, 10, 255, 255, 255);

        std::string highScoreMsg = "High: " + std::to_string(highScore);
        renderDynamicText(highScoreMsg.c_str(), 10, 40, 255, 255, 0);
    }

    void renderButton(const char* text, int index, int selectedIndex) {
        SDL_Color color = (index == selectedIndex)
            ? SDL_Color{255, 255, 0, 255}
            : SDL_Color{255, 255, 255, 255};

        SDL_Rect rect;
        rect.w = 220;
        rect.h = 40;
        rect.x = SCREEN_WIDTH / 2 - rect.w / 2;
        rect.y = 200 + index * 60;

        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &rect);

        SDL_Surface* surface = TTF_RenderText_Solid(font, text, SDL_Color{0, 0, 0, 255});
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect {
            rect.x + (rect.w - surface->w) / 2,
            rect.y + (rect.h - surface->h) / 2,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }

    void renderPauseButton(const char* text, int index) {
        SDL_Color color = (index == pauseMenuOption)
            ? SDL_Color{255, 255, 0, 255}
            : SDL_Color{255, 255, 255, 255};

        SDL_Rect rect;
        rect.w = 200;
        rect.h = 40;
        rect.x = SCREEN_WIDTH / 2 - rect.w / 2;
        rect.y = SCREEN_HEIGHT / 2 - 20 + (index * 60);

        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &rect);

        SDL_Surface* surface = TTF_RenderText_Solid(font, text, SDL_Color{0, 0, 0, 255});
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect {
            rect.x + (rect.w - surface->w) / 2,
            rect.y + (rect.h - surface->h) / 2,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }

    void renderConfigLine(const std::string& txt, int y, SDL_Color color) {
        SDL_Surface* surface = TTF_RenderText_Solid(font, txt.c_str(), color);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect;
        textRect.w = surface->w;
        textRect.h = surface->h;
        textRect.x = (SCREEN_WIDTH - surface->w) / 2;
        textRect.y = y;

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }

    void renderText(const char* text, int centerX, int centerY, int fontSize, SDL_Color color) {
        TTF_Font* tempFont = TTF_OpenFont("COMIC.TTF", fontSize);
        if (!tempFont) {
            return;
        }

        SDL_Surface* surface = TTF_RenderText_Solid(tempFont, text, color);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect{0, 0, surface->w, surface->h};
        textRect.x = centerX - (surface->w / 2);
        textRect.y = centerY - (surface->h / 2);

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
        TTF_CloseFont(tempFont);
    }

    void renderDynamicText(const char* text, int x, int y, Uint8 r, Uint8 g, Uint8 b) {
        SDL_Color color { r, g, b, 255 };
        SDL_Surface* surface = TTF_RenderText_Solid(scoreFont, text, color);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, nullptr, &textRect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }

    //---------------------------------------------------
    //             START & RESET GAME
    //---------------------------------------------------
    void startGame() {
        state = GameState::PLAYING;
        resetGame();
    }

    void resetGame() {
        snake.clear();
        snake.push_back({SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2});
        direction = {1, 0};
        score     = 0;

        foodItems.clear();
        obstacles.clear();
        spawnFood();
        spawnObstacles(numObstacles);
    }

    //---------------------------------------------------
    //       SPAWNING FOOD & OBSTACLES
    //---------------------------------------------------
    void spawnFood() {
        for (int i = 0; i < numFoodItems; i++) {
            Point food;
            bool validPos = false;
            while (!validPos) {
                food.x = (std::rand() % (SCREEN_WIDTH / GRID_SIZE)) * GRID_SIZE;
                food.y = (std::rand() % (SCREEN_HEIGHT / GRID_SIZE)) * GRID_SIZE;
                validPos = true;
                // check snake
                for (auto& seg : snake) {
                    if (food == seg) {
                        validPos = false;
                        break;
                    }
                }
                // check obstacles
                for (auto& obs : obstacles) {
                    if (food == obs) {
                        validPos = false;
                        break;
                    }
                }
            }
            foodItems.push_back(food);
        }
    }

    void spawnObstacles(int count) {
        for (int i = 0; i < count; i++) {
            Point obs;
            bool validPos = false;
            while (!validPos) {
                obs.x = (std::rand() % (SCREEN_WIDTH / GRID_SIZE)) * GRID_SIZE;
                obs.y = (std::rand() % (SCREEN_HEIGHT / GRID_SIZE)) * GRID_SIZE;
                validPos = true;
                // check snake
                for (auto& seg : snake) {
                    if (obs == seg) {
                        validPos = false;
                        break;
                    }
                }
                // check food
                for (auto& f : foodItems) {
                    if (obs == f) {
                        validPos = false;
                        break;
                    }
                }
            }
            obstacles.push_back(obs);
        }
    }
};

//-------------------------------------------------------
//                        MAIN
//-------------------------------------------------------
int main() {
    Application app;
    app.run();
    return 0;
}