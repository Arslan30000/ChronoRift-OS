#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ==================== CONSTANTS ====================
#define SHM_NAME "/chrono_shm"
#define MAX_PLAYERS 4
#define MAX_ENEMIES 9
#define INV_SLOTS 20
#define LT_MAX 50
#define NUM_WEAPONS 8
#define LOG_SIZE 30

// Roll number: 23i-0572
#define ROLL_SEED 572
#define ROLL_LAST 2
#define ROLL_SECOND_LAST 7
#define ROLL_LAST_TWO 72

// ==================== ENUMS ====================
enum ActionType {
    ACT_NONE = 0,
    ACT_STRIKE,
    ACT_EXHAUST,
    ACT_USE_WEAPON,
    ACT_SWAP_IN,
    ACT_HEAL,
    ACT_SKIP,
    ACT_ULTIMATE
};

enum GamePhase {
    PHASE_WAITING = 0,
    PHASE_BATTLE,
    PHASE_GAME_OVER
};

// ==================== STRUCTS ====================
struct Weapon {
    int id;
    char name[32];
    int slot_size;
    int damage;
    char icon_file[32];
};

struct Inventory {
    int slots[INV_SLOTS];   // weapon ID per slot, 0 = free
    int long_term[LT_MAX];  // weapon IDs in long-term storage
    int lt_count;
};

struct Entity {
    int id;
    char name[32];
    pid_t process_id;
    int hp;
    int max_hp;
    int stamina;
    int max_stamina;
    int speed;
    int damage;
    bool is_alive;
    bool is_stunned;
    time_t stun_start;       // timestamp when stun began
    Inventory inv;
};

struct ArtifactEntry {
    bool exists;        // Whether artifact is in the game world
    bool locked;        // Whether someone holds it
    int owner_id;       // entity id, -1 if free
    int owner_type;     // 0=player, 1=enemy, -1 if free
    int waiting_id;     // who wants this, -1 if nobody
    int waiting_type;   // 0=player, 1=enemy
};

struct ActionMessage {
    int sender_type;    // 0=player, 1=enemy
    int sender_id;
    ActionType type;
    int target_id;
    int weapon_id;      // for USE_WEAPON / SWAP_IN
    bool is_ready;
};

struct LogEntry {
    char message[128];
};

struct WeaponDrop {
    bool pending;
    int weapon_id;
    int killer_id;       // player who killed the enemy
    bool player_chose;   // player responded
    bool player_took;    // player accepted the weapon
};

// GUI Input — renderer writes here, HIP reads
// Phase flow: 0=idle, 1=waiting for action, 2=waiting for target,
//             3=waiting for weapon, 4=waiting for storage idx, 5=weapon drop choice
struct GuiInput {
    int phase;              // current input phase
    int selected_action;    // which action button was clicked (1-7)
    int selected_target;    // which enemy was clicked
    int selected_weapon;    // weapon id chosen
    int selected_storage;   // long-term storage index
    int drop_choice;        // 1=take, 0=decline
    bool input_ready;       // set by renderer when user clicks
    bool waiting;           // set by HIP when it needs input
    int for_player_id;      // which player's turn
    // Party selection via GUI
    int party_size;         // 0 = not selected yet
    bool party_selected;
};

// Animation state for visual effects in renderer
struct AnimState {
    int type;           // 0=none, 1=strike, 2=exhaust, 3=weapon, 4=heal, 5=stun, 6=ultimate, 7=death
    float timer;        // countdown in seconds
    char actor_name[32];
    char target_name[32];
    int damage;
    int actor_type;     // 0=player, 1=enemy
    int target_type;
    int target_id;      // NEW: Knows exactly which card to highlight
    int actor_id;       // NEW: Knows exactly who is acting
};
struct GameState {
    // Synchronization
    sem_t mutex;
    sem_t action_sem;

    // Phase
    GamePhase phase;

    // Players
    Entity players[MAX_PLAYERS];
    int num_players;
    bool hip_connected;

    // Enemies
    Entity enemies[MAX_ENEMIES];
    int num_enemies;
    bool asp_connected;

    // Pending action
    ActionMessage pending_action;

    // Turn management
    int active_turn_type;   // 0=player, 1=enemy, -1=none
    int active_turn_id;     // entity id

    // Artifacts: 0=Solar Core, 1=Lunar Blade, 2=Eclipse Relic
    ArtifactEntry artifacts[3];
    sem_t artifact_mutex;

    // Weapon drop
    WeaponDrop weapon_drop;
    WeaponDrop relic_drop; // For Eclipse Relic prompt

    // GUI input channel
    GuiInput gui;

    // Action log for GUI
    LogEntry log_entries[LOG_SIZE];
    int log_count;

    // Ultimate ability
    bool ultimate_active;
    pid_t asp_pid;

    // Game state
    bool game_over;
    int winner;          // 0=players won, 1=enemies won, 2=quit
    int total_kills;     // win when >= 10
    int total_spawned;   // total enemies spawned so far (max 10)
    float wave_transition_timer; // >0 means wave is spawning

    // Animation
    AnimState anim;

    // Timing
    int tick_count;
    time_t npc_turn_start;

    // Quit
    pid_t arbiter_pid;   // for SIGTERM from HIP
};

// ==================== WEAPON TABLE ====================
static const Weapon WEAPON_TABLE[NUM_WEAPONS] = {
    {1, "Solar Core",    10, 95, "Staff10.png"},
    {2, "Lunar Blade",   10, 90, "Sword14.png"},
    {3, "Iron Halberd",   7, 55, "Axe4.png"},
    {4, "Venom Dagger",   4, 30, "Dagger4.png"},
    {5, "Thunderstaff",   6, 50, "Staff5.png"},
    {6, "Obsidian Axe",   5, 45, "Axe1.png"},
    {7, "Frostbow",       6, 48, "Bow1.png"},
    {8, "Splinter Stick", 2, 12, "Staff1.png"}
};

// ==================== HELPERS ====================
static inline const Weapon* get_weapon_by_id(int id) {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (WEAPON_TABLE[i].id == id) return &WEAPON_TABLE[i];
    }
    return NULL;
}

static inline void add_log(GameState* gs, const char* msg) {
    int idx = gs->log_count % LOG_SIZE;
    strncpy(gs->log_entries[idx].message, msg, 127);
    gs->log_entries[idx].message[127] = '\0';
    gs->log_count++;
}

// Player / Enemy display names
static const char* PLAYER_NAMES[] = {"Aether", "Blaze", "Cryo", "Dawn"};
static const char* ENEMY_NAMES[]  = {"Shade", "Wraith", "Ghoul", "Specter",
                                      "Phantom", "Revenant", "Banshee", "Lich", "Dread"};

#endif