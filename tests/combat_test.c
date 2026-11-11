#ifdef NDEBUG
#undef NDEBUG
#endif
#include "combat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static Game Fight(void) {
    Game g;
    GameStart(&g, MODE_LOCAL, 1, 12345);
    g.phase = ROUND_FIGHT;
    g.fighters[0].position.x = 300;
    g.fighters[1].position.x = 380;
    return g;
}
static void Tick(Game *g, int count, FighterInput a, FighterInput b) {
    for (int i = 0; i < count; ++i) {
        GameUpdate(g, a, b, FIXED_DT);
        a.jump = a.light = a.heavy = a.special = a.dash = false;
        b.jump = b.light = b.heavy = b.special = b.dash = false;
    }
}
static void TestAttackWindows(void) {
    Game g = Fight();
    Tick(&g, 6, (FighterInput){.light = true}, (FighterInput){0});
    assert(g.fighters[1].health == MAX_HEALTH);
    Tick(&g, 25, (FighterInput){0}, (FighterInput){0});
    assert(g.fighters[1].health == MAX_HEALTH - 64);
    Tick(&g, 50, (FighterInput){0}, (FighterInput){0});
    assert(g.fighters[1].health == MAX_HEALTH - 64);
}
static void TestDirectionalGuard(void) {
    Game g = Fight();
    Tick(&g, 30, (FighterInput){.light = true}, (FighterInput){.block = true});
    assert(g.fighters[1].health < MAX_HEALTH);
    assert(g.fighters[1].health > MAX_HEALTH - 10);
    assert(g.fighters[1].energy < 40);
}
static void TestWhiff(void) {
    Game g = Fight();
    g.fighters[1].position.x = 1000;
    Tick(&g, 80, (FighterInput){.heavy = true}, (FighterInput){0});
    assert(g.fighters[1].health == MAX_HEALTH);
}
static void TestJumpAndBounds(void) {
    Game g = Fight();
    Tick(&g, 20, (FighterInput){.jump = true}, (FighterInput){0});
    assert(g.fighters[0].position.y < GROUND_Y - 50);
    Tick(&g, 160, (FighterInput){.move = -1}, (FighterInput){0});
    assert(g.fighters[0].position.y == GROUND_Y);
    assert(g.fighters[0].grounded);
    assert(g.fighters[0].position.x >= 85);
}
static void TestDashCost(void) {
    Game g = Fight();
    g.fighters[0].energy = 3;
    Tick(&g, 1, (FighterInput){.dash = true}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_IDLE);
    g.fighters[0].energy = 40;
    Tick(&g, 1, (FighterInput){.dash = true}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_DASH);
    assert(g.fighters[0].invincible > 0);
    assert(g.fighters[0].energy < 29);
}
static void TestProjectile(void) {
    Game g = Fight();
    g.fighters[1].position.x = 700;
    Tick(&g, 110, (FighterInput){.special = true}, (FighterInput){0});
    assert(g.fighters[1].health == MAX_HEALTH - 105);
    assert(g.fighters[0].energy < 30);
}
static void TestPause(void) {
    Game g = Fight();
    g.paused = true;
    float timer = g.roundTime;
    Tick(&g, 100, (FighterInput){.light = true, .move = 1}, (FighterInput){0});
    assert(g.roundTime == timer);
    assert(g.fighters[0].position.x == 300);
}
static void TestRoundAndMatch(void) {
    Game g = Fight();
    g.fighters[1].health = 10;
    Tick(&g, 30, (FighterInput){.light = true}, (FighterInput){0});
    assert(g.phase == ROUND_END && g.winner == 0 && g.fighters[0].wins == 1);
    Tick(&g, 400, (FighterInput){0}, (FighterInput){0});
    assert(g.round == 2 && g.fighters[0].health == MAX_HEALTH && g.fighters[0].wins == 1);
    g.phase = ROUND_FIGHT;
    g.fighters[0].position.x = 300;
    g.fighters[1].position.x = 380;
    g.fighters[1].health = 10;
    Tick(&g, 450, (FighterInput){.light = true}, (FighterInput){0});
    assert(g.phase == MATCH_END && g.fighters[0].wins == 2);
}
static void TestTimeoutAndDraw(void) {
    Game g = Fight();
    g.roundTime = .001f;
    g.fighters[0].health = 800;
    Tick(&g, 1, (FighterInput){0}, (FighterInput){0});
    assert(g.phase == ROUND_END && g.winner == 1);
    g = Fight();
    g.roundTime = .001f;
    Tick(&g, 1, (FighterInput){0}, (FighterInput){0});
    assert(g.winner == -1 && g.fighters[0].wins == 0 && g.fighters[1].wins == 0);
}
static void TestTraining(void) {
    Game g = Fight();
    g.mode = MODE_TRAINING;
    Tick(&g, 10000, (FighterInput){0}, (FighterInput){0});
    assert(g.phase == ROUND_FIGHT && g.roundTime == 75);
}
static void TestBufferedAttack(void) {
    Game g = Fight();
    Tick(&g, 36, (FighterInput){.light = true}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_LIGHT);
    Tick(&g, 1, (FighterInput){.heavy = true}, (FighterInput){0});
    Tick(&g, 20, (FighterInput){0}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_HEAVY);
}
static void TestDeterministicAI(void) {
    Game a, b;
    GameStart(&a, MODE_DEMO, 2, 555);
    GameStart(&b, MODE_DEMO, 2, 555);
    for (int i = 0; i < 20000; ++i) {
        GameUpdate(&a, (FighterInput){0}, (FighterInput){0}, FIXED_DT);
        GameUpdate(&b, (FighterInput){0}, (FighterInput){0}, FIXED_DT);
    }
    assert(a.fighters[0].health == b.fighters[0].health);
    assert(a.fighters[1].health == b.fighters[1].health);
    assert(a.phase == MATCH_END);
}
int main(void) {
    TestAttackWindows();
    TestDirectionalGuard();
    TestWhiff();
    TestJumpAndBounds();
    TestDashCost();
    TestProjectile();
    TestPause();
    TestRoundAndMatch();
    TestTimeoutAndDraw();
    TestTraining();
    TestBufferedAttack();
    TestDeterministicAI();
    puts("12 combat regression checks passed.");
    return 0;
}
