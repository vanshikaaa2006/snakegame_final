// main.c
#include "raylib.h"
#include "snake.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#define CELL_SIZE 20
#define GRID_WIDTH 30
#define GRID_HEIGHT 20
#define MAX_NAME_LEN 30
#define LEADERBOARD_FILE "leaderboard.txt"
#define LEADERBOARD_WIDTH 300

// Obstacles
#define MAX_WALLS 8

typedef struct {
    char name[MAX_NAME_LEN];
    int score;
} PlayerScore;

// Forward declarations
void save_score(const char *name, int score);
void draw_leaderboard_panel(int startX, const char *currentPlayer);
int on_snake(Snake *snake, int x, int y);
int wall_at(Vector2 *walls, int wallCount, int x, int y);
void get_random_free_cell(Snake *snake, Vector2 *walls, int wallCount, int *outX, int *outY);

int main(void) {
    // Window + audio
    InitWindow(GRID_WIDTH * CELL_SIZE + LEADERBOARD_WIDTH, GRID_HEIGHT * CELL_SIZE, "Snake Game in C (Raylib)");
    InitAudioDevice();

    // Load sounds
    Sound eatSound = LoadSound("sounds/eat.wav");
    Sound hitSound = LoadSound("sounds/hit.wav");
    Sound bonusSound = LoadSound("sounds/bonus.wav");

    SetSoundVolume(eatSound, 0.7f);
    SetSoundVolume(hitSound, 1.0f);
    SetSoundVolume(bonusSound, 0.6f);

    // Difficulty variables (set by menu)
    int gameSpeed = 10;           // default
    int scoreMultiplier = 1;      // default

    // Game state
    srand((unsigned)time(NULL));
    char playerName[MAX_NAME_LEN] = "";
    int nameEntered = 0;
    int letterCount = 0;

    // Difficulty selection state
    int difficultySelected = 0; // 0 = not chosen, 1 = chosen

    Snake *snake = NULL;
    Vector2 food = {0,0};
    Vector2 bonusFruit = {0,0};
    Vector2 powerFruit = {0,0}; // green power (slow) fruit
    Vector2 walls[MAX_WALLS];

    int grow = 0, gameOver = 0, score = 0;
    int fruitsEaten = 0;
    int bonusActive = 0;
    double bonusStartTime = 0;
    const double bonusDuration = 5.0;

    int powerActive = 0;
    double powerStartTime = 0;
    const double powerDuration = 7.0; // how long power fruit stays available
    int slowMode = 0;
    double slowStartTime = 0;
    const double slowDuration = 5.0; // how long slow effect lasts

    // initial target fps (will be set after difficulty)
    SetTargetFPS(gameSpeed);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // ------------------- Difficulty selection screen -------------------
        if (!difficultySelected) {
            DrawText("Select Difficulty:", 80, 80, 30, RAYWHITE);
            DrawText("[1] Easy   (8 FPS, 1x)", 90, 140, 20, LIGHTGRAY);
            DrawText("[2] Medium (12 FPS, 2x)", 90, 180, 20, LIGHTGRAY);
            DrawText("[3] Hard   (18 FPS, 3x)", 90, 220, 20, LIGHTGRAY);
            DrawText("Press 1/2/3 to choose", 90, 280, 18, GRAY);

            if (IsKeyPressed(KEY_ONE)) {
                gameSpeed = 8; scoreMultiplier = 1; difficultySelected = 1;
            } else if (IsKeyPressed(KEY_TWO)) {
                gameSpeed = 12; scoreMultiplier = 2; difficultySelected = 1;
            } else if (IsKeyPressed(KEY_THREE)) {
                gameSpeed = 18; scoreMultiplier = 3; difficultySelected = 1;
            }

            EndDrawing();
            continue;
        }

        // ------------------- Player name input -------------------
        if (!nameEntered) {
            // show selected difficulty
            char diffText[64];
            sprintf(diffText, "Difficulty: %s  (FPS %d, %dx)", 
                    (scoreMultiplier==1?"Easy": (scoreMultiplier==2?"Medium":"Hard")),
                    gameSpeed, scoreMultiplier);
            DrawText(diffText, 80, 40, 20, LIGHTGRAY);

            DrawText("Enter your name:", 80, 100, 30, RAYWHITE);
            DrawText(playerName, 80, 150, 30, GREEN);
            DrawText("Press ENTER to start", 80, 200, 20, GRAY);

            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (letterCount < MAX_NAME_LEN - 1)) {
                    playerName[letterCount] = (char)key;
                    playerName[letterCount + 1] = '\0';
                    letterCount++;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && letterCount > 0) {
                letterCount--;
                playerName[letterCount] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) && strlen(playerName) > 0) {
                nameEntered = 1;
                // initialize game entities
                snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);
                // populate walls in free cells
                for (int i = 0; i < MAX_WALLS; i++) {
                    int wx, wy;
                    get_random_free_cell(snake, walls, i, &wx, &wy); // note: pass i so previous walls are respected
                    walls[i].x = wx; walls[i].y = wy;
                }
                // place normal food
                get_random_free_cell(snake, walls, MAX_WALLS, (int*)&food.x, (int*)&food.y);
                fruitsEaten = 0;
                bonusActive = 0;
                powerActive = 0;
                slowMode = 0;
                score = 0;
                gameOver = 0;
                SetTargetFPS(gameSpeed);
            }

            EndDrawing();
            continue;
        }

        // ------------------- Game loop (logic) -------------------
        // If slowMode is active, keep target FPS at half (we'll manage timing)
        if (slowMode) {
            if (GetTime() - slowStartTime > slowDuration) {
                slowMode = 0;
                SetTargetFPS(gameSpeed);
            } else {
                SetTargetFPS(gameSpeed / 2 > 0 ? gameSpeed / 2 : 1);
            }
        } else {
            SetTargetFPS(gameSpeed);
        }

        if (!gameOver) {
            // input
            if (IsKeyPressed(KEY_UP) && snake->direction != 2) snake->direction = 0;
            if (IsKeyPressed(KEY_RIGHT) && snake->direction != 3) snake->direction = 1;
            if (IsKeyPressed(KEY_DOWN) && snake->direction != 0) snake->direction = 2;
            if (IsKeyPressed(KEY_LEFT) && snake->direction != 1) snake->direction = 3;

            move_snake(snake, grow);
            grow = 0;

            // collision with walls (obstacles)
            for (int i = 0; i < MAX_WALLS; i++) {
                if ((int)walls[i].x == snake->head->x && (int)walls[i].y == snake->head->y) {
                    PlaySound(hitSound);
                    gameOver = 1;
                    break;
                }
            }

            // Normal food eaten
            if (!gameOver && snake->head->x == (int)food.x && snake->head->y == (int)food.y) {
                PlaySound(eatSound);
                grow = 1;
                score += 10 * scoreMultiplier;
                fruitsEaten++;

                // place new normal food
                int fx, fy;
                get_random_free_cell(snake, walls, MAX_WALLS, &fx, &fy);
                food.x = fx; food.y = fy;

                // spawn bonus every 8 normal fruits
                if (fruitsEaten % 8 == 0 && !bonusActive) {
                    PlaySound(bonusSound);
                    bonusActive = 1;
                    get_random_free_cell(snake, walls, MAX_WALLS, (int*)&bonusFruit.x, (int*)&bonusFruit.y);
                    bonusStartTime = GetTime();
                }

                // spawn power (slow) fruit every 12 normal fruits
                if (fruitsEaten % 12 == 0 && !powerActive) {
                    // place powerFruit
                    powerActive = 1;
                    get_random_free_cell(snake, walls, MAX_WALLS, (int*)&powerFruit.x, (int*)&powerFruit.y);
                    powerStartTime = GetTime();
                }
            }

            // Bonus fruit handling
            if (bonusActive) {
                if (GetTime() - bonusStartTime > bonusDuration) {
                    bonusActive = 0; // disappear
                } else if (snake->head->x == (int)bonusFruit.x && snake->head->y == (int)bonusFruit.y) {
                    PlaySound(eatSound);
                    score += 20 * scoreMultiplier; // double normal * multiplier
                    grow = 1;
                    bonusActive = 0;
                }
            }

            // Power fruit handling (green slow fruit)
            if (powerActive) {
                if (GetTime() - powerStartTime > powerDuration) {
                    powerActive = 0; // disappear if not eaten
                } else if (snake->head->x == (int)powerFruit.x && snake->head->y == (int)powerFruit.y) {
                    PlaySound(eatSound);
                    // apply slow effect
                    slowMode = 1;
                    slowStartTime = GetTime();
                    // Award some points for power fruit (slightly more than normal)
                    score += 15 * scoreMultiplier;
                    grow = 1;
                    powerActive = 0;
                }
            }

            // self or wall collision already handled above partially; check boundary/self
            if (!gameOver && check_collision(snake, GRID_WIDTH, GRID_HEIGHT)) {
                PlaySound(hitSound);
                gameOver = 1;
            }
        }

        // ------------------- Drawing -------------------
        // draw game area background
        DrawRectangle(0, 0, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE, (Color){10,10,10,255});

        // draw walls
        for (int i = 0; i < MAX_WALLS; i++) {
            DrawRectangle((int)walls[i].x * CELL_SIZE, (int)walls[i].y * CELL_SIZE, CELL_SIZE, CELL_SIZE, GRAY);
        }

        if (!gameOver) {
            // Normal food
            DrawRectangle((int)food.x * CELL_SIZE, (int)food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, RED);

            // Bonus fruit (yellow, larger)
            if (bonusActive) {
                int bonusSize = CELL_SIZE + 6;
                DrawRectangle((int)bonusFruit.x * CELL_SIZE - 3, (int)bonusFruit.y * CELL_SIZE - 3, bonusSize, bonusSize, YELLOW);
                double remaining = bonusDuration - (GetTime() - bonusStartTime);
                char bonusText[32];
                sprintf(bonusText, "BONUS: %.1fs", remaining);
                DrawText(bonusText, 10, GRID_HEIGHT * CELL_SIZE - 30, 20, YELLOW);
            }

            // Power fruit (green) draw slightly larger than normal
            if (powerActive) {
                int pSize = CELL_SIZE + 4;
                DrawRectangle((int)powerFruit.x * CELL_SIZE - 2, (int)powerFruit.y * CELL_SIZE - 2, pSize, pSize, GREEN);
                double prem = powerDuration - (GetTime() - powerStartTime);
                char ptext[32];
                sprintf(ptext, "POWER: %.1fs", prem);
                DrawText(ptext, 150, GRID_HEIGHT * CELL_SIZE - 30, 20, GREEN);
            }

            // Draw snake
            draw_snake(snake, CELL_SIZE);

            // Score / HUD
            char scoreText[80];
            sprintf(scoreText, "Player: %s   Score: %d", playerName, score);
            DrawText(scoreText, 10, 10, 20, RAYWHITE);

            // Show slow mode indicator if active
            if (slowMode) {
                DrawText("SLOWED!", 300, 10, 20, SKYBLUE);
            }
        } else {
            DrawText("GAME OVER", 100, 100, 40, RED);
            char finalText[96];
            sprintf(finalText, "Player: %s  |  Score: %d", playerName, score);
            DrawText(finalText, 100, 160, 25, RAYWHITE);
            DrawText("Press [R] to restart", 100, 200, 20, GRAY);
            DrawText("Press [L] to save score", 100, 230, 20, GRAY);

            if (IsKeyPressed(KEY_R)) {
                // restart: free snake, recreate, reposition everything
                if (snake) free_snake(snake);
                snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);

                // regenerate walls and fruits
                for (int i = 0; i < MAX_WALLS; i++) {
                    int wx, wy;
                    get_random_free_cell(snake, walls, i, &wx, &wy);
                    walls[i].x = wx; walls[i].y = wy;
                }
                get_random_free_cell(snake, walls, MAX_WALLS, (int*)&food.x, (int*)&food.y);
                fruitsEaten = 0;
                bonusActive = 0;
                powerActive = 0;
                slowMode = 0;
                score = 0;
                gameOver = 0;
                SetTargetFPS(gameSpeed);
            }

            if (IsKeyPressed(KEY_L)) {
                save_score(playerName, score);
            }
        }

        // draw leaderboard panel on right (live)
        draw_leaderboard_panel(GRID_WIDTH * CELL_SIZE, playerName);

        EndDrawing();
    }

    // Cleanup
    if (snake) free_snake(snake);
    UnloadSound(eatSound);
    UnloadSound(hitSound);
    UnloadSound(bonusSound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

/*** Utility functions ***/

// save a name+score to file (append)
void save_score(const char *name, int score) {
    FILE *f = fopen(LEADERBOARD_FILE, "a");
    if (!f) return;
    fprintf(f, "%s %d\n", name, score);
    fclose(f);
}

// draw leaderboard panel and highlight current player
void draw_leaderboard_panel(int startX, const char *currentPlayer) {
    DrawRectangle(startX, 0, LEADERBOARD_WIDTH, GRID_HEIGHT * CELL_SIZE, (Color){30,30,30,255});
    DrawText("LEADERBOARD", startX + 40, 20, 25, GOLD);

    FILE *f = fopen(LEADERBOARD_FILE, "r");
    if (!f) {
        DrawText("No scores yet!", startX + 40, 70, 20, GRAY);
        return;
    }

    PlayerScore scores[200];
    int count = 0;
    while (fscanf(f, "%s %d", scores[count].name, &scores[count].score) == 2 && count < 200) {
        count++;
    }
    fclose(f);

    // sort descending
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (scores[j].score < scores[j+1].score) {
                PlayerScore tmp = scores[j];
                scores[j] = scores[j+1];
                scores[j+1] = tmp;
            }
        }
    }

    // display top 10, highlight current player
    for (int i = 0; i < count && i < 10; i++) {
        char entry[128];
        sprintf(entry, "%2d. %-10s %5d", i+1, scores[i].name, scores[i].score);
        if (strcmp(scores[i].name, currentPlayer) == 0) {
            DrawText(entry, startX + 20, 70 + i*30, 22, YELLOW);
        } else {
            DrawText(entry, startX + 20, 70 + i*30, 20, RAYWHITE);
        }
    }
}

// check if coordinates are on the snake (returns 1 if yes)
int on_snake(Snake *snake, int x, int y) {
    if (!snake) return 0;
    Node *p = snake->head;
    while (p) {
        if (p->x == x && p->y == y) return 1;
        p = p->next;
    }
    return 0;
}

// check if a wall exists at (x,y)
int wall_at(Vector2 *walls, int wallCount, int x, int y) {
    for (int i = 0; i < wallCount; i++) {
        if ((int)walls[i].x == x && (int)walls[i].y == y) return 1;
    }
    return 0;
}

// Get a random free cell not occupied by snake or walls.
// wallsCount argument tells how many walls are already placed (so placing incremental walls works)
void get_random_free_cell(Snake *snake, Vector2 *walls, int wallsCount, int *outX, int *outY) {
    int tries = 0;
    while (tries < 10000) {
        int rx = rand() % GRID_WIDTH;
        int ry = rand() % GRID_HEIGHT;
        if (on_snake(snake, rx, ry)) { tries++; continue; }
        if (wall_at(walls, wallsCount, rx, ry)) { tries++; continue; }
        // ok free
        *outX = rx;
        *outY = ry;
        return;
    }
    // fallback (shouldn't normally happen)
    *outX = 0; *outY = 0;
}
