#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

#define SCREEN_WIDTH (2560)
#define SCREEN_HEIGHT (SCREEN_WIDTH * 9 / 16)

#define SCALE (SCREEN_WIDTH / 800)

#define WINDOW_TITLE "Particles"

#define MAX_PARTICLES 1000

#define NUM_BARRIERS 15

#define PARTICLE_INTERVAL 10
#define PARTICLE_SIZE (5.f * SCALE)

#define EMITTER_SIZE (20.f * SCALE)

#define TEXT_SIZE (16 * SCALE)
#define TEXT_OFFSET (8 * SCALE)

#define TIMESCALE 10
#define AVG_KEEP 10

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

//static const Rectangle barrier = (Rectangle) {500, 300, 200, 100};
static Rectangle barriers[NUM_BARRIERS];
static const Color barrierColor = SKYBLUE;

static bool b_gravity, b_brownian, b_nwtn3rd, b_solitaire;

typedef enum { AXIS_X, AXIS_Y } Axis;

inline void CalcVelocityAfterCollision(Box *e, const Axis a) {
    switch (a) {
    case AXIS_X:
        e->velocity.x *= -.8f;
        e->velocity.y *= .95f;
        break;
    case AXIS_Y:
        e->velocity.y *= -.8f;
        e->velocity.x *= .95f;
        break;
    }
}

void UpdateBoxPosition(Box *e, const float deltaTime) {
    float size = max(e->size, 0);
    e->pos = Vector2Clamp(Vector2Add(e->pos, Vector2Scale(e->velocity, deltaTime)),
                          (Vector2){0.f, 0.f},
                          (Vector2){SCREEN_WIDTH - size, SCREEN_HEIGHT - size});
    if(b_brownian) {
        e->velocity = Vector2Add(e->velocity, Vector2Scale(
                (Vector2) {(float) GetRandomValue(-10, 10) / 10.f, (float) GetRandomValue(-10, 10) / 10.f}, 1));
    }
    if(b_gravity) {
        e->velocity = Vector2Add(e->velocity, Vector2Scale((Vector2) {0, 5.f}, deltaTime));
    }

    const Rectangle cur = (Rectangle){e->pos.x, e->pos.y, size, size};
    if (e->pos.x == 0 || e->pos.x == SCREEN_WIDTH - size) {
        CalcVelocityAfterCollision(e, AXIS_X);
    }
    if (e->pos.y == 0 || e->pos.y == SCREEN_HEIGHT - size) {
        CalcVelocityAfterCollision(e, AXIS_Y);
    }

    for (int i = 0; i < NUM_BARRIERS; ++i) {
        const Rectangle barrier = barriers[i], top = {barrier.x, barrier.y, barrier.width, 1},
                        bot = {barrier.x, barrier.y + barrier.height, barrier.width, 1},
                        left = {barrier.x, barrier.y, 1, barrier.height},
                        right = {barrier.x + barrier.width, barrier.y, 1, barrier.height},
                        topCol = GetCollisionRec(cur, top), botCol = GetCollisionRec(cur, bot),
                        leftCol = GetCollisionRec(cur, left),
                        rightCol = GetCollisionRec(cur, right);
        const float vertIsect = max(topCol.width, botCol.width),
                    horzIsect = max(leftCol.height, rightCol.height);
        if (vertIsect > horzIsect) {
            if (topCol.width > 0) {
                e->pos.y = top.y - size - 1;
            } else if (botCol.width > 0) {
                e->pos.y = bot.y + 1;
            }
            if (topCol.width > 0 || botCol.width > 0) {
                CalcVelocityAfterCollision(e, AXIS_Y);
            }
        } else {
            if (leftCol.height > 0) {
                e->pos.x = left.x - size - 1;
            } else if (rightCol.height > 0) {
                e->pos.x = right.x + 1;
            }
            if (leftCol.height > 0 || rightCol.height > 0) {
                CalcVelocityAfterCollision(e, AXIS_X);
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
    Vector2 fuzz = (Vector2){(float) GetRandomValue(-64, 64) / 8.,
                             (float) GetRandomValue(-128, 128) / 8.};
    p->velocity = fuzz;
    if (b_nwtn3rd) {
        e->velocity = Vector2Subtract(e->velocity, Vector2Scale(fuzz, PARTICLE_SIZE / EMITTER_SIZE));
    }
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
    for (int i = 0; i < NUM_BARRIERS; ++i) {
        barriers[i] = (Rectangle){GetRandomValue(48, SCREEN_WIDTH - 128),
                                  GetRandomValue(48, SCREEN_HEIGHT - 128),
                                  GetRandomValue(48, 480),
                                  GetRandomValue(48, 480)};
        if (barriers[i].x + barriers[i].width > SCREEN_WIDTH
            || barriers[i].y + barriers[i].height > SCREEN_HEIGHT) {
            i--;
            continue;
        }
        for (int j = 0; j < i; ++j) {
            Rectangle collision = GetCollisionRec(barriers[i], barriers[j]);
            if (collision.height > 0 || collision.width > 0) {
                i--;
                break;
            }
        }
    }
}

void DrawBox(Box *b) {
    DrawRectangleV(b->pos, (Vector2){b->size, b->size}, b->color);
    if (!b_solitaire) {
        DrawRectangleLinesEx((Rectangle){b->pos.x, b->pos.y, b->size, b->size}, 2.f, BLACK);
    }
}

int main(void) {
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(500);
    b_gravity = true;
    b_brownian = false;
    b_nwtn3rd = false;
    b_solitaire = false;

    Emitter e = {(Vector2){SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f},
                 (Vector2){-80.f, -80.f},
                 (Color){0xFF, 0xFF, 0xFF, 0xFF},
                 EMITTER_SIZE,
                 {0},
                 0,
                 0};
    size_t frames = 0;

    generateRandomBarriers();


    double t = GetTime();
    SetRandomSeed(*(unsigned long *) &t);
    while (!WindowShouldClose()) {
        switch (GetKeyPressed()) {
        case KEY_R:
            generateRandomBarriers();
            e.count = 0;
            e.next = 0;
            break;
            case KEY_G:
                b_gravity = !b_gravity;
                break;
            case KEY_B:
                b_brownian = !b_brownian;
                break;
            case KEY_N:
                b_nwtn3rd = !b_nwtn3rd;
                break;
            case KEY_L:
                b_solitaire = !b_solitaire;
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
            static Vector2 hVel = (Vector2) {0.0f, 0.0f};
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                e.pos = GetMousePosition();
                e.velocity = Vector2Zero();
                hVel = Vector2Add(hVel,
                                  Vector2Scale(Vector2Divide(GetMouseDelta(), (Vector2) {SCREEN_WIDTH, SCREEN_HEIGHT}),
                                               1. / GetFrameTime()));
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                e.velocity = Vector2Scale(hVel, 1);
                hVel = Vector2Zero();
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
        static Vector2 histPos[AVG_KEEP] = {0}, histVel[AVG_KEEP] = {0};
        static char fpsbuf[128], posbuf[128], velbuf[128], flagsbuf[512];
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
        static const char *BOOLSTRINGS[2] = {"false", "true"};
        sprintf(flagsbuf,
                "Gravity [G]: %s | Brownian motion [B]: %s | Newton's 3rd Law [N]: %s | Windows "
                "Solitaire Mode [L]: %s",
                BOOLSTRINGS[!!b_gravity],
                BOOLSTRINGS[!!b_brownian],
                BOOLSTRINGS[!!b_nwtn3rd],
                BOOLSTRINGS[!!b_solitaire]);
        const Vector2 text_size = MeasureTextEx(GetFontDefault(), fpsbuf, TEXT_SIZE, 1);
        BeginDrawing();
        if (!b_solitaire) {
            ClearBackground(CLITERAL(Color){0x22, 0x22, 0x22, 0xFF});
        }
        for (size_t i = 0; i < e.count; i++) {
            // DrawCircleV(e.particles[i].pos, e.particles[i].size,
            // e.particles[i].color);
            // DrawRectangleV(e.particles[i].pos, (Vector2) {e.particles[i].size, e.particles[i].size},
            //                e.particles[i].color);
            DrawBox(&e.particles[i]);
        }
        for (int i = 0; i < NUM_BARRIERS; ++i) {
            DrawRectangleRec(barriers[i], barrierColor);
            DrawRectangleLinesEx(barriers[i], 4.f, BLACK);
        }
        DrawBox((Box *) &e);
        //DrawRectangleV(e.pos, (Vector2) {e.size, e.size}, RAYWHITE);
        DrawText(fpsbuf, TEXT_SIZE, text_size.y + TEXT_OFFSET, TEXT_SIZE, RAYWHITE);
        DrawText(posbuf, TEXT_SIZE, 2 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        DrawText(velbuf, TEXT_SIZE, 3 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        DrawText(flagsbuf,
                 TEXT_SIZE / 2,
                 (text_size.y / 2 + TEXT_OFFSET / 2),
                 TEXT_SIZE / 2,
                 RAYWHITE);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
