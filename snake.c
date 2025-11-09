#include "raylib.h"
#include "snake.h"
#include <stdlib.h>

Snake* create_snake(int startX, int startY) {
    Snake* snake = (Snake*)malloc(sizeof(Snake));
    Node* head = (Node*)malloc(sizeof(Node));
    head->x = startX;
    head->y = startY;
    head->next = NULL;
    snake->head = head;
    snake->direction = 1; // Start moving right
    return snake;
}

void move_snake(Snake* snake, int grow) {
    Node* newHead = (Node*)malloc(sizeof(Node));
    newHead->x = snake->head->x;
    newHead->y = snake->head->y;

    switch (snake->direction) {
        case 0: newHead->y -= 1; break;
        case 1: newHead->x += 1; break;
        case 2: newHead->y += 1; break;
        case 3: newHead->x -= 1; break;
    }

    newHead->next = snake->head;
    snake->head = newHead;

    if (!grow) {
        Node* temp = snake->head;
        while (temp->next && temp->next->next)
            temp = temp->next;
        free(temp->next);
        temp->next = NULL;
    }
}

void draw_snake(Snake* snake, int cellSize) {
    Node* current = snake->head;
    while (current) {
        DrawRectangle(current->x * cellSize, current->y * cellSize, cellSize, cellSize, GREEN);
        current = current->next;
    }
}

int check_collision(Snake* snake, int width, int height) {
    Node* head = snake->head;
    if (head->x < 0 || head->x >= width || head->y < 0 || head->y >= height)
        return 1;

    Node* body = head->next;
    while (body) {
        if (body->x == head->x && body->y == head->y)
            return 1;
        body = body->next;
    }
    return 0;
}

void free_snake(Snake* snake) {
    Node* current = snake->head;
    while (current) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
    free(snake);
}
