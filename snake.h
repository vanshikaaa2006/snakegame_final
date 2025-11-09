#ifndef SNAKE_H
#define SNAKE_H

typedef struct Node {
    int x, y;
    struct Node* next;
} Node;

typedef struct Snake {
    Node* head;
    int direction; // 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT
} Snake;

Snake* create_snake(int startX, int startY);
void move_snake(Snake* snake, int grow);
int check_collision(Snake* snake, int width, int height);
void draw_snake(Snake* snake, int cellSize);
void free_snake(Snake* snake);

#endif
