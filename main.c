#include "raylib.h"

typedef struct Fighter{
    Rectangle body;
    float velocityY ;
    bool isJumping;
    int health;
}Fighter;


int main(void){
    const int screenwidth = 1000;
    const int screenheight = 600;

    InitWindow(screenwidth,screenheight,"dih-fighter");

    Fighter player = {
        .body = {200, 400, 60, 120},
        .velocityY = 0,
        .isJumping = false,
        .health = 100
    };

    Fighter enemy = {
        .body = {700, 400, 60, 120},
        .velocityY = 0,
        .isJumping = false,
        .health = 100
    };

    const float gravity = 0.7f;
    const float jumpForce = -15.0f;

    SetTargetFPS(60);

    while(!WindowShouldClose()){

        if(IsKeyDown(KEY_A)){
            player.body.x -= 5;
        }

        if(IsKeyDown(KEY_D)){
            player.body.x += 5;
        }

        if(IsKeyPressed(KEY_W) && !player.isJumping){
            player.velocityY = jumpForce;
            player.isJumping = true;
        }

        player.velocityY += gravity;
        player.body.y = player.velocityY;

        if (player.body.y >= 400)
        {
            player.body.y = 400;
            player.velocityY = 0;
            player.isJumping = false;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangle(0,520,screenwidth,80,DARKGRAY);


        //health bars
        DrawRectangle(20,20,player.health*2,30,GREEN);
        DrawRectangle(780,20,enemy.health*2,30,RED);


        //fighters
        DrawRectangleRec(player.body,BLUE);
        DrawRectangleRec(enemy.body,RED);
        DrawText("A/D = Move", 20, 70, 20, WHITE);
        DrawText("W = Jump", 20, 100, 20, WHITE);
        EndDrawing();
    }


    CloseWindow();

}