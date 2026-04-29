#include "renderer.h"
#include "game_logic.h"
#include <cmath>
#include <cstdio>

// ===================== COLOR PALETTE =====================
static const sf::Color BG_COLOR(10, 14, 26);
static const sf::Color PANEL_BG(20, 30, 50, 220);
static const sf::Color PLAYER_COLOR(40, 120, 220);
static const sf::Color PLAYER_DARK(20, 60, 110);
static const sf::Color ENEMY_COLOR(220, 50, 50);
static const sf::Color ENEMY_DARK(110, 25, 25);
static const sf::Color GOLD(251, 191, 36);
static const sf::Color CYAN_T(6, 182, 212);
static const sf::Color HP_GREEN(34, 197, 94);
static const sf::Color HP_RED(239, 68, 68);
static const sf::Color HP_YELLOW(234, 179, 8);
static const sf::Color STAM_BLUE(59, 130, 246);
static const sf::Color STAM_CYAN(6, 182, 212);
static const sf::Color TEXT_WHITE(230, 230, 240);
static const sf::Color TEXT_DIM(140, 140, 160);
static const sf::Color STUN_YELLOW(250, 204, 21);
static const sf::Color DEAD_GRAY(80, 80, 80);
static const sf::Color READY_GLOW(100, 255, 100, 60);
static const sf::Color ULT_GOLD(255, 215, 0, 100);

// ===================== HELPERS =====================
static void drawRoundedRect(sf::RenderWindow& w, float x, float y, float width, float height, sf::Color fill, sf::Color outline = sf::Color::Transparent, float outThick = 0) {
    sf::RectangleShape r(sf::Vector2f(width, height));
    r.setPosition(x, y);
    r.setFillColor(fill);
    if (outThick > 0) { r.setOutlineColor(outline); r.setOutlineThickness(outThick); }
    w.draw(r);
}

static sf::Color hpColor(float pct) {
    if (pct > 0.5f) return sf::Color((sf::Uint8)(HP_GREEN.r*(pct) + HP_YELLOW.r*(1-pct)), (sf::Uint8)(HP_GREEN.g*pct + HP_YELLOW.g*(1-pct)), HP_GREEN.b);
    return sf::Color((sf::Uint8)(HP_YELLOW.r*pct*2 + HP_RED.r*(1-pct*2)), (sf::Uint8)(HP_YELLOW.g*pct*2), HP_RED.b);
}

static void drawBar(sf::RenderWindow& w, float x, float y, float width, float height, float pct, sf::Color fillCol, sf::Color bgCol = sf::Color(30,30,40)) {
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    sf::RectangleShape bg(sf::Vector2f(width, height));
    bg.setPosition(x, y); bg.setFillColor(bgCol); w.draw(bg);
    if (pct > 0) {
        sf::RectangleShape bar(sf::Vector2f(width * pct, height));
        bar.setPosition(x, y); bar.setFillColor(fillCol); w.draw(bar);
    }
}

static void drawPlayerSprite(sf::RenderWindow& w, float cx, float cy, sf::Color col, bool dead, bool stunned) {
    sf::Color c = dead ? DEAD_GRAY : col;
    // Body
    sf::RectangleShape body(sf::Vector2f(20, 28));
    body.setPosition(cx - 10, cy - 8); body.setFillColor(c); w.draw(body);
    // Head
    sf::CircleShape head(10); head.setPosition(cx - 10, cy - 28);
    head.setFillColor(sf::Color(c.r+40, c.g+40, c.b+40)); w.draw(head);
    // Shield
    sf::RectangleShape shield(sf::Vector2f(8, 18));
    shield.setPosition(cx - 18, cy - 4); shield.setFillColor(sf::Color(180,180,200)); w.draw(shield);
    // Sword
    sf::RectangleShape sword(sf::Vector2f(4, 26));
    sword.setPosition(cx + 10, cy - 14); sword.setFillColor(sf::Color(200,200,210)); w.draw(sword);
    sf::RectangleShape hilt(sf::Vector2f(12, 4));
    hilt.setPosition(cx + 6, cy - 6); hilt.setFillColor(GOLD); w.draw(hilt);
    if (stunned && !dead) {
        sf::CircleShape star(4, 5);
        star.setPosition(cx - 4, cy - 38); star.setFillColor(STUN_YELLOW); w.draw(star);
        sf::CircleShape star2(3, 5);
        star2.setPosition(cx + 8, cy - 34); star2.setFillColor(STUN_YELLOW); w.draw(star2);
    }
}

static void drawEnemySprite(sf::RenderWindow& w, float cx, float cy, sf::Color col, bool dead, bool stunned) {
    sf::Color c = dead ? DEAD_GRAY : col;
    // Body
    sf::RectangleShape body(sf::Vector2f(24, 28));
    body.setPosition(cx - 12, cy - 8); body.setFillColor(c); w.draw(body);
    // Head
    sf::CircleShape head(11); head.setPosition(cx - 11, cy - 30);
    head.setFillColor(sf::Color(c.r-20 > 0 ? c.r-20 : 0, c.g, c.b)); w.draw(head);
    // Horns
    sf::ConvexShape horn(3); horn.setPoint(0, sf::Vector2f(0,12));
    horn.setPoint(1, sf::Vector2f(5,0)); horn.setPoint(2, sf::Vector2f(10,12));
    horn.setPosition(cx - 16, cy - 40); horn.setFillColor(sf::Color(c.r, c.g+30, c.b)); w.draw(horn);
    sf::ConvexShape horn2(3); horn2.setPoint(0, sf::Vector2f(0,12));
    horn2.setPoint(1, sf::Vector2f(5,0)); horn2.setPoint(2, sf::Vector2f(10,12));
    horn2.setPosition(cx + 6, cy - 40); horn2.setFillColor(sf::Color(c.r, c.g+30, c.b)); w.draw(horn2);
    // Eyes
    sf::CircleShape eye1(2); eye1.setPosition(cx-7, cy-24); eye1.setFillColor(sf::Color(255,255,0)); w.draw(eye1);
    sf::CircleShape eye2(2); eye2.setPosition(cx+3, cy-24); eye2.setFillColor(sf::Color(255,255,0)); w.draw(eye2);
    // Claws
    sf::RectangleShape claw(sf::Vector2f(3, 10));
    claw.setPosition(cx - 15, cy + 16); claw.setFillColor(sf::Color(c.r, c.g+50, c.b)); w.draw(claw);
    sf::RectangleShape claw2(sf::Vector2f(3, 10));
    claw2.setPosition(cx + 12, cy + 16); claw2.setFillColor(sf::Color(c.r, c.g+50, c.b)); w.draw(claw2);
    if (stunned && !dead) {
        sf::CircleShape star(4, 5);
        star.setPosition(cx - 4, cy - 48); star.setFillColor(STUN_YELLOW); w.draw(star);
    }
}

// ===================== ENTITY CARD =====================
static void drawEntityCard(sf::RenderWindow& win, sf::Font& font, Entity* e, float x, float y, float w, float h,
                           bool isPlayer, bool isActive, sf::Sprite* pngSprite = NULL) {
    sf::Color borderCol = isPlayer ? PLAYER_COLOR : ENEMY_COLOR;
    sf::Color bgCol = isPlayer ? PLAYER_DARK : ENEMY_DARK;

    if (!e->is_alive) { borderCol = DEAD_GRAY; bgCol = sf::Color(30,30,35); }

    // Active glow
    if (isActive && e->is_alive) {
        drawRoundedRect(win, x-3, y-3, w+6, h+6, READY_GLOW, GOLD, 2);
    }

    // Card background
    drawRoundedRect(win, x, y, w, h, bgCol, borderCol, 2);

    // Sprite — use PNG if available, else geometric fallback
    float spX = x + 40, spY = y + 48;
    if (pngSprite && e->is_alive) {
        pngSprite->setPosition(x + 5, y + 4);
        // Scale to fit 64x72 area
        sf::FloatRect bounds = pngSprite->getLocalBounds();
        if (bounds.width > 0 && bounds.height > 0) {
            pngSprite->setScale(64.f / bounds.width, 72.f / bounds.height);
        }
        if (!e->is_alive) pngSprite->setColor(sf::Color(100,100,100,150));
        else if (e->is_stunned) pngSprite->setColor(sf::Color(255,255,100,200));
        else pngSprite->setColor(sf::Color::White);
        win.draw(*pngSprite);
    } else {
        if (isPlayer) drawPlayerSprite(win, spX, spY, borderCol, !e->is_alive, e->is_stunned);
        else drawEnemySprite(win, spX, spY, borderCol, !e->is_alive, e->is_stunned);
    }

    // Name
    sf::Text name(e->name, font, 13);
    name.setFillColor(e->is_alive ? TEXT_WHITE : DEAD_GRAY);
    name.setStyle(sf::Text::Bold);
    name.setPosition(x + 75, y + 8);
    win.draw(name);

    // ID tag
    char tag[16];
    snprintf(tag, sizeof(tag), isPlayer ? "[P%d]" : "[E%d]", e->id);
    sf::Text idTag(tag, font, 11);
    idTag.setFillColor(borderCol);
    idTag.setPosition(x + 75, y + 26);
    win.draw(idTag);

    // Status
    if (!e->is_alive) {
        sf::Text dead("DEAD", font, 11); dead.setFillColor(HP_RED);
        dead.setPosition(x + w - 45, y + 8); win.draw(dead);
    } else if (e->is_stunned) {
        sf::Text st("STUNNED", font, 10); st.setFillColor(STUN_YELLOW);
        st.setPosition(x + w - 62, y + 8); win.draw(st);
    } else if (e->stamina >= e->max_stamina) {
        sf::Text rdy("READY!", font, 10); rdy.setFillColor(HP_GREEN);
        rdy.setPosition(x + w - 50, y + 8); win.draw(rdy);
    }

    if (!e->is_alive) return;

    // HP bar
    float hpPct = (e->max_hp > 0) ? (float)e->hp / e->max_hp : 0;
    sf::Text hpLabel("HP", font, 10); hpLabel.setFillColor(TEXT_DIM);
    hpLabel.setPosition(x + 75, y + 42); win.draw(hpLabel);
    drawBar(win, x + 95, y + 44, w - 108, 12, hpPct, hpColor(hpPct));
    char hpTxt[32]; snprintf(hpTxt, sizeof(hpTxt), "%d/%d", e->hp, e->max_hp);
    sf::Text hpV(hpTxt, font, 9); hpV.setFillColor(TEXT_WHITE);
    hpV.setPosition(x + 97, y + 43); win.draw(hpV);

    // Stamina bar
    float stPct = (e->max_stamina > 0) ? (float)e->stamina / e->max_stamina : 0;
    sf::Text stLabel("ST", font, 10); stLabel.setFillColor(TEXT_DIM);
    stLabel.setPosition(x + 75, y + 60); win.draw(stLabel);
    bool full = (e->stamina >= e->max_stamina);
    sf::Color stCol = full ? STAM_CYAN : STAM_BLUE;
    drawBar(win, x + 95, y + 62, w - 108, 12, stPct, stCol);
    char stTxt[32]; snprintf(stTxt, sizeof(stTxt), "%d/%d", e->stamina, e->max_stamina);
    sf::Text stV(stTxt, font, 9); stV.setFillColor(TEXT_WHITE);
    stV.setPosition(x + 97, y + 61); win.draw(stV);
}

// ===================== MAIN RENDER THREAD =====================
void* render_thread_func(void* arg) {
    GameState* gs = (GameState*)arg;

    sf::RenderWindow window(sf::VideoMode(1100, 900), "Chrono Rift", sf::Style::Close);
    window.setFramerateLimit(30);

    sf::Font font;
    font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

    // ===== SPRITESHEET + PNG LOADING =====
    // The renderer supports TWO methods for loading art:
    //
    // METHOD 1 (Spritesheet): Place a single spritesheet PNG and define crop
    //   rectangles below. This is ideal for tilesets and sprite atlases.
    //   Files:  assets/characters.png   — all player + enemy sprites
    //           assets/items.png        — all weapon/item icons
    //           assets/background.png   — full 1100x750 background
    //           assets/title.png        — 600x200 title logo
    //
    // METHOD 2 (Individual PNGs): Place individual files as before:
    //   assets/player_0.png .. player_3.png   (64x80 each)
    //   assets/enemy_0.png  .. enemy_8.png    (64x80 each)
    //   assets/weapon_1.png .. weapon_5.png   (32x32 each)
    //
    // The code tries spritesheets first, then individual files, then geometric shapes.
    // =====================================================================

    // --- Background & Title (individual files) ---
    sf::Texture bgTex;      bool hasBg = bgTex.loadFromFile("assets/background.png");
    sf::Sprite  bgSprite;    if (hasBg) bgSprite.setTexture(bgTex);

    sf::Texture titleTex;    bool hasTitle = titleTex.loadFromFile("assets/title.png");
    sf::Sprite  titleSprite; if (hasTitle) titleSprite.setTexture(titleTex);

    // --- Character Spritesheet ---
    // ┌──────────────────────────────────────────────────────────────────┐
    // │ EDIT THESE RECTANGLES to match YOUR spritesheet layout!         │
    // │ Format: sf::IntRect(x, y, width, height) in pixels              │
    // │ Open your spritesheet in any image editor and note pixel coords │
    // └──────────────────────────────────────────────────────────────────┘
    sf::Texture charSheet;
    bool hasCharSheet = charSheet.loadFromFile("assets/characters.png");

    // Player sprite crop regions (x, y, w, h) from characters.png
    // Your spritesheet: single row of characters, each ~16x16 pixels
    // First 4 characters = heroes, next 5+ = enemies
    // *** ADJUST these if your sprites are a different size! ***
    int SW = 16, SH = 16; // Sprite width/height — change to match your sheet
    sf::IntRect playerRect[MAX_PLAYERS] = {
        sf::IntRect(SW*0, 0, SW, SH),   // P0 - Aether  (1st character)
        sf::IntRect(SW*1, 0, SW, SH),   // P1 - Blaze   (2nd character)
        sf::IntRect(SW*2, 0, SW, SH),   // P2 - Cryo    (3rd character)
        sf::IntRect(SW*3, 0, SW, SH),   // P3 - Dawn    (4th character)
    };

    // Enemy sprite crop regions — cycle through the 2 working sprites (5th & 6th)
    sf::IntRect enemyRect[MAX_ENEMIES] = {
        sf::IntRect(SW*4, 0, SW, SH),   // E0 - Shade
        sf::IntRect(SW*5, 0, SW, SH),   // E1 - Wraith
        sf::IntRect(SW*4, 0, SW, SH),   // E2 - Ghoul    (reuse 5th)
        sf::IntRect(SW*5, 0, SW, SH),   // E3 - Specter  (reuse 6th)
        sf::IntRect(SW*4, 0, SW, SH),   // E4 - Phantom  (reuse 5th)
        sf::IntRect(SW*5, 0, SW, SH),   // E5 - Revenant (reuse 6th)
        sf::IntRect(SW*4, 0, SW, SH),   // E6 - Banshee  (reuse 5th)
        sf::IntRect(SW*5, 0, SW, SH),   // E7 - Lich     (reuse 6th)
        sf::IntRect(SW*4, 0, SW, SH),   // E8 - Dread    (reuse 5th)
    };

    // --- Item/Weapon Spritesheet ---
    sf::Texture itemSheet;
    bool hasItemSheet = itemSheet.loadFromFile("assets/items.png");

    // Weapon icon crop regions from items.png
    // Default: assumes 5 weapons in a row, each 16x16
    sf::IntRect weaponRect[NUM_WEAPONS] = {
        sf::IntRect(0,  0, 16, 16),    // W1 - Solar Core
        sf::IntRect(16, 0, 16, 16),    // W2 - Lunar Blade
        sf::IntRect(32, 0, 16, 16),    // W3 - Iron Halberd
        sf::IntRect(48, 0, 16, 16),    // W4 - Venom Dagger
        sf::IntRect(64, 0, 16, 16),    // W5 - Thunderstaff
    };

    // Build sprite objects from spritesheets
    sf::Sprite playerSpr[MAX_PLAYERS];
    bool hasPlayer[MAX_PLAYERS] = {};
    sf::Texture playerTex[MAX_PLAYERS]; // for individual file fallback

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (hasCharSheet) {
            playerSpr[i].setTexture(charSheet);
            playerSpr[i].setTextureRect(playerRect[i]);
            hasPlayer[i] = true;
        } else {
            // Fallback: try individual file
            char path[64]; snprintf(path, sizeof(path), "assets/player_%d.png", i);
            hasPlayer[i] = playerTex[i].loadFromFile(path);
            if (hasPlayer[i]) playerSpr[i].setTexture(playerTex[i]);
        }
    }

    sf::Sprite enemySpr[MAX_ENEMIES];
    bool hasEnemy[MAX_ENEMIES] = {};
    sf::Texture enemyTex[MAX_ENEMIES];

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (hasCharSheet) {
            enemySpr[i].setTexture(charSheet);
            enemySpr[i].setTextureRect(enemyRect[i]);
            hasEnemy[i] = true;
        } else {
            char path[64]; snprintf(path, sizeof(path), "assets/enemy_%d.png", i);
            hasEnemy[i] = enemyTex[i].loadFromFile(path);
            if (hasEnemy[i]) enemySpr[i].setTexture(enemyTex[i]);
        }
    }

    sf::Sprite weaponSpr[NUM_WEAPONS];
    bool hasWeapon[NUM_WEAPONS] = {};
    sf::Texture weaponTex[NUM_WEAPONS];

    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (hasItemSheet) {
            weaponSpr[i].setTexture(itemSheet);
            weaponSpr[i].setTextureRect(weaponRect[i]);
            hasWeapon[i] = true;
        } else {
            char path[64]; snprintf(path, sizeof(path), "assets/weapon_%d.png", i + 1);
            hasWeapon[i] = weaponTex[i].loadFromFile(path);
            if (hasWeapon[i]) weaponSpr[i].setTexture(weaponTex[i]);
        }
    }
    // ===== END TEXTURE LOADING =====

    // Action button layout constants
    const int NUM_BTNS = 7;
    const char* btnLabels[NUM_BTNS] = {"Strike", "Exhaust", "Use Weapon", "Swap In", "Heal", "Skip", "Ultimate"};
    const int btnAction[NUM_BTNS] = {1, 2, 3, 4, 5, 6, 7};
    sf::FloatRect btnRects[NUM_BTNS]; // computed each frame

    // Enemy card rects for click-to-target
    sf::FloatRect enemyCardRects[MAX_ENEMIES];

    // Weapon drop button rects
    sf::FloatRect dropYesRect, dropNoRect;

    float time_acc = 0;
    sf::Clock clk;

    while (window.isOpen()) {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) {
                window.close();
                sem_wait(&gs->mutex);
                gs->game_over = true;
                sem_post(&gs->mutex);
                return NULL;
            }
            // ===== MOUSE CLICK HANDLING =====
            if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
                float mx = (float)ev.mouseButton.x;
                float my = (float)ev.mouseButton.y;
                sem_wait(&gs->mutex);

                // Phase 1: Action selection — check action buttons
                if (gs->gui.waiting && gs->gui.phase == 1 && !gs->gui.input_ready) {
                    for (int b = 0; b < NUM_BTNS; b++) {
                        if (btnRects[b].contains(mx, my)) {
                            gs->gui.selected_action = btnAction[b];
                            // Actions needing a target go to phase 2
                            if (btnAction[b] <= 3) {
                                gs->gui.phase = 2; // need target
                            } else {
                                gs->gui.input_ready = true;
                            }
                            break;
                        }
                    }
                }
                // Phase 2: Target selection — check enemy cards
                else if (gs->gui.waiting && gs->gui.phase == 2 && !gs->gui.input_ready) {
                    for (int e = 0; e < gs->num_enemies; e++) {
                        if (gs->enemies[e].is_alive && enemyCardRects[e].contains(mx, my)) {
                            gs->gui.selected_target = e;
                            gs->gui.input_ready = true;
                            break;
                        }
                    }
                }
                // Phase 5: Weapon drop choice
                else if (gs->gui.waiting && gs->gui.phase == 5 && !gs->gui.input_ready) {
                    if (dropYesRect.contains(mx, my)) {
                        gs->gui.drop_choice = 1;
                        gs->gui.input_ready = true;
                    } else if (dropNoRect.contains(mx, my)) {
                        gs->gui.drop_choice = 0;
                        gs->gui.input_ready = true;
                    }
                }

                sem_post(&gs->mutex);
            }
        }

        float dt = clk.restart().asSeconds();
        time_acc += dt;

        window.clear(BG_COLOR);

        // Draw background PNG if available
        if (hasBg) window.draw(bgSprite);

        sem_wait(&gs->mutex);
        GamePhase phase = gs->phase;
        bool gameOver = gs->game_over;

        if (phase == PHASE_WAITING) {
            // ============ TITLE / WAITING SCREEN ============
            // Animated background particles
            for (int i = 0; i < 40; i++) {
                float px = fmod(i * 73.7f + time_acc * (10 + i % 5), 1100.0f);
                float py = fmod(i * 47.3f + time_acc * (8 + i % 3), 900.0f);
                sf::CircleShape dot(1.5f + (i % 3));
                dot.setPosition(px, py);
                dot.setFillColor(sf::Color(100, 140, 220, 30 + (i * 7) % 50));
                window.draw(dot);
            }

            // Title — use PNG logo if available, else text
            if (hasTitle) {
                sf::FloatRect tlb = titleSprite.getLocalBounds();
                titleSprite.setPosition(550 - tlb.width / 2, 160);
                window.draw(titleSprite);
            } else {
                sf::Text title("CHRONO RIFT", font, 64);
                title.setStyle(sf::Text::Bold);
                float glow = (sinf(time_acc * 2.0f) + 1.0f) * 0.5f;
                sf::Uint8 gb = (sf::Uint8)(180 + 75 * glow);
                title.setFillColor(sf::Color(gb, 220, 255));
                sf::FloatRect tb = title.getLocalBounds();
                title.setPosition(550 - tb.width / 2, 200);
                window.draw(title);
            }

            // Subtitle
            sf::Text sub("A Multi-Process Tactical Battle", font, 20);
            sub.setFillColor(sf::Color(160, 180, 220));
            sf::FloatRect sb2 = sub.getLocalBounds();
            sub.setPosition(550 - sb2.width / 2, 285);
            window.draw(sub);

            // Decorative line
            sf::RectangleShape line(sf::Vector2f(400, 2));
            line.setPosition(350, 320);
            line.setFillColor(sf::Color(100, 140, 220, 120));
            window.draw(line);

            // Seed info
            sf::Text seed("Operator: Muhammad Arslan | Seed: 23i-0572", font, 14);
            seed.setFillColor(GOLD);
            sf::FloatRect seedB = seed.getLocalBounds();
            seed.setPosition(550 - seedB.width / 2, 340);
            window.draw(seed);

            // Waiting text (pulsing)
            float pulse = (sinf(time_acc * 3.0f) + 1.0f) * 0.5f;
            sf::Uint8 alpha = (sf::Uint8)(120 + 135 * pulse);
            sf::Text wait("Waiting for players to connect...", font, 18);
            wait.setFillColor(sf::Color(200, 200, 220, alpha));
            sf::FloatRect wb = wait.getLocalBounds();
            wait.setPosition(550 - wb.width / 2, 420);
            window.draw(wait);

            // Connection status
            char cstat[128];
            snprintf(cstat, sizeof(cstat), "HIP: %s   |   ASP: %s",
                gs->hip_connected ? "CONNECTED" : "waiting...",
                gs->asp_connected ? "CONNECTED" : "waiting...");
            sf::Text cs(cstat, font, 14);
            cs.setFillColor(sf::Color(140, 160, 180));
            sf::FloatRect csb = cs.getLocalBounds();
            cs.setPosition(550 - csb.width / 2, 460);
            window.draw(cs);

        } else if (phase == PHASE_BATTLE || (gameOver && phase == PHASE_GAME_OVER)) {
            // ============ BATTLE SCREEN ============

            // Top bar
            drawRoundedRect(window, 0, 0, 1100, 42, sf::Color(15, 20, 35));
            sf::Text titleBar("CHRONO RIFT", font, 16);
            titleBar.setStyle(sf::Text::Bold);
            titleBar.setFillColor(CYAN_T);
            titleBar.setPosition(15, 10);
            window.draw(titleBar);

            char tickStr[64];
            snprintf(tickStr, sizeof(tickStr), "Tick: %d", gs->tick_count);
            sf::Text tickT(tickStr, font, 13);
            tickT.setFillColor(TEXT_DIM);
            tickT.setPosition(200, 13);
            window.draw(tickT);

            // Active turn indicator
            if (gs->active_turn_type >= 0) {
                Entity* ae = (gs->active_turn_type == 0) ? &gs->players[gs->active_turn_id] : &gs->enemies[gs->active_turn_id];
                char turnBuf[64];
                snprintf(turnBuf, sizeof(turnBuf), "Active: %s %s", gs->active_turn_type == 0 ? "[P]" : "[E]", ae->name);
                sf::Text turnT(turnBuf, font, 13);
                turnT.setFillColor(GOLD);
                turnT.setPosition(350, 13);
                window.draw(turnT);
            }

            if (gs->ultimate_active) {
                sf::Text ultT(">>> ULTIMATE ACTIVE <<<", font, 14);
                ultT.setFillColor(GOLD);
                ultT.setPosition(700, 12);
                ultT.setStyle(sf::Text::Bold);
                window.draw(ultT);
                // Overlay
                sf::RectangleShape ultOverlay(sf::Vector2f(1100, 900));
                ultOverlay.setFillColor(ULT_GOLD);
                window.draw(ultOverlay);
            }

            // Section labels
            sf::Text pLabel("PLAYERS", font, 14);
            pLabel.setFillColor(PLAYER_COLOR); pLabel.setStyle(sf::Text::Bold);
            pLabel.setPosition(20, 50);
            window.draw(pLabel);

            sf::Text eLabel("ENEMIES", font, 14);
            eLabel.setFillColor(ENEMY_COLOR); eLabel.setStyle(sf::Text::Bold);
            eLabel.setPosition(560, 50);
            window.draw(eLabel);

            // Player cards — pass PNG sprites
            float cardW = 510, cardH = 82;
            for (int i = 0; i < gs->num_players; i++) {
                bool active = (gs->active_turn_type == 0 && gs->active_turn_id == i);
                sf::Sprite* spr = (i < MAX_PLAYERS && hasPlayer[i]) ? &playerSpr[i] : NULL;
                drawEntityCard(window, font, &gs->players[i], 15, 72 + i * (cardH + 8), cardW, cardH, true, active, spr);
            }

            // Enemy cards — pass PNG sprites
            for (int i = 0; i < gs->num_enemies; i++) {
                bool active = (gs->active_turn_type == 1 && gs->active_turn_id == i);
                sf::Sprite* spr = (i < MAX_ENEMIES && hasEnemy[i]) ? &enemySpr[i] : NULL;
                float ey = 72 + i * (cardH + 8);
                if (i >= 4) {
                    drawEntityCard(window, font, &gs->enemies[i], 820, 72 + (i - 4) * (cardH + 8), 265, cardH, false, active, spr);
                } else {
                    drawEntityCard(window, font, &gs->enemies[i], 555, ey, cardW, cardH, false, active, spr);
                }
            }

            // ============ ACTION BUTTONS (when player turn + GUI waiting) ============
            if (gs->gui.waiting && gs->active_turn_type == 0 && !gs->gui.input_ready) {
                float btnX = 15, btnY0 = 72 + gs->num_players * 90 + 5;
                float btnW = 165, btnH = 32, gap = 6;

                if (gs->gui.phase == 1) {
                    // Draw action buttons
                    drawRoundedRect(window, btnX - 5, btnY0 - 5, 530, 80, sf::Color(15, 25, 45, 220), CYAN_T, 1);
                    sf::Text prompt("SELECT ACTION:", font, 12);
                    prompt.setFillColor(GOLD); prompt.setStyle(sf::Text::Bold);
                    prompt.setPosition(btnX, btnY0 - 2);
                    window.draw(prompt);
                    btnY0 += 16;

                    for (int b = 0; b < NUM_BTNS; b++) {
                        float bx = btnX + (b % 3) * (btnW + gap);
                        float by = btnY0 + (b / 3) * (btnH + gap);
                        // Skip ultimate if player doesn't have both artifacts
                        if (b == 6) {
                            bool hasSol = false, hasLun = false;
                            int pid = gs->gui.for_player_id;
                            for (int s = 0; s < INV_SLOTS; s++) {
                                if (gs->players[pid].inv.slots[s] == 1) hasSol = true;
                                if (gs->players[pid].inv.slots[s] == 2) hasLun = true;
                            }
                            if (!hasSol || !hasLun) continue;
                        }
                        sf::Color btnCol(30, 70, 130);
                        sf::Color btnOut(60, 130, 220);
                        if (b == 6) { btnCol = sf::Color(100, 70, 20); btnOut = GOLD; }
                        drawRoundedRect(window, bx, by, btnW, btnH, btnCol, btnOut, 2);
                        sf::Text bt(btnLabels[b], font, 13);
                        bt.setFillColor(TEXT_WHITE); bt.setStyle(sf::Text::Bold);
                        bt.setPosition(bx + 10, by + 7);
                        window.draw(bt);
                        btnRects[b] = sf::FloatRect(bx, by, btnW, btnH);
                    }
                } else if (gs->gui.phase == 2) {
                    // Target selection prompt
                    drawRoundedRect(window, btnX - 5, btnY0 - 5, 530, 30, sf::Color(80, 20, 20, 220), ENEMY_COLOR, 2);
                    sf::Text prompt("CLICK AN ENEMY TO TARGET:", font, 14);
                    prompt.setFillColor(sf::Color(255, 200, 200)); prompt.setStyle(sf::Text::Bold);
                    prompt.setPosition(btnX + 10, btnY0 + 2);
                    window.draw(prompt);
                }
            }

            // Store enemy card rects for click detection
            for (int i = 0; i < gs->num_enemies; i++) {
                float ey = 72 + i * (cardH + 8);
                if (i >= 4) {
                    enemyCardRects[i] = sf::FloatRect(820, 72 + (i-4)*(cardH+8), 265, cardH);
                } else {
                    enemyCardRects[i] = sf::FloatRect(555, ey, cardW, cardH);
                }
            }

            // ============ INVENTORY PANEL ============
            float invY = 490;
            drawRoundedRect(window, 15, invY, 520, 60, PANEL_BG, sf::Color(60, 80, 120), 1);
            sf::Text invLabel("INVENTORY (P0)", font, 11);
            invLabel.setFillColor(CYAN_T);
            invLabel.setPosition(20, invY + 3);
            window.draw(invLabel);

            if (gs->num_players > 0) {
                for (int s = 0; s < INV_SLOTS; s++) {
                    float sx = 20 + s * 25;
                    float sy = invY + 20;
                    int wid = gs->players[0].inv.slots[s];
                    sf::Color slotCol = (wid == 0) ? sf::Color(30, 35, 45) :
                        (wid == 1) ? sf::Color(220, 180, 40) :
                        (wid == 2) ? sf::Color(100, 120, 220) :
                        (wid == 3) ? sf::Color(140, 140, 140) :
                        (wid == 4) ? sf::Color(80, 180, 80) :
                        sf::Color(100, 60, 180);
                    drawRoundedRect(window, sx, sy, 22, 30, slotCol, sf::Color(60,70,80), 1);
                    if (wid > 0) {
                        // Use weapon PNG if available
                        int widx = wid - 1;
                        if (widx >= 0 && widx < NUM_WEAPONS && hasWeapon[widx]) {
                            weaponSpr[widx].setPosition(sx + 1, sy + 1);
                            sf::FloatRect wb2 = weaponSpr[widx].getLocalBounds();
                            if (wb2.width > 0 && wb2.height > 0)
                                weaponSpr[widx].setScale(20.f / wb2.width, 28.f / wb2.height);
                            window.draw(weaponSpr[widx]);
                        } else {
                            const Weapon* wp = get_weapon_by_id(wid);
                            if (wp) {
                                char wc[4]; wc[0] = wp->name[0]; wc[1] = '\0';
                                sf::Text wt(wc, font, 10);
                                wt.setFillColor(TEXT_WHITE);
                                wt.setPosition(sx + 6, sy + 8);
                                window.draw(wt);
                            }
                        }
                    }
                }
            }

            // ============ ARTIFACT PANEL ============
            float artY = 560;
            drawRoundedRect(window, 15, artY, 520, 40, PANEL_BG, sf::Color(120, 100, 40), 1);
            sf::Text artLabel("ARTIFACTS", font, 11);
            artLabel.setFillColor(GOLD); artLabel.setPosition(20, artY + 3); window.draw(artLabel);

            const char* artNames[] = {"Solar Core", "Lunar Blade", "Eclipse Relic"};
            for (int a = 0; a < 3; a++) {
                float ax = 20 + a * 175;
                if (!gs->artifacts[a].exists) continue;
                char abuf[64];
                if (gs->artifacts[a].locked) {
                    const char* oType = gs->artifacts[a].owner_type == 0 ? "P" : "E";
                    snprintf(abuf, sizeof(abuf), "%s [%s%d]", artNames[a], oType, gs->artifacts[a].owner_id);
                } else {
                    snprintf(abuf, sizeof(abuf), "%s [free]", artNames[a]);
                }
                sf::Text at(abuf, font, 10);
                at.setFillColor(gs->artifacts[a].locked ? sf::Color(255,200,100) : sf::Color(120,180,120));
                at.setPosition(ax, artY + 22);
                window.draw(at);
            }

            // ============ ACTION LOG ============
            float logY = 610;
            drawRoundedRect(window, 15, logY, 1070, 180, PANEL_BG, sf::Color(60, 70, 90), 1);
            sf::Text logLabel("ACTION LOG", font, 12);
            logLabel.setFillColor(CYAN_T); logLabel.setStyle(sf::Text::Bold);
            logLabel.setPosition(20, logY + 5);
            window.draw(logLabel);

            int logStart = gs->log_count > 11 ? gs->log_count - 11 : 0;
            for (int l = logStart; l < gs->log_count; l++) {
                int idx = l % LOG_SIZE;
                sf::Text lt(gs->log_entries[idx].message, font, 11);
                bool isImportant = (gs->log_entries[idx].message[0] == '*' || gs->log_entries[idx].message[0] == '>');
                lt.setFillColor(isImportant ? GOLD : TEXT_DIM);
                lt.setPosition(25, logY + 22 + (l - logStart) * 14);
                window.draw(lt);
            }

            // ============ GAME OVER OVERLAY ============
            if (gameOver) {
                sf::RectangleShape overlay(sf::Vector2f(1100, 900));
                overlay.setFillColor(sf::Color(0, 0, 0, 180));
                window.draw(overlay);

                const char* result = (gs->winner == 0) ? "VICTORY!" : "DEFEAT!";
                sf::Color resCol = (gs->winner == 0) ? GOLD : HP_RED;
                sf::Text resT(result, font, 72);
                resT.setStyle(sf::Text::Bold);
                resT.setFillColor(resCol);
                sf::FloatRect rb = resT.getLocalBounds();
                resT.setPosition(550 - rb.width / 2, 280);
                window.draw(resT);

                sf::Text subT("The battle has ended.", font, 20);
                subT.setFillColor(TEXT_DIM);
                sf::FloatRect sb3 = subT.getLocalBounds();
                subT.setPosition(550 - sb3.width / 2, 370);
                window.draw(subT);
            }

            // Weapon drop notification with clickable buttons
            if (gs->weapon_drop.pending && !gs->weapon_drop.player_chose) {
                drawRoundedRect(window, 300, 300, 500, 100, sf::Color(30, 25, 10, 240), GOLD, 3);
                const Weapon* dw = get_weapon_by_id(gs->weapon_drop.weapon_id);
                if (dw) {
                    char dbuf[128];
                    snprintf(dbuf, sizeof(dbuf), "WEAPON DROP: %s (Dmg:%d, Slots:%d)",
                        dw->name, dw->damage, dw->slot_size);
                    sf::Text dt(dbuf, font, 16);
                    dt.setFillColor(GOLD); dt.setStyle(sf::Text::Bold);
                    dt.setPosition(320, 315);
                    window.draw(dt);
                }
                // Yes button
                drawRoundedRect(window, 370, 355, 120, 35, sf::Color(20, 100, 40), HP_GREEN, 2);
                sf::Text yesT("Pick Up", font, 14); yesT.setFillColor(TEXT_WHITE); yesT.setStyle(sf::Text::Bold);
                yesT.setPosition(395, 362); window.draw(yesT);
                dropYesRect = sf::FloatRect(370, 355, 120, 35);
                // No button
                drawRoundedRect(window, 510, 355, 120, 35, sf::Color(100, 20, 20), HP_RED, 2);
                sf::Text noT("Decline", font, 14); noT.setFillColor(TEXT_WHITE); noT.setStyle(sf::Text::Bold);
                noT.setPosition(535, 362); window.draw(noT);
                dropNoRect = sf::FloatRect(510, 355, 120, 35);
            }
        }

        sem_post(&gs->mutex);
        window.display();
    }
    return NULL;
}
