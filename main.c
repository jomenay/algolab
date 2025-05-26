#include "raylib.h"
#include <stdlib.h>

#define MARIO_WALK_FRAME_COUNT  3
#define MARIO_WALK_FRAME_WIDTH  25
#define MARIO_WALK_FRAME_HEIGHT 49
#define MARIO_JUMP_WIDTH  25    
#define MARIO_JUMP_HEIGHT 49
#define MARIO_IDLE_WIDTH  25
#define MARIO_IDLE_HEIGHT 49
#define MARIO_BASE_HEIGHT MARIO_IDLE_HEIGHT

#define CLOUD_COUNT 100
#define CLOUD_SPACING 200
#define GROUND_START_X 0
#define GROUND_LENGTH 2000
#define MARIO_START_X (GROUND_START_X + 100)

#define MAX_BLOCKS 50
#define MAX_ENEMIES 10
#define MAX_COINS 20
#define BLOCK_SIZE 64
#define COIN_SIZE 32
#define ENEMY_SIZE 48

typedef enum GameScreen { TITLE, GAMEPLAY, SETTINGS } GameScreen;

typedef enum BlockType {
    BLOCK_QUESTION,
    BLOCK_STONE
} BlockType;

typedef struct {
    Rectangle rect;
    BlockType type;
    bool hasCoin;
    bool hit;
    Texture2D texture;
} Block;

typedef struct {
    Rectangle rect;
    bool active;
    int direction;
    float speed;
    Texture2D texture;
} Enemy;

typedef struct {
    Rectangle rect;
    bool collected;
    Texture2D texture;
} Coin;

void OturtMarioZemine(Vector2* marioPos, int groundY, int baseHeight) {
    marioPos->y = groundY - baseHeight;
}

bool CheckCollision(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width &&
        a.x + a.width > b.x &&
        a.y < b.y + b.height &&
        a.y + a.height > b.y;
}

// Mini level: platform zemine yakın, düşmanlar zeminde
void InitLevel(Block blocks[], int* blockCount, Enemy enemies[], int* enemyCount, Coin coins[], int* coinCount, int groundY,
    Texture2D soruBlok, Texture2D tasBlok, Texture2D coinPng, Texture2D dusmanPng) {
    int b = 0, e = 0, c = 0;

    // Platform parametreleri
    int platY = groundY - BLOCK_SIZE - 40; // Zemine daha yakın (40px yukarıda)
    int platStartX = GROUND_START_X + 250;
    int platBlockCount = 8; // Platformda toplam blok sayısı (tasblok + sorublok)

    // Platform: [tasblok][sorublok][tasblok][sorublok]...[tasblok]
    for (int i = 0; i < platBlockCount; i++) {
        BlockType type = (i % 2 == 0) ? BLOCK_STONE : BLOCK_QUESTION;
        blocks[b].rect = (Rectangle){ platStartX + i * BLOCK_SIZE, platY, BLOCK_SIZE, BLOCK_SIZE };
        blocks[b].type = type;
        blocks[b].hasCoin = (type == BLOCK_QUESTION);
        blocks[b].hit = false;
        blocks[b].texture = (type == BLOCK_QUESTION) ? soruBlok : tasBlok;
        b++;

        // Coin, soru bloğun üstünde
        if (type == BLOCK_QUESTION) {
            coins[c].rect = (Rectangle){ platStartX + i * BLOCK_SIZE + (BLOCK_SIZE - COIN_SIZE) / 2, platY - COIN_SIZE, COIN_SIZE, COIN_SIZE };
            coins[c].collected = false;
            coins[c].texture = coinPng;
            c++;
        }
    }

    // Düşmanlar: zeminde, platformun sol ve sağ taraflarında
    int enemyY = groundY - ENEMY_SIZE - 100; // Zemin yüksekliği 100, düşman tam üstünde
    int enemyX1 = platStartX - 120;
    int enemyX2 = platStartX + platBlockCount * BLOCK_SIZE + 40;
    enemies[e].rect = (Rectangle){ enemyX1, enemyY, ENEMY_SIZE, ENEMY_SIZE };
    enemies[e].active = true;
    enemies[e].direction = 1;
    enemies[e].speed = 2.0f;
    enemies[e].texture = dusmanPng;
    e++;

    enemies[e].rect = (Rectangle){ enemyX2, enemyY, ENEMY_SIZE, ENEMY_SIZE };
    enemies[e].active = true;
    enemies[e].direction = -1;
    enemies[e].speed = 2.0f;
    enemies[e].texture = dusmanPng;
    e++;

    *blockCount = b;
    *enemyCount = e;
    *coinCount = c;
}

// Düşmanlar tasbloklara çarpınca yön değiştirir, zeminde hareket eder
void UpdateEnemies(Enemy enemies[], int enemyCount, Block blocks[], int blockCount) {
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].active) continue;
        enemies[i].rect.x += enemies[i].direction * enemies[i].speed;

        // Tasbloklara çarpınca yön değiştir
        for (int j = 0; j < blockCount; j++) {
            if (blocks[j].type == BLOCK_STONE) {
                Rectangle enemyNext = enemies[i].rect;
                enemyNext.x += enemies[i].direction * enemies[i].speed;
                if (CheckCollision(enemyNext, blocks[j].rect)) {
                    enemies[i].direction *= -1;
                    break;
                }
            }
        }
        // Harita sınırından çıkmasın
        if (enemies[i].rect.x < GROUND_START_X) {
            enemies[i].rect.x = GROUND_START_X;
            enemies[i].direction = 1;
        }
        if (enemies[i].rect.x > GROUND_START_X + GROUND_LENGTH - ENEMY_SIZE) {
            enemies[i].rect.x = GROUND_START_X + GROUND_LENGTH - ENEMY_SIZE;
            enemies[i].direction = -1;
        }
    }
}

void HandleCollisions(Vector2* marioPos, Rectangle marioCollider, bool* isJumping, float* velocityY,
    Block blocks[], int blockCount, Enemy enemies[], int enemyCount, Coin coins[], int coinCount, int* score, int groundY) {
    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].rect.width == 0) continue;
        if (CheckCollision(marioCollider, blocks[i].rect)) {
            if (marioCollider.y + marioCollider.height <= blocks[i].rect.y + 10 && *velocityY > 0) {
                marioPos->y = blocks[i].rect.y - marioCollider.height;
                *velocityY = 0;
                *isJumping = false;
                if (blocks[i].type == BLOCK_QUESTION && !blocks[i].hit) {
                    blocks[i].hit = true;
                    if (blocks[i].hasCoin) *score += 100;
                }
            }
        }
    }
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].active) continue;
        if (CheckCollision(marioCollider, enemies[i].rect)) {
            // Mario düşmanın üstünden geliyorsa (yani zıplama ile)
            if (marioCollider.y + marioCollider.height - 5 <= enemies[i].rect.y && *velocityY > 0) {
                enemies[i].active = false;
                *velocityY = -10.0f / 2;
                *score += 200;
            }
            // Mario düşmana yandan veya alttan çarparsa (oyun mantığına göre burada Mario'ya zarar verilebilir)
        }
    }
    for (int i = 0; i < coinCount; i++) {
        if (coins[i].collected || coins[i].rect.width == 0) continue;
        if (CheckCollision(marioCollider, coins[i].rect)) {
            coins[i].collected = true;
            *score += 50;
        }
    }
}

void DrawGameElements(Block blocks[], int blockCount, Enemy enemies[], int enemyCount, Coin coins[], int coinCount) {
    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].rect.width == 0) continue;
        DrawTexture(blocks[i].texture, blocks[i].rect.x, blocks[i].rect.y, WHITE);
    }
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].active) continue;
        if (enemies[i].direction == -1) {
            DrawTexture(enemies[i].texture, enemies[i].rect.x, enemies[i].rect.y, WHITE);
        }
        else {
            Rectangle flipped = { 0, 0, (float)-enemies[i].texture.width, (float)enemies[i].texture.height };
            DrawTextureRec(enemies[i].texture, flipped, (Vector2) { enemies[i].rect.x + ENEMY_SIZE, enemies[i].rect.y }, WHITE);
        }
    }
    for (int i = 0; i < coinCount; i++) {
        if (!coins[i].collected && coins[i].rect.width != 0) {
            DrawTexture(coins[i].texture, coins[i].rect.x, coins[i].rect.y, WHITE);
        }
    }
}

int main() {
    const int screenWidth = 1200;
    const int screenHeight = 800;

    InitWindow(screenWidth, screenHeight, "Super Mario - Raylib");
    SetTargetFPS(60);
    InitAudioDevice();
    SetMasterVolume(1.0f);

    GameScreen currentScreen = TITLE;
    Rectangle startButton = { screenWidth / 2 - 400, 340, 300, 80 };
    Rectangle exitButton = { screenWidth / 2 - 400, 460, 300, 80 };
    Rectangle resumeButton = { screenWidth / 2 - 100, 200, 200, 50 };
    Rectangle soundButton = { screenWidth / 2 - 100, 280, 200, 50 };
    Rectangle quitButton = { screenWidth / 2 - 100, 360, 200, 50 };
    Rectangle settingsIconRect = { screenWidth - 60, 20, 40, 40 };

    const int groundHeight = 100;
    const int groundY = screenHeight;

    Vector2 marioPosition = { MARIO_START_X, groundY - MARIO_BASE_HEIGHT - groundHeight };
    float marioSpeed = 5.0f;
    bool isJumping = false;
    float jumpForce = 10.0f;
    float gravity = 0.5f;
    float velocityY = 0;
    int marioDirection = 1;
    int walkFrame = 0;
    int walkFrameCounter = 0;
    int walkFrameSpeed = 8;
    int score = 0;

    Texture2D cloudTexture = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/cloud.png");
    Vector2 clouds[CLOUD_COUNT];
    int cloudIndex = 0;
    for (int i = 0; i < CLOUD_COUNT * 2 && cloudIndex < CLOUD_COUNT; i++) {
        float x = GROUND_START_X + i * CLOUD_SPACING;
        if (x < MARIO_START_X) continue;
        if (x > GROUND_START_X + GROUND_LENGTH - cloudTexture.width) break;
        clouds[cloudIndex].x = x;
        clouds[cloudIndex].y = 50 + rand() % 150;
        cloudIndex++;
    }
    int visibleCloudCount = cloudIndex;

    // PNG'leri yükle
    Texture2D titleImage = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/background.jpg");
    Texture2D marioWalkTextures[MARIO_WALK_FRAME_COUNT] = {
        LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/MarioWalking1.png"),
        LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/MarioWalking2.png"),
        LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/MarioWalking3.png")
    };
    Texture2D marioJumpTexture = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/MarioJump.png");
    Texture2D marioIdleTexture = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/MarioIdle.png");
    Texture2D buttonTexture = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/button.png");
    Texture2D settingsIconTexture = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/settings.png");
    Texture2D coinPng = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/coin.png");
    Texture2D dusmanPng = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/dusman.png");
    Texture2D soruBlok = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/sorublok.png");
    Texture2D tasBlok = LoadTexture("C:/Users/ARDA/source/repos/algolab/x64/Debug/tasblok.png");
    Music titleMusic = LoadMusicStream("C:/Users/ARDA/source/repos/algolab/x64/Debug/titleMusic.wav");
    Music gameMusic = LoadMusicStream("C:/Users/ARDA/source/repos/algolab/x64/Debug/gameMusic.wav");

    // Parkur elementleri
    Block blocks[MAX_BLOCKS] = { 0 };
    Enemy enemies[MAX_ENEMIES] = { 0 };
    Coin coins[MAX_COINS] = { 0 };
    int blockCount = 0, enemyCount = 0, coinCount = 0;
    InitLevel(blocks, &blockCount, enemies, &enemyCount, coins, &coinCount, groundY - groundHeight, soruBlok, tasBlok, coinPng, dusmanPng);

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight - groundHeight / 2.0f };
    camera.target = (Vector2){ marioPosition.x + MARIO_WALK_FRAME_WIDTH / 2, groundY - groundHeight / 2 };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    bool isMuted = false;

    while (!WindowShouldClose()) {
        if (currentScreen == TITLE && titleMusic.stream.buffer != NULL) {
            UpdateMusicStream(titleMusic);
        }
        else if (currentScreen == GAMEPLAY && gameMusic.stream.buffer != NULL) {
            UpdateMusicStream(gameMusic);
        }

        Vector2 mousePoint = GetMousePosition();

        switch (currentScreen) {
        case TITLE:
            if (CheckCollisionPointRec(mousePoint, startButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                currentScreen = GAMEPLAY;
                if (titleMusic.stream.buffer != NULL) StopMusicStream(titleMusic);
                if (gameMusic.stream.buffer != NULL) PlayMusicStream(gameMusic);
            }
            else if (CheckCollisionPointRec(mousePoint, exitButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                break;
            }
            break;

        case GAMEPLAY: {
            bool isMoving = false;
            float marioRightLimit = GROUND_START_X + GROUND_LENGTH - MARIO_WALK_FRAME_WIDTH;

            if (IsKeyDown(KEY_RIGHT)) {
                marioPosition.x += marioSpeed;
                if (marioPosition.x > marioRightLimit) marioPosition.x = marioRightLimit;
                isMoving = true;
                marioDirection = 1;
            }
            if (IsKeyDown(KEY_LEFT)) {
                if (marioPosition.x > GROUND_START_X) {
                    marioPosition.x -= marioSpeed;
                    if (marioPosition.x < GROUND_START_X) marioPosition.x = GROUND_START_X;
                    isMoving = true;
                    marioDirection = -1;
                }
            }

            if (isMoving && !isJumping) {
                walkFrameCounter++;
                if (walkFrameCounter >= walkFrameSpeed) {
                    walkFrameCounter = 0;
                    walkFrame++;
                    if (walkFrame >= MARIO_WALK_FRAME_COUNT) walkFrame = 0;
                }
            }
            else {
                walkFrame = 0;
                walkFrameCounter = 0;
            }

            if (IsKeyPressed(KEY_SPACE) && !isJumping) {
                velocityY = -jumpForce;
                isJumping = true;
            }

            velocityY += gravity;
            marioPosition.y += velocityY;

            int zeminY = groundY - groundHeight;
            if (marioPosition.y >= zeminY - MARIO_BASE_HEIGHT) {
                OturtMarioZemine(&marioPosition, zeminY, MARIO_BASE_HEIGHT);
                isJumping = false;
                velocityY = 0;
            }

            Rectangle marioCollider = { marioPosition.x, marioPosition.y, MARIO_WALK_FRAME_WIDTH, MARIO_WALK_FRAME_HEIGHT };
            UpdateEnemies(enemies, enemyCount, blocks, blockCount);
            HandleCollisions(&marioPosition, marioCollider, &isJumping, &velocityY, blocks, blockCount, enemies, enemyCount, coins, coinCount, &score, groundY - groundHeight);

            if (CheckCollisionPointRec(mousePoint, settingsIconRect) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                currentScreen = SETTINGS;
            }

            camera.target = (Vector2){ marioPosition.x + MARIO_WALK_FRAME_WIDTH / 2, groundY - groundHeight / 2 };
            break;
        }

        case SETTINGS:
            if (CheckCollisionPointRec(mousePoint, resumeButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                currentScreen = GAMEPLAY;
            }
            else if (CheckCollisionPointRec(mousePoint, soundButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                isMuted = !isMuted;
                SetMasterVolume(isMuted ? 0.0f : 1.0f);
            }
            else if (CheckCollisionPointRec(mousePoint, quitButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                break;
            }
            break;
        }

        BeginDrawing();
        ClearBackground(SKYBLUE);

        if (currentScreen == GAMEPLAY) {
            BeginMode2D(camera);

            // Bulutlar
            for (int i = 0; i < visibleCloudCount; i++) {
                if (clouds[i].x >= GROUND_START_X && clouds[i].x <= GROUND_START_X + GROUND_LENGTH - cloudTexture.width) {
                    DrawTexture(cloudTexture, clouds[i].x, clouds[i].y, WHITE);
                }
            }

            // Zemin (yeşil)
            DrawRectangle(GROUND_START_X, groundY - 100, GROUND_LENGTH, 100, GREEN);

            // Parkur elementleri
            DrawGameElements(blocks, blockCount, enemies, enemyCount, coins, coinCount);

            // Mario çizimi
            Vector2 drawPos = marioPosition;
            int offsetY = 0;
            if (isJumping)
                offsetY = MARIO_BASE_HEIGHT - MARIO_JUMP_HEIGHT;
            else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_LEFT))
                offsetY = MARIO_BASE_HEIGHT - MARIO_WALK_FRAME_HEIGHT;
            else
                offsetY = MARIO_BASE_HEIGHT - MARIO_IDLE_HEIGHT;
            drawPos.y += offsetY;

            if (isJumping) {
                Rectangle jumpSource = { 0, 0, marioDirection == 1 ? MARIO_JUMP_WIDTH : -MARIO_JUMP_WIDTH, MARIO_JUMP_HEIGHT };
                if (marioDirection == -1) drawPos.x += MARIO_JUMP_WIDTH;
                DrawTextureRec(marioJumpTexture, jumpSource, drawPos, WHITE);
            }
            else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_LEFT)) {
                Texture2D currentWalkTexture = marioWalkTextures[walkFrame];
                if (marioDirection == 1) {
                    DrawTexture(currentWalkTexture, drawPos.x, drawPos.y, WHITE);
                }
                else {
                    Rectangle src = { 0, 0, (float)-currentWalkTexture.width, (float)currentWalkTexture.height };
                    DrawTextureRec(currentWalkTexture, src, drawPos, WHITE);
                }
            }
            else {
                if (marioDirection == 1) {
                    DrawTexture(marioIdleTexture, drawPos.x, drawPos.y, WHITE);
                }
                else {
                    Rectangle src = { 0, 0, (float)-marioIdleTexture.width, (float)marioIdleTexture.height };
                    DrawTextureRec(marioIdleTexture, src, drawPos, WHITE);
                }
            }

            EndMode2D();

            DrawText("Super Mario - Raylib", 10, 10, 20, BLACK);
            DrawText(TextFormat("Skor: %d", score), 10, 40, 20, BLACK);
            DrawTexture(settingsIconTexture, settingsIconRect.x, settingsIconRect.y, WHITE);
        }
        else if (currentScreen == TITLE) {
            DrawTexture(titleImage, 0, 0, WHITE);
            DrawText("SUPER MANO BROS", screenWidth / 2 - MeasureText("SUPER MANO BROS", 50) / 1, 100, 75, YELLOW);

            DrawTexture(buttonTexture, startButton.x + (startButton.width - buttonTexture.width) / 2,
                startButton.y + (startButton.height - buttonTexture.height) / 2, WHITE);
            DrawTexture(buttonTexture, exitButton.x + (exitButton.width - buttonTexture.width) / 2,
                exitButton.y + (exitButton.height - buttonTexture.height) / 2, WHITE);

            if (CheckCollisionPointRec(mousePoint, startButton))
                DrawRectangle(startButton.x, startButton.y, startButton.width, startButton.height, Fade(YELLOW, 0.4f));
            if (CheckCollisionPointRec(mousePoint, exitButton))
                DrawRectangle(exitButton.x, exitButton.y, exitButton.width, exitButton.height, Fade(RED, 0.3f));

            DrawText("BASLA", startButton.x + startButton.width / 2 - MeasureText("BASLA", 40) / 2,
                startButton.y + startButton.height / 2 - 20, 45, BLACK);
            DrawText("CIKIS", exitButton.x + exitButton.width / 2 - MeasureText("CIKIS", 40) / 2,
                exitButton.y + exitButton.height / 2 - 20, 42, BLACK);
        }
        else if (currentScreen == SETTINGS) {
            DrawRectangleGradientV(0, 0, screenWidth, screenHeight, DARKGRAY, GRAY);

            DrawRectangleRec(resumeButton, CheckCollisionPointRec(mousePoint, resumeButton) ? GREEN : DARKGREEN);
            DrawRectangleRec(soundButton, CheckCollisionPointRec(mousePoint, soundButton) ? YELLOW : GOLD);
            DrawRectangleRec(quitButton, CheckCollisionPointRec(mousePoint, quitButton) ? RED : MAROON);

            DrawText("DEVAM ET", resumeButton.x + resumeButton.width / 2 - MeasureText("DEVAM ET", 30) / 2,
                resumeButton.y + resumeButton.height / 2 - 15, 30, BLACK);
            DrawText(isMuted ? "SES AÇ" : "SES KAPAT", soundButton.x + soundButton.width / 2 - MeasureText(isMuted ? "SES AÇ" : "SES KAPAT", 30) / 2,
                soundButton.y + soundButton.height / 2 - 15, 30, BLACK);
            DrawText("CIKIS YAP", quitButton.x + quitButton.width / 2 - MeasureText("CIKIS YAP", 30) / 2,
                quitButton.y + quitButton.height / 2 - 15, 30, BLACK);
        }

        EndDrawing();
    }

    // Kaynakları serbest bırak
    UnloadTexture(titleImage);
    for (int i = 0; i < MARIO_WALK_FRAME_COUNT; i++) UnloadTexture(marioWalkTextures[i]);
    UnloadTexture(marioJumpTexture);
    UnloadTexture(marioIdleTexture);
    UnloadTexture(buttonTexture);
    UnloadTexture(settingsIconTexture);
    UnloadTexture(cloudTexture);
    UnloadTexture(coinPng);
    UnloadTexture(dusmanPng);
    UnloadTexture(soruBlok);
    UnloadTexture(tasBlok);

    if (titleMusic.stream.buffer != NULL) UnloadMusicStream(titleMusic);
    if (gameMusic.stream.buffer != NULL) UnloadMusicStream(gameMusic);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
