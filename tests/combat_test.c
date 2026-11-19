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
static void TestHurtReaction(void) {
    for (int attacker = 0; attacker < 2; ++attacker) {
        Game g = Fight();
        FighterInput attack = {.heavy = true};
        for (int i = 0; i < 80 && g.fighters[1 - attacker].health == MAX_HEALTH; ++i) {
            GameUpdate(&g, attacker == 0 ? attack : (FighterInput){0},
                       attacker == 1 ? attack : (FighterInput){0}, FIXED_DT);
            attack.heavy = false;
        }
        Fighter *target = &g.fighters[1 - attacker];
        assert(target->health < MAX_HEALTH);
        assert(target->stun > 0 && target->hurtDuration == target->stun);
        assert(target->hurtDirection == (attacker == 0 ? 1 : -1));
        float stun = target->stun;
        Tick(&g, 1, (FighterInput){0}, (FighterInput){0});
        assert(target->stun == stun); // Impact pose freezes during hitstop.
        Tick(&g, 100, (FighterInput){0}, (FighterInput){0});
        assert(target->stun == 0); // Recovery finishes; pose must not stick.
    }
    Game guarded = Fight();
    Tick(&guarded, 30, (FighterInput){.light = true}, (FighterInput){.block = true});
    assert(guarded.fighters[1].hurtDuration == 0);
}

static int CountEvents(const Game *g, EventType type) {
    int n = 0;
    for (int i = 0; i < g->eventCount; ++i)
        n += g->events[i].type == type;
    return n;
}
static void Press(Game *g, FighterInput a, FighterInput b, int ticks, EventType watch, int *seen) {
    for (int i = 0; i < ticks; ++i) {
        GameUpdate(g, a, b, FIXED_DT);
        if (seen)
            *seen += CountEvents(g, watch);
        a.jump = a.light = a.heavy = a.special = a.dash = a.super = false;
        b.jump = b.light = b.heavy = b.special = b.dash = b.super = false;
    }
}
static void TestChainLaunches(void) {
    Game g = Fight();
    int launches = 0;
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 14, EV_LAUNCH, &launches);
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 14, EV_LAUNCH, &launches);
    Press(&g, (FighterInput){.heavy = true}, (FighterInput){0}, 60, EV_LAUNCH, &launches);
    assert(launches == 1);
    assert(g.fighters[0].maxCombo >= 3);
    Press(&g, (FighterInput){0}, (FighterInput){0}, 120, EV_COUNT, NULL);
    assert(g.fighters[1].knockdown > 0 || g.fighters[1].invincible > 0 || FighterCanAct(&g.fighters[1]));
    assert(g.fighters[0].combo == 0 && g.fighters[0].lastComboDamage > 64 + 50);
}
static void TestDamageScaling(void) {
    Game g = Fight();
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 14, EV_COUNT, NULL);
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 14, EV_COUNT, NULL);
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 30, EV_COUNT, NULL);
    float dealt = MAX_HEALTH - g.fighters[1].health;
    assert(g.fighters[0].maxCombo == 3);
    assert(dealt < 64 * 3 && dealt > 64 * 2);
}
static void TestCounterHit(void) {
    Game g = Fight();
    g.fighters[1].action = ACTION_HEAVY; // caught in start-up
    g.fighters[1].actionTime = 0.02f;
    int counters = 0;
    Press(&g, (FighterInput){.light = true}, (FighterInput){0}, 30, EV_COUNTER, &counters);
    assert(counters == 1);
    assert(fabsf(g.fighters[1].health - (MAX_HEALTH - 64 * 1.2f)) < 0.01f);
}
static void TestGuardBreak(void) {
    Game g = Fight();
    g.fighters[1].energy = 3;
    int breaks = 0;
    Press(&g, (FighterInput){.heavy = true}, (FighterInput){.block = true}, 50, EV_GUARD_BREAK, &breaks);
    assert(breaks == 1);
    assert(g.fighters[1].stun > 0.3f);
}
static void TestKnockdownAndWakeUp(void) {
    Game g = Fight();
    g.fighters[1].velocity.y = -500;
    g.fighters[1].grounded = false;
    g.fighters[1].position.y = GROUND_Y - 60;
    Press(&g, (FighterInput){.heavy = true}, (FighterInput){0}, 40, EV_COUNT, NULL);
    int downs = 0;
    Press(&g, (FighterInput){0}, (FighterInput){0}, 200, EV_KNOCKDOWN, &downs);
    assert(downs == 1);
    Press(&g, (FighterInput){0}, (FighterInput){0}, 120, EV_COUNT, NULL);
    assert(FighterCanAct(&g.fighters[1]));
}
static void TestBackdash(void) {
    Game g = Fight();
    Tick(&g, 1, (FighterInput){.dash = true, .move = -1}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_BACKDASH);
    assert(g.fighters[0].facing == 1 && g.fighters[0].velocity.x < 0);
}
static void TestAirAttacks(void) {
    Game g = Fight();
    g.fighters[1].position.x = 450;
    Tick(&g, 25, (FighterInput){.jump = true, .move = 1}, (FighterInput){0});
    Tick(&g, 1, (FighterInput){.heavy = true}, (FighterInput){0});
    assert(g.fighters[0].action == ACTION_DIVE);
    Tick(&g, 80, (FighterInput){0}, (FighterInput){0});
    assert(g.fighters[1].health < MAX_HEALTH);
}
static void TestRenSuper(void) {
    Game g = Fight();
    g.fighters[1].position.x = 900;
    g.fighters[0].energy = MAX_ENERGY;
    int flashes = 0;
    Press(&g, (FighterInput){.super = true}, (FighterInput){0}, 300, EV_SUPER_FLASH, &flashes);
    assert(flashes == 1);
    assert(g.fighters[1].health < MAX_HEALTH - 250);
    assert(g.fighters[0].supersLanded == 1);
}
static void TestKaeSuperAndUppercut(void) {
    Game g;
    GameStartEx(&g, MODE_LOCAL, 1, 7, CHAR_KAE, CHAR_REN, STAGE_STORM_TERRACE);
    g.phase = ROUND_FIGHT;
    g.fighters[0].position.x = 300;
    g.fighters[1].position.x = 600;
    g.fighters[0].energy = MAX_ENERGY;
    Press(&g, (FighterInput){.super = true}, (FighterInput){0}, 400, EV_COUNT, NULL);
    assert(g.fighters[1].health < MAX_HEALTH - 250);
    // Crimson Ascent beats a jump-in.
    g = (Game){0};
    GameStartEx(&g, MODE_LOCAL, 1, 7, CHAR_KAE, CHAR_REN, STAGE_SANCTUARY);
    g.phase = ROUND_FIGHT;
    g.fighters[0].position.x = 300;
    g.fighters[1].position.x = 360;
    g.fighters[1].grounded = false;
    g.fighters[1].position.y = GROUND_Y - 120;
    int launches = 0;
    Press(&g, (FighterInput){.special = true}, (FighterInput){0}, 40, EV_LAUNCH, &launches);
    assert(launches == 1);
}
static void TestHardAIDefends(void) {
    Game g;
    GameStart(&g, MODE_SOLO, 2, 99);
    g.phase = ROUND_FIGHT;
    int guards = 0, hits = 0;
    for (int i = 0; i < 120 * 20; ++i) {
        // Mash heavy kicks from close range and count how the CPU answers.
        FighterInput p = {.heavy = i % 50 == 0, .move = fabsf(g.fighters[0].position.x - g.fighters[1].position.x) > 110 ? (g.fighters[0].position.x < g.fighters[1].position.x ? 1.0f : -1.0f) : 0};
        GameUpdate(&g, p, (FighterInput){0}, FIXED_DT);
        guards += CountEvents(&g, EV_BLOCK);
        hits += CountEvents(&g, EV_HIT);
        if (g.phase != ROUND_FIGHT)
            break;
    }
    assert(guards > 0);
    assert(hits > 0);
}
int main(void) {
    TestHurtReaction();
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
    TestChainLaunches();
    TestDamageScaling();
    TestCounterHit();
    TestGuardBreak();
    TestKnockdownAndWakeUp();
    TestBackdash();
    TestAirAttacks();
    TestRenSuper();
    TestKaeSuperAndUppercut();
    TestHardAIDefends();
    puts("23 combat regression checks passed.");
    return 0;
}
