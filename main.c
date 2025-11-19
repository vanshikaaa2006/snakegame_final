#include "raylib.h"
#include "snake.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define CELL_SIZE 20
#define GRID_WIDTH 30
#define GRID_HEIGHT 20
#define MAX_NAME_LEN 30
#define LEADERBOARD_FILE "leaderboard.txt"
#define LEADERBOARD_WIDTH 300

// Max obstacles possible (actual count depends on difficulty)
#define MAX_WALLS 8

typedef struct {
    char name[MAX_NAME_LEN];
    int score;
} PlayerScore;

// Wall linked list node
typedef struct WallNode {
    Vector2 pos;
    struct WallNode *next;
} WallNode;

// Leaderboard BST node
typedef struct ScoreNode {
    char name[MAX_NAME_LEN];
    int score;
    struct ScoreNode *left, *right;
} ScoreNode;

// ---- Forward declarations ----
void save_score(const char *name, int score);
void draw_leaderboard_panel(int startX, const char *currentPlayer);

// Placement helpers
int on_snake_occ(int x, int y); // uses occupancy grid
bool wall_at_list(WallNode *walls, int x, int y);
int get_random_free_cell(Snake *snake, WallNode *walls, int *outX, int *outY);
int place_random_food_not_on(Snake *snake, WallNode *walls, int *outX, int *outY,
                              int avoidX1, int avoidY1, int avoidX2, int avoidY2);

// wall list helpers
WallNode* add_wall(WallNode *head, int x, int y);
void free_walls(WallNode *head);
int wall_count_list(WallNode *head);

// occupancy hash-set for snake positions
static unsigned char occ[GRID_HEIGHT][GRID_WIDTH];
void rebuild_occupancy_from_snake(Snake *snake) {
    // clear
    for (int y = 0; y < GRID_HEIGHT; y++) for (int x = 0; x < GRID_WIDTH; x++) occ[y][x] = 0;
    if (!snake) return;
    Node *p = snake->head;
    while (p) {
        if (p->x >= 0 && p->x < GRID_WIDTH && p->y >= 0 && p->y < GRID_HEIGHT)
            occ[p->y][p->x] = 1;
        p = p->next;
    }
}
int on_snake_occ(int x, int y) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return 0;
    return occ[y][x];
}

// ------------------------- BST utilities for leaderboard -------------------------
ScoreNode* bst_insert(ScoreNode *root, const char *name, int score) {
    if (!root) {
        ScoreNode *n = malloc(sizeof(ScoreNode));
        strncpy(n->name, name, MAX_NAME_LEN);
        n->name[MAX_NAME_LEN-1] = '\0';
        n->score = score; n->left = n->right = NULL; return n;
    }
    if (score > root->score) root->left = bst_insert(root->left, name, score);
    else root->right = bst_insert(root->right, name, score);
    return root;
}

// reverse in-order traversal to collect top N
void bst_collect_top(ScoreNode *root, PlayerScore *outArr, int *idx, int limit) {
    if (!root || *idx >= limit) return;
    bst_collect_top(root->left, outArr, idx, limit);
    if (*idx < limit) {
        strncpy(outArr[*idx].name, root->name, MAX_NAME_LEN);
        outArr[*idx].score = root->score;
        (*idx)++;
    }
    if (*idx < limit) bst_collect_top(root->right, outArr, idx, limit);
}

void free_bst(ScoreNode *root) {
    if (!root) return; free_bst(root->left); free_bst(root->right); free(root);
}

// -------------------- Random free cell helpers (enumeration-based) --------------------
int get_random_free_cell(Snake *snake, WallNode *walls, int *outX, int *outY) {
    // build occupancy combined
    unsigned char used[GRID_HEIGHT][GRID_WIDTH];
    for (int y = 0; y < GRID_HEIGHT; y++) for (int x = 0; x < GRID_WIDTH; x++) used[y][x] = 0;

    // snake
    if (snake) {
        Node *p = snake->head;
        while (p) {
            if (p->x >= 0 && p->x < GRID_WIDTH && p->y >= 0 && p->y < GRID_HEIGHT)
                used[p->y][p->x] = 1;
            p = p->next;
        }
    }
    // walls
    WallNode *w = walls;
    while (w) {
        int wx = (int)w->pos.x, wy = (int)w->pos.y;
        if (wx >= 0 && wx < GRID_WIDTH && wy >= 0 && wy < GRID_HEIGHT) used[wy][wx] = 1;
        w = w->next;
    }
    int freeCount = 0;
    for (int y = 0; y < GRID_HEIGHT; y++) for (int x = 0; x < GRID_WIDTH; x++) if (!used[y][x]) freeCount++;
    if (freeCount == 0) return 0;
    int pick = rand() % freeCount;
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (!used[y][x]) {
                if (pick == 0) { *outX = x; *outY = y; return 1; }
                pick--;
            }
        }
    }
    return 0;
}

int place_random_food_not_on(Snake *snake, WallNode *walls, int *outX, int *outY,
                              int avoidX1, int avoidY1, int avoidX2, int avoidY2) {
    unsigned char used[GRID_HEIGHT][GRID_WIDTH];
    for (int y = 0; y < GRID_HEIGHT; y++) for (int x = 0; x < GRID_WIDTH; x++) used[y][x] = 0;
    if (snake) {
        Node *p = snake->head;
        while (p) { if (p->x>=0 && p->x<GRID_WIDTH && p->y>=0 && p->y<GRID_HEIGHT) used[p->y][p->x]=1; p = p->next; }
    }
    WallNode *w = walls;
    while (w) { int wx=(int)w->pos.x, wy=(int)w->pos.y; if (wx>=0 && wx<GRID_WIDTH && wy>=0 && wy<GRID_HEIGHT) used[wy][wx]=1; w=w->next; }
    if (avoidX1>=0 && avoidY1>=0 && avoidX1<GRID_WIDTH && avoidY1<GRID_HEIGHT) used[avoidY1][avoidX1]=1;
    if (avoidX2>=0 && avoidY2>=0 && avoidX2<GRID_WIDTH && avoidY2<GRID_HEIGHT) used[avoidY2][avoidX2]=1;
    int freeCount = 0; for (int y=0;y<GRID_HEIGHT;y++) for (int x=0;x<GRID_WIDTH;x++) if(!used[y][x]) freeCount++;
    if (freeCount==0) return 0;
    int pick = rand()%freeCount;
    for (int y=0;y<GRID_HEIGHT;y++) for (int x=0;x<GRID_WIDTH;x++) if(!used[y][x]) { if (pick==0) { *outX=x; *outY=y; return 1; } pick--; }
    return 0;
}

// -------------------- Wall list helpers --------------------
WallNode* add_wall(WallNode *head, int x, int y) {
    WallNode *n = malloc(sizeof(WallNode)); n->pos.x = (float)x; n->pos.y=(float)y; n->next=NULL;
    if (!head) return n;
    // append at end for deterministic order
    WallNode *p = head; while (p->next) p = p->next; p->next = n; return head;
}

void free_walls(WallNode *head) { WallNode *p = head; while (p) { WallNode *t = p->next; free(p); p = t; } }

int wall_count_list(WallNode *head) { int c=0; WallNode *p=head; while(p){c++; p=p->next;} return c; }

bool wall_at_list(WallNode *walls, int x, int y) {
    WallNode *p = walls; while (p) { if ((int)p->pos.x==x && (int)p->pos.y==y) return true; p=p->next; } return false;
}

// -------------------- Simple BFS pathfinder for optional AI mode --------------------
// returns 1 if path found and fills first move in outDir (0 up,1 right,2 down,3 left)
int find_path_to_target(int sx, int sy, int tx, int ty, WallNode *walls, int *outDir) {
    // simple BFS that avoids walls; doesn't consider snake body for safety (could be extended)
    if (sx==tx && sy==ty) return 0;
    int visited[GRID_HEIGHT][GRID_WIDTH]; for (int y=0;y<GRID_HEIGHT;y++) for (int x=0;x<GRID_WIDTH;x++) visited[y][x]=0;
    int px[GRID_HEIGHT][GRID_WIDTH], py[GRID_HEIGHT][GRID_WIDTH], pd[GRID_HEIGHT][GRID_WIDTH];
    typedef struct { int x,y; } Q; Q q[GRID_WIDTH*GRID_HEIGHT]; int qh=0, qt=0;
    q[qt++] = (Q){sx,sy}; visited[sy][sx]=1; px[sy][sx]=-1;
    int dirs[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
    while (qh<qt) {
        Q cur = q[qh++]; if (cur.x==tx && cur.y==ty) break;
        for (int d=0; d<4; d++) {
            int nx = cur.x + dirs[d][0], ny = cur.y + dirs[d][1];
            if (nx<0||nx>=GRID_WIDTH||ny<0||ny>=GRID_HEIGHT) continue;
            if (visited[ny][nx]) continue;
            if (wall_at_list(walls, nx, ny)) continue;
            visited[ny][nx]=1; px[ny][nx]=cur.x; py[ny][nx]=cur.y; pd[ny][nx]=d;
            q[qt++] = (Q){nx,ny};
        }
    }
    if (!visited[ty][tx]) return 0;
    // backtrack to get first move
    int cx=tx, cy=ty;
    int prevx = px[cy][cx], prevy = py[cy][cx];
    while (prevx != -1 && !(prevx==sx && prevy==sy)) {
        int tx2 = prevx; int ty2 = prevy; prevx = px[ty2][tx2]; prevy = py[ty2][tx2]; cx = tx2; cy = ty2;
    }
    // pd[c y][c x] stores direction used to step from parent to this cell; find direction from start
    // find neighbor of start that is on path
    for (int d=0; d<4; d++) {
        int nx = sx + dirs[d][0], ny = sy + dirs[d][1];
        if (nx==cx && ny==cy) { *outDir = d; return 1; }
    }
    return 0;
}

// -------------------- File-based leaderboard saving --------------------
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

    ScoreNode *root = NULL;
    char name[MAX_NAME_LEN]; int sc;
    while (fscanf(f, "%s %d", name, &sc) == 2) {
        root = bst_insert(root, name, sc);
    }
    fclose(f);

    PlayerScore top[10]; int idx = 0;
    // our bst_insert puts higher scores to left; reverse in-order collects descending
    bst_collect_top(root, top, &idx, 10);

    for (int i = 0; i < idx; i++) {
        char entry[128]; sprintf(entry, "%2d. %-10s %5d", i+1, top[i].name, top[i].score);
        if (strcmp(top[i].name, currentPlayer) == 0) DrawText(entry, startX + 20, 70 + i*30, 22, YELLOW);
        else DrawText(entry, startX + 20, 70 + i*30, 20, RAYWHITE);
    }

    free_bst(root);
}

// -------------------- Main --------------------
int main(void) {
    // Window + audio
    InitWindow(GRID_WIDTH * CELL_SIZE + LEADERBOARD_WIDTH,
               GRID_HEIGHT * CELL_SIZE,
               "Snake Game in C (Raylib) - DS Enhanced");
    InitAudioDevice();

    // Load sounds
    Sound eatSound   = LoadSound("sounds/eat.wav");
    Sound hitSound   = LoadSound("sounds/hit.wav");
    Sound bonusSound = LoadSound("sounds/bonus.wav");

    SetSoundVolume(eatSound, 0.7f);
    SetSoundVolume(hitSound, 1.0f);
    SetSoundVolume(bonusSound, 0.6f);

    // Difficulty variables
    int gameSpeed = 10;        // will change after selection
    int scoreMultiplier = 1;   // 1=Easy, 2=Medium, 3=Hard
    int lives = 3;             // changes with difficulty

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
    Vector2 powerFruit = {0,0};
    WallNode *walls = NULL;

    int grow = 0, gameOver = 0, score = 0;
    int fruitsEaten = 0;

    int bonusActive = 0;
    double bonusStartTime = 0;
    const double bonusDuration = 5.0;

    int powerActive = 0;
    double powerStartTime = 0;
    const double powerDuration = 7.0;

    int slowMode = 0;
    double slowStartTime = 0;
    const double slowDuration = 5.0;

    bool paused = false;
    bool aiMode = false;

    SetTargetFPS(gameSpeed);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // ------------------- Difficulty selection screen -------------------
        if (!difficultySelected) {
            DrawText("Select Difficulty:", 80, 80, 30, RAYWHITE);
            DrawText("[1] Easy   (8 FPS,   1x, NO obstacles, 3 lives)", 90, 140, 20, LIGHTGRAY);
            DrawText("[2] Medium (12 FPS,  2x, obstacles, 5 lives)",   90, 180, 20, LIGHTGRAY);
            DrawText("[3] Hard   (18 FPS,  3x, obstacles, 8 lives)",   90, 220, 20, LIGHTGRAY);
            DrawText("Press 1/2/3 to choose", 90, 280, 18, GRAY);

            if (IsKeyPressed(KEY_ONE)) { gameSpeed = 8;  scoreMultiplier = 1; lives = 3; difficultySelected = 1; }
            else if (IsKeyPressed(KEY_TWO)) { gameSpeed = 12; scoreMultiplier = 2; lives = 5; difficultySelected = 1; }
            else if (IsKeyPressed(KEY_THREE)) { gameSpeed = 18; scoreMultiplier = 3; lives = 8; difficultySelected = 1; }

            EndDrawing();
            continue;
        }

        // ------------------- Player name input -------------------
        if (!nameEntered) {
            char diffText[96];
            sprintf(diffText, "Difficulty: %s  (FPS %d, %dx, Lives: %d)",
                    (scoreMultiplier==1?"Easy": (scoreMultiplier==2?"Medium":"Hard")),
                    gameSpeed, scoreMultiplier, lives);
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
                if (snake) free_snake(snake);
                snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);
                rebuild_occupancy_from_snake(snake);

                // populate walls only if difficulty requires obstacles
                free_walls(walls); walls = NULL;
                if (scoreMultiplier > 1) {
                    for (int i = 0; i < MAX_WALLS; i++) {
                        int wx, wy;
                        if (get_random_free_cell(snake, walls, &wx, &wy)) {
                            walls = add_wall(walls, wx, wy);
                        }
                    }
                }

                // place normal food (avoid clash with snake/walls)
                int fx, fy;
                if (get_random_free_cell(snake, walls, &fx, &fy)) { food.x = (float)fx; food.y = (float)fy; }

                fruitsEaten = 0;
                bonusActive = 0;
                powerActive = 0;
                slowMode = 0;
                score = 0;
                gameOver = 0;
                paused = false;
                aiMode = false;

                SetTargetFPS(gameSpeed);
            }

            EndDrawing();
            continue;
        }

        // Pause toggle
        if (IsKeyPressed(KEY_P)) paused = !paused;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_A)) aiMode = !aiMode; // toggle AI mode

        if (paused) {
            DrawText("PAUSED - Press P to resume", 80, 80, 30, GOLD);
            // still draw current board
            DrawRectangle(0, 0, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE, (Color){10,10,10,255});
            // draw walls
            WallNode *w = walls;
            while (w) { DrawRectangle((int)w->pos.x * CELL_SIZE, (int)w->pos.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, GRAY); w = w->next; }
            draw_snake(snake, CELL_SIZE);
            DrawRectangle((int)food.x * CELL_SIZE, (int)food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, RED);
            draw_leaderboard_panel(GRID_WIDTH * CELL_SIZE, playerName);
            EndDrawing();
            continue;
        }

        // ------------------- Game loop (logic) -------------------
        if (slowMode) {
            if (GetTime() - slowStartTime > slowDuration) {
                slowMode = 0; SetTargetFPS(gameSpeed);
            } else {
                SetTargetFPS(gameSpeed / 2 > 0 ? gameSpeed / 2 : 1);
            }
        } else {
            SetTargetFPS(gameSpeed);
        }

        if (!gameOver) {
            // input (only when not AI mode)
            if (!aiMode) {
                if (IsKeyPressed(KEY_UP) && snake->direction != 2) snake->direction = 0;
                if (IsKeyPressed(KEY_RIGHT) && snake->direction != 3) snake->direction = 1;
                if (IsKeyPressed(KEY_DOWN) && snake->direction != 0) snake->direction = 2;
                if (IsKeyPressed(KEY_LEFT) && snake->direction != 1) snake->direction = 3;
            } else {
                // AI: compute first move towards food
                int dir;
                if (find_path_to_target(snake->head->x, snake->head->y, (int)food.x, (int)food.y, walls, &dir)) {
                    snake->direction = dir;
                }
            }

            move_snake(snake, grow);
            grow = 0;
            rebuild_occupancy_from_snake(snake);

            // collision with walls (if any)
            if (!gameOver) {
                WallNode *w = walls; bool collidedWall = false;
                while (w) {
                    if ((int)w->pos.x == snake->head->x && (int)w->pos.y == snake->head->y) { collidedWall = true; break; }
                    w = w->next;
                }
                if (collidedWall) {
                    // lose a life or game over
                    if (lives > 1) {
                        lives--; // don't play game over sound
                        // respawn snake in center
                        free_snake(snake); snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);
                        rebuild_occupancy_from_snake(snake);
                        continue; // skip other checks this frame
                    } else {
                        // final life lost -> game over
                        lives = 0; PlaySound(hitSound); gameOver = 1;
                    }
                }
            }

            // Normal food eaten
            if (!gameOver && snake->head->x == (int)food.x && snake->head->y == (int)food.y) {
                PlaySound(eatSound);
                grow = 1;
                score += 10 * scoreMultiplier;
                fruitsEaten++;

                // place new normal food — avoid active bonus/power positions
                int fx, fy;
                int ax1 = bonusActive ? (int)bonusFruit.x : -1;
                int ay1 = bonusActive ? (int)bonusFruit.y : -1;
                int ax2 = powerActive ? (int)powerFruit.x : -1;
                int ay2 = powerActive ? (int)powerFruit.y : -1;
                if (place_random_food_not_on(snake, walls, &fx, &fy, ax1, ay1, ax2, ay2)) { food.x = (float)fx; food.y = (float)fy; }

                // spawn bonus every 8 normal fruits
                if (fruitsEaten % 8 == 0 && !bonusActive) {
                    PlaySound(bonusSound);
                    int bx, by;
                    if (place_random_food_not_on(snake, walls, &bx, &by, (int)food.x, (int)food.y, ax2, ay2)) {
                        bonusFruit.x = (float)bx; bonusFruit.y = (float)by; bonusStartTime = GetTime(); bonusActive = 1;
                    }
                }

                // spawn power (slow) fruit every 12 normal fruits
                if (fruitsEaten % 12 == 0 && !powerActive) {
                    int px, py;
                    if (place_random_food_not_on(snake, walls, &px, &py, (int)food.x, (int)food.y, bonusActive ? (int)bonusFruit.x : -1, bonusActive ? (int)bonusFruit.y : -1)) {
                        powerFruit.x = (float)px; powerFruit.y = (float)py; powerStartTime = GetTime(); powerActive = 1;
                    }
                }
            }

            // Bonus fruit handling
            if (bonusActive) {
                if (GetTime() - bonusStartTime > bonusDuration) {
                    bonusActive = 0; // disappear
                } else if (snake->head->x == (int)bonusFruit.x && snake->head->y == (int)bonusFruit.y) {
                    PlaySound(eatSound);
                    score += 20 * scoreMultiplier; // double normal * multiplier
                    grow = 1; bonusActive = 0;
                }
            }

            // Power fruit handling (green slow fruit)
            if (powerActive) {
                if (GetTime() - powerStartTime > powerDuration) {
                    powerActive = 0; // disappear if not eaten
                } else if (snake->head->x == (int)powerFruit.x && snake->head->y == (int)powerFruit.y) {
                    PlaySound(eatSound);
                    // apply slow effect
                    slowMode = 1; slowStartTime = GetTime();
                    // points for power fruit
                    score += 15 * scoreMultiplier; grow = 1; powerActive = 0;
                }
            }

            // boundary/self collision (self collision -> lose life or game over)
            if (!gameOver && check_collision(snake, GRID_WIDTH, GRID_HEIGHT)) {
                if (lives > 1) {
                    lives--;
                    free_snake(snake); snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);
                    rebuild_occupancy_from_snake(snake);
                } else {
                    lives = 0; PlaySound(hitSound); gameOver = 1;
                }
            }
        }

        // ------------------- Drawing -------------------
        // game area background
        DrawRectangle(0, 0, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE, (Color){10,10,10,255});

        // draw walls
        WallNode *w = walls;
        while (w) { DrawRectangle((int)w->pos.x * CELL_SIZE, (int)w->pos.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, GRAY); w = w->next; }

        if (!gameOver) {
            // Normal food
            DrawRectangle((int)food.x * CELL_SIZE, (int)food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, RED);

            // Bonus fruit (yellow, larger)
            if (bonusActive) {
                int bonusSize = CELL_SIZE + 6;
                DrawRectangle((int)bonusFruit.x * CELL_SIZE - 3, (int)bonusFruit.y * CELL_SIZE - 3, bonusSize, bonusSize, YELLOW);
                double remaining = bonusDuration - (GetTime() - bonusStartTime);
                char bonusText[32]; sprintf(bonusText, "BONUS: %.1fs", remaining);
                DrawText(bonusText, 10, GRID_HEIGHT * CELL_SIZE - 30, 20, YELLOW);
            }

            // Power fruit (green, slightly larger)
            if (powerActive) {
                int pSize = CELL_SIZE + 4;
                DrawRectangle((int)powerFruit.x * CELL_SIZE - 2, (int)powerFruit.y * CELL_SIZE - 2, pSize, pSize, GREEN);
                double prem = powerDuration - (GetTime() - powerStartTime);
                char ptext[32]; sprintf(ptext, "POWER: %.1fs", prem);
                DrawText(ptext, 150, GRID_HEIGHT * CELL_SIZE - 30, 20, GREEN);
            }

            // Snake
            draw_snake(snake, CELL_SIZE);

            // HUD
            char scoreText[128]; sprintf(scoreText, "Player: %s   Score: %d   Lives: %d   Mode: %s", playerName, score, lives, aiMode?"AI":"Human");
            DrawText(scoreText, 10, 10, 20, RAYWHITE);

            if (slowMode) DrawText("SLOWED!", 300, 10, 20, SKYBLUE);
        } else {
            DrawText("GAME OVER", 100, 100, 40, RED);
            char finalText[96]; sprintf(finalText, "Player: %s  |  Score: %d", playerName, score);
            DrawText(finalText, 100, 160, 25, RAYWHITE);
            DrawText("Press [R] to restart", 100, 200, 20, GRAY);
            DrawText("Press [L] to save score", 100, 230, 20, GRAY);

            if (IsKeyPressed(KEY_R)) {
                if (snake) free_snake(snake);
                snake = create_snake(GRID_WIDTH/2, GRID_HEIGHT/2);

                // regenerate walls/fruits based on difficulty
                free_walls(walls); walls = NULL;
                if (scoreMultiplier > 1) {
                    for (int i = 0; i < MAX_WALLS; i++) {
                        int wx, wy;
                        if (get_random_free_cell(snake, walls, &wx, &wy)) walls = add_wall(walls, wx, wy);
                    }
                }

                int fx, fy; if (get_random_free_cell(snake, walls, &fx, &fy)) { food.x = (float)fx; food.y = (float)fy; }

                fruitsEaten = 0; bonusActive = 0; powerActive = 0; slowMode = 0; score = 0; gameOver = 0;
                // reset lives to difficulty defaults
                if (scoreMultiplier == 1) lives = 3; else if (scoreMultiplier == 2) lives = 5; else lives = 8;
                SetTargetFPS(gameSpeed);
            }

            if (IsKeyPressed(KEY_L)) save_score(playerName, score);
        }

        // Live leaderboard at right
        draw_leaderboard_panel(GRID_WIDTH * CELL_SIZE, playerName);

        EndDrawing();
    }

    // Cleanup
    if (snake) free_snake(snake);
    free_walls(walls);
    UnloadSound(eatSound);
    UnloadSound(hitSound);
    UnloadSound(bonusSound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}


  
    

  

                             
              
