#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

#define SCREEN_WIDTH (1600)
#define SCREEN_HEIGHT (900)

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

static const Rectangle barrier = (Rectangle) {500, 300, 200, 100};
static const Color barrierColor = SKYBLUE;

void UpdateBoxPosition(Box *e, float deltaTime) {
    e->pos = Vector2Clamp(Vector2Add(e->pos, Vector2Scale(e->velocity, deltaTime)), (Vector2) {0.f, 0.f},
                          (Vector2) {SCREEN_WIDTH - e->size, SCREEN_HEIGHT - e->size});
    e->velocity = Vector2Add(e->velocity, Vector2Scale((Vector2) {0.f, 5.f}, deltaTime));
    Rectangle cur = (Rectangle) {e->pos.x, e->pos.y, e->size, e->size};
    Rectangle bCollide = GetCollisionRec(cur, barrier);

    if (e->pos.x == 0 || e->pos.x == SCREEN_WIDTH - e->size
        || (bCollide.height == e->size && e->pos.x > barrier.x - e->size
            && e->pos.x < (barrier.x + barrier.width))) {
        e->velocity.x *= -.8f;
        e->velocity.y *= .95f;
    }
    if (e->pos.y == 0 || e->pos.y == SCREEN_HEIGHT - e->size
        || (bCollide.width == e->size && e->pos.y > barrier.y - e->size
            && e->pos.y < (barrier.y + barrier.height))) {
        e->velocity.y *= -.8f;
        e->velocity.x *= .95f;
    }
    if (bCollide.height > 0 || bCollide.width > 0) {
        //fix position so these stupid cubes stop getting stuck in the wall
        //just give it a little nudge
        if (bCollide.width > bCollide.height) {
            if (e->pos.y > barrier.y - e->size && e->pos.y < barrier.y + barrier.height) {
                //entity is vertically stuck
                bool inLowerHalf = e->pos.y > barrier.y + barrier.height / 2;
                e->pos.y = inLowerHalf ? barrier.y + barrier.height : barrier.y - e->size;
                cur.y = e->pos.y;
            }
            bCollide = GetCollisionRec(cur, barrier);
            if (bCollide.width > 0 && e->pos.x > barrier.x - e->size
                && e->pos.x < barrier.x + barrier.width) {
                //entity is horizontally stuck
                bool inRightHalf = e->pos.x > barrier.x + barrier.width / 2;
                e->pos.x = inRightHalf ? barrier.x + barrier.width : barrier.x - e->size;
                cur.x = e->pos.x;
            }
        } else {
            if (e->pos.x > barrier.x - e->size && e->pos.x < barrier.x + barrier.width) {
                //entity is horizontally stuck
                bool inRightHalf = e->pos.x > barrier.x + barrier.width / 2;
                e->pos.x = inRightHalf ? barrier.x + barrier.width : barrier.x - e->size;
                cur.x = e->pos.x;
            }

            bCollide = GetCollisionRec(cur, barrier);
            if (bCollide.height > 0 && e->pos.y > barrier.y - e->size
                && e->pos.y < barrier.y + barrier.height) {
                //entity is vertically stuck
                bool inLowerHalf = e->pos.y > barrier.y + barrier.height / 2;
                e->pos.y = inLowerHalf ? barrier.y + barrier.height : barrier.y - e->size;
                cur.y = e->pos.y;
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
    hsv.x += 300. / increment;
    hsv.y -= .1 / increment;
    hsv.z -= .1 / increment;
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

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(1000);


    Emitter e = {(Vector2) {SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f}, (Vector2) {-80.f, -80.f},
                 (Color) {0xFF, 0xFF, 0xFF, 0xFF}, EMITTER_SIZE, {0}, 0, 0};
    size_t frames = 0;
    double t = GetTime();
    SetRandomSeed(*(unsigned long *) &t);
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime() * TIMESCALE;

        {
            static size_t hframes;
            static Vector2 hVel = (Vector2) {0.0f, 0.0f};
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                e.pos = GetMousePosition();
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
        Color testColor = (Color) {e.velocity.y > 0 ? 0xFF : 0x00, e.velocity.x > 0 ? 0xFF : 0x00, 0xFF, 0xFF};
        DrawRectangleRec(barrier, barrierColor);
        DrawRectangleV(e.pos, (Vector2) {e.size, e.size}, testColor);
        DrawText(fpsbuf, TEXT_SIZE, text_size.y + TEXT_OFFSET, TEXT_SIZE, RAYWHITE);
        DrawText(posbuf, TEXT_SIZE, 2 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        DrawText(velbuf, TEXT_SIZE, 3 * (text_size.y + TEXT_OFFSET), TEXT_SIZE, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
