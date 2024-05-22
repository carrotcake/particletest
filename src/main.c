#include "raylib.h"
#include "raymath.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH (2560)
#define SCREEN_HEIGHT (SCREEN_WIDTH * 9 / 16)

#define SCALE (SCREEN_WIDTH / 800)

#define WINDOW_TITLE "Particles"

#define MAX_PARTICLES 1280

#define NUM_BARRIERS 15

#define PARTICLE_INTERVAL 1
#define PARTICLE_SIZE (5.f * SCALE)

#define EMITTER_SIZE (20.f * SCALE)
#define MAX_ESIZE (30.f * SCALE)
#define MIN_ESIZE (5.f * SCALE)

#define TEXT_SIZE (12 * SCALE)
#define TEXT_OFFSET (8 * SCALE)

#define TIMESCALE 10
#define AVG_KEEP 25

#define NUMTHREADS 4

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define flteq(a, b) (fabs((a) - (b)) < .5)

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

static Emitter e = {(Vector2){SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f},
                    (Vector2){-80.f, -80.f},
                    (Color){0xFF, 0xFF, 0xFF, 0xFF},
                    EMITTER_SIZE,
                    {0},
                    0,
                    0};

//static const Rectangle barrier = (Rectangle) {500, 300, 200, 100};
static Rectangle barriers[NUM_BARRIERS];
static const Color barrierColor = SKYBLUE;
static char fpsbuf[128], posbuf[128], velbuf[128], flagsbuf[512];
static bool b_gravity, b_brownian, b_nwtn3rd, b_repulsion, b_solitaire, b_rocket[4] = {0},
                                                                        b_menuopen;
static size_t frames = 0;
typedef enum { UP, DOWN, LEFT, RIGHT } Dir;
typedef enum { AXIS_X, AXIS_Y } Axis;
static double repulsion_radius = 1.75, repulsion_factor = 1.25, brown_factor = .25;

inline void CalcVelocityAfterCollision(Box *e, const Axis a) {
    double friction = (1 - ((e->size) / (MAX_ESIZE * 2)));
    switch (a) {
    case AXIS_X:
        e->velocity.x *= -friction;
        e->velocity.y *= friction;
        break;
    case AXIS_Y:
        e->velocity.y *= -friction;
        e->velocity.x *= friction;
        break;
    }
}

void UpdateBoxPosition(Box *b, const float deltaTime) {
    float size = max(b->size, 0);
    b->pos = Vector2Clamp(Vector2Add(b->pos, Vector2Scale(b->velocity, deltaTime)),
                          (Vector2){1.f, 1.f},
                          (Vector2){SCREEN_WIDTH - size, SCREEN_HEIGHT - size});
    double speed = Vector2Length(b->velocity);

    const Rectangle cur = (Rectangle){b->pos.x, b->pos.y, size, size};
    if (speed > .01 && (b->pos.x == 1 || b->pos.x == SCREEN_WIDTH - size)) {
        CalcVelocityAfterCollision(b, AXIS_X);
    }
    if (speed > .01 && (b->pos.y == 1 || b->pos.y == SCREEN_HEIGHT - size)) {
        CalcVelocityAfterCollision(b, AXIS_Y);
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
                b->pos.y = top.y - size;
            } else if (botCol.width > 0) {
                b->pos.y = bot.y + 1;
            }
            if (topCol.width > 0 || botCol.width > 0) {
                CalcVelocityAfterCollision(b, AXIS_Y);
            }
        } else {
            if (leftCol.height > 0) {
                b->pos.x = left.x - size;
            } else if (rightCol.height > 0) {
                b->pos.x = right.x + 1;
            }
            if (leftCol.height > 0 || rightCol.height > 0) {
                CalcVelocityAfterCollision(b, AXIS_X);
            }
        }
    }
    if (b_gravity) {
        b->velocity = Vector2Add(b->velocity, Vector2Scale((Vector2){0, 5.f}, deltaTime));
    }
    speed = Vector2Length(b->velocity);
    if (b != (Box *) &e && b_brownian) {
        b->velocity = Vector2Normalize(
            Vector2Add(b->velocity,
                       Vector2Scale((Vector2){(float) GetRandomValue(-16, 16) / 16.f,
                                              (float) GetRandomValue(-16, 16) / 16.f},
                                    brown_factor)));
        b->velocity = Vector2Scale(b->velocity, speed);
    }
    //drag

    b->velocity = Vector2Scale(b->velocity, Remap(b->size, 0, MAX_ESIZE, .99999, .999));
}

void UpdateParticle(Box *p) {
    const double increment = (double) (MAX_PARTICLES);

    Vector3 hsv = ColorToHSV(p->color);
    if (hsv.x >= 360.f) {
        hsv.x = 10;
    }
    hsv.x += (720. * (MAX_PARTICLES / 1000.)) / (increment);
    // hsv.y -= .1 / increment;
    //hsv.z -= .1 / increment;
    p->size -= PARTICLE_SIZE / increment;
    p->color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
}

static double ratio = PARTICLE_SIZE / (EMITTER_SIZE * (1000 / (PARTICLE_INTERVAL * 70)));

void emitParticle(Emitter *e) {
    Box *p = &e->particles[e->next];
    p->size = PARTICLE_SIZE;
    p->color = RED;
    static Vector2 pos_offset = {PARTICLE_SIZE / 2., PARTICLE_SIZE / 2.};
    pos_offset.x += p->size;
    if (pos_offset.x > e->size - PARTICLE_SIZE) {
        pos_offset.x = e->size > PARTICLE_SIZE * 2 ? PARTICLE_SIZE / 2. : 0;
        pos_offset.y += p->size;
    }
    if (pos_offset.y > e->size - PARTICLE_SIZE) {
        pos_offset.y = e->size > PARTICLE_SIZE * 2 ? PARTICLE_SIZE / 2. : 0;
    }
    if (b_nwtn3rd && e->size > PARTICLE_SIZE * 2) {
        if (b_rocket[UP] || b_rocket[DOWN]) {
            pos_offset.y = b_rocket[UP] ? e->size - PARTICLE_SIZE : 0;
        }
        if (b_rocket[LEFT]) {
            pos_offset.x = e->size - PARTICLE_SIZE;
        }
        if (b_rocket[RIGHT]) {
            pos_offset.x = 0;
        }
    }
    p->pos = Vector2Add((Vector2){e->pos.x, e->pos.y}, pos_offset);
    if (b_rocket[RIGHT]) {
        pos_offset.y += p->size;
    }
    Vector2 fuzz = (Vector2){(float) GetRandomValue(-64, 64) / 8.,
                             (float) GetRandomValue(-128, 128) / 8.};

    fuzz = Vector2Add(Vector2Scale(e->velocity, ratio), fuzz);
    if (b_nwtn3rd) {
        if (b_rocket[UP]) {
            if (fuzz.y < 0)
                fuzz.y = -1. * fuzz.y;
        } else if (b_rocket[DOWN]) {
            if (fuzz.y > 0)
                fuzz.y = -1 * fuzz.y;
        }
        if (b_rocket[LEFT]) {
            if (fuzz.x < 0)
                fuzz.x = -1 * fuzz.x;
        } else if (b_rocket[RIGHT]) {
            if (fuzz.x > 0)
                fuzz.x = -1 * fuzz.x;
        }
    }
    p->velocity = fuzz;
    if (b_nwtn3rd) {
        e->velocity = Vector2Subtract(e->velocity,
                                      Vector2Scale(fuzz, GetFrameTime() * ratio * 1024));
    }

    if (e->next + 1 < MAX_PARTICLES && e->next >= e->count) {
        e->count++;
    }
    e->next++;
    if (e->next >= MAX_PARTICLES) {
        e->next = 0;
    }
}

static const size_t CELL_SIZE = 64;

#define distance_from_emitter(BOX) \
    (Vector2Distance(Vector2Add((BOX).pos, (Vector2){(BOX).size / 2., (BOX).size / 2.}), \
                     Vector2Add(e.pos, (Vector2){e.size / 2., e.size / 2.})))

#define distance_from_particle(BOX1, BOX2) \
    (Vector2Distance(Vector2Add((BOX1).pos, (Vector2){(BOX1).size / 2., (BOX1).size / 2.}), \
                     Vector2Add((BOX2).pos, (Vector2){(BOX2).size / 2., (BOX2).size / 2.})))

#define get_cell(BOX) \
    ((struct { int x, y; }){(int) (BOX).pos.x / CELL_SIZE, (int) (BOX).pos.y / CELL_SIZE})

#define in_same_cell(BOX1, BOX2) \
    (abs(get_cell((BOX1)).x - get_cell((BOX2)).x) < 2 \
     && abs(get_cell((BOX1)).y - get_cell((BOX2)).y) < 2)

void *DoRepulsionForBox(void *arg) {
    const double dt = GetFrameTime();
    size_t offset = (size_t) arg;
    Box *boxes = e.particles;
    for (int i = offset; i < e.count; i += NUMTHREADS) {
        const float radius = boxes[i].size * repulsion_radius;
        if (boxes[i].size > 0) {
            if (distance_from_emitter(boxes[i]) < e.size * repulsion_radius) {
                double inv = Clamp(1
                                       / Vector2DistanceSqr(Vector2Normalize(boxes[i].pos),
                                                            Vector2Normalize(
                                                                Vector2Add(e.pos,
                                                                           (Vector2){e.size / 2,
                                                                                     e.size / 2}))),
                                   0,
                                   repulsion_factor * 4);
                Vector2 diff = Vector2Subtract(boxes[i].pos, e.pos);
                Vector2 deltaV = Vector2Scale(diff, dt * inv);
                boxes[i].velocity = Vector2Add(boxes[i].velocity, deltaV);
            }
            for (int j = 0; j < e.count; j++) {
                if (boxes[j].size > 0 && j != i && in_same_cell(boxes[i], boxes[j])) {
                    if (distance_from_particle(boxes[i], boxes[j])
                        < max(radius, boxes[j].size * repulsion_radius)) {
                        double inv = Clamp(1
                                               / Vector2DistanceSqr(Vector2Normalize(boxes[j].pos),
                                                                    Vector2Normalize(boxes[i].pos)),
                                           0,
                                           repulsion_factor);
                        Vector2 diff = Vector2Subtract(boxes[i].pos, boxes[j].pos);
                        Vector2 deltaV = Vector2Scale(diff, dt * inv);
                        boxes[i].velocity = Vector2Add(boxes[i].velocity, deltaV);
                    }
                }
            }
        }
    }
    return 0;
}
static pthread_t tid[NUMTHREADS];
void DoBoxRepulsion(Box *boxes) {
    for (size_t i = 0; i < NUMTHREADS; i++) {
        pthread_create(&tid[i], NULL, DoRepulsionForBox, (void *) i);
    }
    for (size_t i = 0; i < NUMTHREADS; i++) {
        pthread_join(tid[i], NULL);
    }
}

void generateRandomBarriers(void){
    for (int i = 0; i < NUM_BARRIERS; ++i) {
        barriers[i] = (Rectangle){GetRandomValue(128, SCREEN_WIDTH - 128),
                                  GetRandomValue(128, SCREEN_HEIGHT - 128),
                                  GetRandomValue(48, 480),
                                  GetRandomValue(48, 480)};
        if (barriers[i].x + barriers[i].width > SCREEN_WIDTH - 128
            || barriers[i].y + barriers[i].height > SCREEN_HEIGHT - 128) {
            //no barriers go off screen
            i--;
            continue;
        }
        for (int j = 0; j < i; ++j) {
            Rectangle collision = GetCollisionRec(barriers[i], barriers[j]);
            if (collision.height > 0 || collision.width > 0) {
                //no 2 barriers can intersect
                i--;
                break;
            }
        }
    }
}

void DrawBox(Box *b) {
    DrawRectangleV(b->pos, (Vector2){b->size, b->size}, b->color);
    if (!b_solitaire) {
        //solitaire-mode trails need to show particle color, so don't draw outlines
        DrawRectangleLinesEx((Rectangle){b->pos.x, b->pos.y, b->size, b->size}, 2.f, BLACK);
    }
}

void HandleInput(Emitter *e) {
    switch (GetKeyPressed()) {
    case KEY_KP_0:
        e->velocity = Vector2Zero();
        for (int i = 0; i < e->count; i++) {
            e->particles[i].velocity = Vector2Zero();
        }
        break;
    case KEY_R:
        generateRandomBarriers();
        e->count = 0;
        e->next = 0;
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
    case KEY_P:
        b_repulsion = !b_repulsion;
        break;
    case KEY_M:
        b_menuopen = !b_menuopen;
    default:
        break;
    }
    if (frames % 25 == 0) {
        if (IsKeyDown(KEY_COMMA) && e->size > MIN_ESIZE) {
            e->size -= .5;
            ratio = PARTICLE_SIZE / (e->size * (MAX_PARTICLES / (PARTICLE_INTERVAL * 70)));
        }
        if (IsKeyDown(KEY_PERIOD) && e->size < MAX_ESIZE) {
            e->size += .5;
            ratio = PARTICLE_SIZE / (e->size * (MAX_PARTICLES / (PARTICLE_INTERVAL * 70)));
        }
        if (IsKeyDown(KEY_SEMICOLON) && repulsion_radius > .1) {
            repulsion_radius -= .01;
        }
        if (IsKeyDown(KEY_APOSTROPHE) && repulsion_radius < 4) {
            repulsion_radius += .01;
        }
        if (IsKeyDown(KEY_LEFT_BRACKET) && repulsion_factor > 0) {
            repulsion_factor -= .01;
        }
        if (IsKeyDown(KEY_RIGHT_BRACKET) && repulsion_factor < 4) {
            repulsion_factor += .01;
        }
        if (IsKeyDown(KEY_MINUS) && brown_factor > 0.01) {
            brown_factor -= .01;
        }
        if (IsKeyDown(KEY_EQUAL) && brown_factor < 1) {
            brown_factor += .01;
        }
    }
    if (IsKeyDown(KEY_ZERO)) {
        e->velocity = Vector2Zero();
    }
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
        if (b_nwtn3rd) {
            b_rocket[UP] = true;
        } else {
            e->velocity = Vector2Clamp(Vector2Add(e->velocity, (Vector2){0., -0.5}),
                                       (Vector2){-100, -100},
                                       (Vector2){100, 100});
        }
    } else {
        b_rocket[UP] = false;
    }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
        if (b_nwtn3rd) {
            b_rocket[DOWN] = true;
        } else {
            e->velocity = Vector2Clamp(Vector2Add(e->velocity, (Vector2){0., 0.5}),
                                       (Vector2){-100, -100},
                                       (Vector2){100, 100});
        }
    } else {
        b_rocket[DOWN] = false;
    }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        if (b_nwtn3rd) {
            b_rocket[LEFT] = true;
        } else {
            e->velocity = Vector2Clamp(Vector2Add(e->velocity, (Vector2){-0.5, 0.}),
                                       (Vector2){-100, -100},
                                       (Vector2){100, 100});
        }
    } else {
        b_rocket[LEFT] = false;
    }

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        if (b_nwtn3rd) {
            b_rocket[RIGHT] = true;
        } else {
            e->velocity = Vector2Clamp(Vector2Add(e->velocity, (Vector2){0.5, 0.}),
                                       (Vector2){-100, -100},
                                       (Vector2){100, 100});
        }
    } else {
        b_rocket[RIGHT] = false;
    }
    {
        static Vector2 hVel = (Vector2){0.0f, 0.0f};
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            e->pos = GetMousePosition();
            e->velocity = Vector2Zero();
            hVel = Vector2Add(hVel,
                              Vector2Scale(Vector2Divide(GetMouseDelta(),
                                                         (Vector2){SCREEN_WIDTH, SCREEN_HEIGHT}),
                                           1. / GetFrameTime()));
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            e->velocity = Vector2Scale(hVel, 1);
            hVel = Vector2Zero();
        }
    }
}

void DoTextStuff(Emitter *e) {
    static Vector2 histPos[AVG_KEEP] = {0}, histVel[AVG_KEEP] = {0};
    histPos[frames % AVG_KEEP] = e->pos;
    histVel[frames % AVG_KEEP] = e->velocity;
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

    static const char *BOOLSTRINGS[2] = {"false", "true"};
    if (b_menuopen) {
        sprintf(posbuf, "Position: (%-5.2F, % -5.2F)", avgPos.x, avgPos.y);
        sprintf(velbuf, "Velocity: (%-7.2F, % -7.2F)", avgVel.x, avgVel.y);
        sprintf(flagsbuf,
                "Show / Hide Menu [M]\n\nGravity [G]: %s\n\nBrownian [B]: %s\n\t(< - + >factor = "
                "%.2f)\n\nNewton's 3rd "
                "[N]: %s\n\n"
                "Repulsion [P]: %s\n\t(< ; ' >radius = %.2f,\n\t < [ ] >factor = %.2f)\n\nXP "
                "Solitaire[L]: %s\n\n"
                "Emitter Size: %.0fpx\n\t(< , . >)\n\n"
                "Regenerate colliders [R]\n\n"
                "WASD/\n\tArrows/\n\t\tClick+Drag\n to move Emitter\n\n"
                "Set Emitter\n\tvelocity to 0 [0]\n\n"
                "Set ALL\n\tvelocities to 0 [kp_0]",
                BOOLSTRINGS[!!b_gravity],
                BOOLSTRINGS[!!b_brownian],
                brown_factor,
                BOOLSTRINGS[!!b_nwtn3rd],
                BOOLSTRINGS[!!b_repulsion],
                repulsion_radius,
                repulsion_factor,
                BOOLSTRINGS[!!b_solitaire],
                e->size);
    } else {
        sprintf(flagsbuf, "Show / Hide Menu [M]\n\n");
    }
}

void Draw(Emitter *e) {
    const Vector2 text_size = MeasureTextEx(GetFontDefault(), fpsbuf, TEXT_SIZE, 1);
    BeginDrawing();
    if (!b_solitaire) {
        ClearBackground(CLITERAL(Color){0x22, 0x22, 0x22, 0xFF});
    }
    for (int i = 0; i < NUM_BARRIERS; ++i) {
        DrawRectangleRec(barriers[i], barrierColor);
        DrawRectangleLinesEx(barriers[i], 4.f, BLACK);
    }

    if (e->count > e->next) {
        for (size_t i = e->next + 1; i < e->count; i++) {
            if (e->particles[i].size > 0)
                DrawBox(&e->particles[i]);
        }
        for (size_t i = 0; i < e->next + 1; i++) {
            if (e->particles[i].size > 0)
                DrawBox(&e->particles[i]);
        }
    } else {
        for (size_t i = 0; i < e->count; i++) {
            if (e->particles[i].size > 0)
                DrawBox(&e->particles[i]);
        }
    }
    DrawBox((Box *) e);
    if (b_menuopen) {
        DrawRectangleRec((Rectangle){TEXT_OFFSET / 2,
                                     TEXT_OFFSET / 2,
                                     MeasureTextEx(GetFontDefault(), flagsbuf, TEXT_SIZE, 1).x
                                         + TEXT_OFFSET,
                                     SCREEN_HEIGHT - TEXT_OFFSET},
                         GetColor(0x0A0A0A55));
        DrawText(fpsbuf,
                 TEXT_OFFSET,
                 SCREEN_HEIGHT - (3 * (text_size.y + TEXT_OFFSET)),
                 TEXT_SIZE / 1.25,
                 RAYWHITE);
        DrawText(posbuf,
                 TEXT_OFFSET,
                 SCREEN_HEIGHT - (2 * (text_size.y + TEXT_OFFSET)),
                 TEXT_SIZE / 1.25,
                 RAYWHITE);
        DrawText(velbuf,
                 TEXT_OFFSET,
                 SCREEN_HEIGHT - (text_size.y + TEXT_OFFSET),
                 TEXT_SIZE / 1.25,
                 RAYWHITE);
        DrawText(flagsbuf, TEXT_OFFSET, (TEXT_OFFSET), TEXT_SIZE / 1.25, RAYWHITE);
    } else {
        Vector2 flgbuf_size = MeasureTextEx(GetFontDefault(), flagsbuf, TEXT_SIZE, 1);
        DrawRectangleRec((Rectangle){TEXT_OFFSET / 2,
                                     TEXT_OFFSET / 2,
                                     flgbuf_size.x + TEXT_OFFSET,
                                     flgbuf_size.y + TEXT_OFFSET},
                         GetColor(0x0A0A0A55));
        DrawText(fpsbuf, TEXT_OFFSET, TEXT_OFFSET * 4, TEXT_SIZE / 1.25, RAYWHITE);
        DrawText(flagsbuf, TEXT_OFFSET, TEXT_OFFSET, TEXT_SIZE / 1.25, RAYWHITE);
    }

    EndDrawing();
}

int main(void) {
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(500);

    b_gravity = true;
    b_brownian = true;
    b_nwtn3rd = true;

    generateRandomBarriers();

    double t = GetTime();
    SetRandomSeed(*(unsigned long *) &t);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime() * TIMESCALE;
        //handle keyb/mouse input
        HandleInput(&e);
        // create new particle if it's time
        if (++frames % PARTICLE_INTERVAL == 0
            && (!b_nwtn3rd || b_rocket[0] || b_rocket[1] || b_rocket[2] || b_rocket[3])) {
            emitParticle(&e);
        }
        // update emitter position
        UpdateBoxPosition((Box *) &e, deltaTime);
        // update particle positions
        if (b_repulsion && e.count > 1)
            DoBoxRepulsion(e.particles);
        for (size_t i = 0; i < e.count; i++) {
            UpdateBoxPosition(&e.particles[i], deltaTime);
            if (frames % PARTICLE_INTERVAL == 0
                && (!b_nwtn3rd || b_rocket[0] || b_rocket[1] || b_rocket[2] || b_rocket[3])) {
                UpdateParticle(&e.particles[i]);
            }
        }

        DoTextStuff(&e);

        Draw(&e);
    }

    CloseWindow();

    return 0;
}
