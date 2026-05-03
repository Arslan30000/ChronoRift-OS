#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "../shared.h"

// Inventory management
bool allocate_weapon(Entity* e, const Weapon* w);
bool swap_in_weapon(Entity* e, int lt_index);
int  count_inventory_weapons(Entity* e);
void list_inventory_weapons(Entity* e, int* out_ids, int* out_count);

// Action processing
void process_action(GameState* gs, ActionMessage* msg);

// Game state checks
bool check_game_over(GameState* gs);
int  count_alive_players(GameState* gs);
int  count_alive_enemies(GameState* gs);
void spawn_wave(GameState* gs);

// Deadlock detection
bool check_deadlock(GameState* gs);
void resolve_deadlock(GameState* gs);

// Eclipse relic
void spawn_eclipse_relic(GameState* gs);

#endif
