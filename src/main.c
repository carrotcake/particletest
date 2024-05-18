#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

#define SCREEN_WIDTH (2560)
#define SCREEN_HEIGHT (SCREEN_WIDTH * 9 / 16)

#define SCALE (SCREEN_WIDTH / 800)

#define WINDOW_TITLE "Particles"

#define MAX_PARTICLES 1000

#define PARTICLE_INTERVAL 10
#define PARTICLE_SIZE (5.f * SCALE)

#define EMITTER_SIZE (20.f * SCALE)

#define TEXT_SIZE (15 * SCALE)
#define TEXT_OFFSET (8 * SCALE)

#define TIMESCALE 10
#define AVG_KEEP 10

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Color color;
    float size;
} Box;

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Color color;
    float size;
    Box particles[MAX_PARTICLES];
    size_t count, next;
} Emitter;

typedef struct {
    Vector2 a, b;
} Line;

//static const Rectangle barrier = (Rectangle) {500, 300, 200, 100};
static Rectangle barriers[10];
static const Color barrierColor = SKYBLUE;

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))

void UpdateBoxPosition(Box *e, float deltaTime) {
    float size = max(e->size, 0);
    e->pos = Vector2Clamp(Vector2Add(e->pos, Vector2Scale(e->velocity, deltaTime)),
                          (Vector2){0.f, 0.f},
                          (Vector2){SCREEN_WIDTH - size, SCREEN_HEIGHT - size});
    e->velocity = Vector2Add(e->velocity, Vector2Scale((Vector2) {0.f, 5.f}, deltaTime));

    Rectangle cur = (Rectangle){e->pos.x, e->pos.y, size, size};
    if (e->pos.x == 0 || e->pos.x == SCREEN_WIDTH - size) {
        e->velocity.x *= -.8f;
        e->velocity.y *= .95f;
    }
    if (e->pos.y == 0 || e->pos.y == SCREEN_HEIGHT - size) {
        e->velocity.y *= -.8f;
        e->velocity.x *= .95f;
    }

    for(int i = 0; i < 10; ++i) {
        Rectangle barrier = barriers[i];
        Rectangle top = {barrier.x, barrier.y, barrier.width, 1};
        Rectangle bot = {barrier.x, barrier.y + barrier.height, barrier.width, 1};
        Rectangle left = {barrier.x, barrier.y, 1, barrier.height};
        Rectangle right = {barrier.x + barrier.width, barrier.y, 1, barrier.height};

        Rectangle topCol = GetCollisionRec(cur, top);
        Rectangle botCol = GetCollisionRec(cur, bot);
        Rectangle leftCol = GetCollisionRec(cur, left);
        Rectangle rightCol = GetCollisionRec(cur, right);

        float vertIsect = max(topCol.width, botCol.width),
              horzIsect = max(leftCol.height, rightCol.height);
        if (vertIsect > horzIsect) {
            if (topCol.width > 0) {
                e->pos.y = top.y - size - 1;
            } else if (botCol.width > 0) {
                e->pos.y = bot.y + 1;
            }
            if (topCol.width > 0 || botCol.width > 0) {
                e->velocity.y *= -.8f;
                e->velocity.x *= .95f;
            }
        } else {
            if (leftCol.height > 0) {
                e->pos.x = left.x - size - 1;
            } else if (rightCol.height > 0) {
                e->pos.x = right.x + 1;
            }
            if (leftCol.height > 0 || rightCol.height > 0) {
                e->velocity.x *= -.8f;
                e->velocity.y *= .95f;
            }
        }
    }
}

void UpdateParticle(Box *p) {
    const double increment = (double) (MAX_PARTICLES);

    Vector3 hsv = ColorToHSV(p->color);
    if (hsv.x >= 360.f) {
        hsv.x = 10;
    }
    hsv.x += 720. / increment;
    // hsv.y -= .1 / increment;
    //hsv.z -= .1 / increment;
    p->size -= PARTICLE_SIZE / increment;
    p->color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
}

void emitParticle(Emitter *e) {
    Box *p = &e->particles[e->next];
    p->pos = (Vector2) {e->pos.x + e->size / 2, e->pos.y + e->size / 2};
    Vector2 fuzz = (Vector2) {(float) GetRandomValue(-64, 64) / 8., (float) GetRandomValue(-128, 64) / 8.};
    p->velocity = fuzz;
    p->size = PARTICLE_SIZE;
    p->color = RED;
    if (e->next + 1 < MAX_PARTICLES && e->next >= e->count) {
        e->count++;
    }
    e->next++;
    if (e->next >= MAX_PARTICLES) {
        e->next = 0;
    }
}

void generateRandomBarriers(void){
    for(int i = 0; i<10;++i){
        barriers[i] = (Rectangle){GetRandomValue(150, SCREEN_WIDTH - 300),
                                  GetRandomValue(150, SCREEN_HEIGHT - 300),
                                  GetRandomValue(50, 300),
                                  GetRandomValue(50, 300)};
    }
}

int main(void) {
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(1000);


    Emitter e = {(Vector2) {SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f}, (Vector2) {-80.f, -80.f},
                 (Color) {0xFF, 0xFF, 0xFF, 0xFF}, EMITTER_SIZE, {0}, 0, 0};
    size_t frames = 0;

    generateRandomBarriers();


    double t = GetTime();
    SetRandomSeed(*(unsigned long *) &t);
    while (!WindowShouldClose()) {
        switch (GetKeyPressed()) {
        case KEY_R:
            generateRandomBarriers();
            break;
        default:
            break;
        }
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
            e.velocity = Vector2Clamp(Vector2Add(e.velocity, (Vector2){0., -0.5}),
                                      (Vector2){-150, -150},
                                      (Vector2){150, 150});
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            e.velocity = Vector2Clamp(Vector2Add(e.velocity, (Vector2){0., 0.5}),
                                      (Vector2){-150, -150},
                                      (Vector2){150, 150});
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            e.velocity = Vector2Clamp(Vector2Add(e.velocity, (Vector2){-0.5, 0.}),
                                      (Vector2){-150, -150},
                                      (Vector2){150, 150});
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            e.velocity = Vector2Clamp(Vector2Add(e.velocity, (Vector2){0.5, 0.}),
                                      (Vector2){-150, -150},
                                      (Vector2){150, 150});
        }

        float deltaTime = GetFrameTime() * TIMESCALE;

        {
            static size_t hframes;
            static Vector2 hVel = (Vector2) {0.0f, 0.0f};
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                e.pos = GetMousePosition();
                e.velocity = Vector2Zero();
                hVel = Vector2Add(hVel,
                                  Vector2Scale(Vector2Divide(GetMouseDelta(), (Vector2) {SCREEN_WIDTH, SCREEN_HEIGHT}),
                                               1. / GetFrameTime()));
                hframes++;
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                e.velocity = Vector2Scale(hVel, 1);
                hVel = Vector2Zero();
                hframes = 0;
            }
        }
        // update emitter position
        UpdateBoxPosition((Box *) &e, deltaTime);
        // update particle positions
        for (size_t i = 0; i < e.count; i++) {
            UpdateBoxPosition(&e.particles[i], deltaTime);
            if (frames % PARTICLE_INTERVAL == 0) {
                UpdateParticle(&e.particles[i]);
            }
        }
        // create new particle if it's time
        if (++frames % PARTICLE_INTERVAL == 0) {
            emitParticle(&e);
        }

        BeginDrawing();
        ClearBackground(CLITERAL(Color) {0x22, 0x22, 0x22, 0xFF});
        static Vector2 histPos[AVG_KEEP] = {0}, histVel[AVG_KEEP] = {0};
        static unsigned short hframe = 0;
        static char fpsbuf[128], posbuf[128], velbuf[128];
        histPos[frames % AVG_KEEP] = e.pos;
        histVel[frames % AVG_KEEP] = e.velocity;
        static Vector2 avgPos = {0}, avgVel = {0};
        if (frames % AVG_KEEP == 0) {
            avgPos = Vector2Zero();
            avgVel = Vector2Zero();
            for (size_t i = 0; i < AVG_KEEP; i++) {
                avgPos = Vector2Add(avgPos, Vector2Scale(histPos[i], .1));
                avgVel = Vector2Add(avgVel, Vector2Scale(histVel[i], .1));
            }
        }
        sprintf(fpsbuf, "FPS: %d", GetFPS());
        sprintf(posbuf, "Position: (%.4f, %.4f)", avgPos.x, avgPos.y);
        sprintf(velbuf, "Velocity: (%.4f, %.4f)", avgVel.x, avgVel.y);
        const Vector2 text_size = MeasureTextEx(GetFontDefault(), fpsbuf, TEXT_SIZE, 1);
        for (size_t i = 0; i < e.count; i++) {
            // DrawCircleV(e.particles[i].pos, e.particles[i].size,
            // e.particles[i].color);
            DrawRectangleV(e.particles[i].pos, (Vector2) {e.particles[i].size, e.particles[i].size},
                           e.particles[i].color);
        }
        for(int i = 0; i < 10; ++i){
            DrawRectangleRec(barriers[i], barrierColor);
        }
        DrawRectangleV(e.pos, (Vector2) {e.size, e.size}, RAYWHITE);
        DrawText(fpsbuf, TEXT_SIZE, text_size.y + TEXT_OFFSET, TEXT_SIZE, RAYWHITE);
        DrawText(posbuf, TEXT_SIZE, 2 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        DrawText(velbuf, TEXT_SIZE, 3 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
