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

enum ActionType { NONE, STRIKE, EXHAUST, USE_WEAPON, SWAP_IN, HEAL, SKIP };

struct Weapon {
    int id;
    char name[32];
    int slot_size;
    int damage;
};

struct Inventory {
    int slots[20]; // 0 = free, >0 = weapon ID
    int long_term[50];
    int lt_count;
};

struct ArtifactLock {
    bool locked;
    int owner_id; // -1 if none
    int waiting_id; 
};

struct ActionMessage {
    int sender_type; // 0 = player, 1 = enemy
    int sender_id;
    ActionType type;
    int target_id;
    int weapon_id;
    bool is_ready;
};

struct Entity {
    int id;
    pid_t process_id;
    int hp;
    int max_hp;
    int stamina;
    int max_stamina;
    int speed;
    int damage;
    bool is_alive;
    bool is_stunned;
    Inventory inv;
};

struct GameState {
    sem_t mutex;
    sem_t action_sem; // Unnamed semaphore for action signalling
    
    Entity players[4];
    int num_players;
    
    Entity enemies[9];
    int num_enemies;
    
    ActionMessage pending_action;
    
    ArtifactLock solar_core;
    ArtifactLock lunar_blade;
    
    bool game_over;
    bool ultimate_active;
    pid_t asp_pid; // To suspend the entire enemy process
};

// Fixed weapon library
const Weapon WEAPONS[8] = {
    {1, "Solar Core", 10, 95}, {2, "Lunar Blade", 10, 90},
    {3, "Iron Halberd", 7, 55}, {4, "Venom Dagger", 4, 30},
    {5, "Thunderstaff", 6, 50}, {6, "Obsidian Axe", 5, 45},
    {7, "Frostbow", 6, 48}, {8, "Splinter Stick", 2, 12}
};

#endif