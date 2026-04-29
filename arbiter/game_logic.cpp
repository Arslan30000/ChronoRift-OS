#include "game_logic.h"
#include <stdio.h>
#include <string.h>

// ================================================================
// INVENTORY: Contiguous-fit allocator (Section 6)
// ================================================================

// Find the best-fit contiguous free region for a weapon.
// Returns start index or -1 if not found.
static int find_contiguous_free(Entity* e, int size_needed) {
    int run = 0, start = -1;
    for (int i = 0; i < INV_SLOTS; i++) {
        if (e->inv.slots[i] == 0) {
            if (run == 0) start = i;
            run++;
            if (run >= size_needed) return start;
        } else {
            run = 0;
        }
    }
    return -1;
}

// Place weapon starting at `start` for `size` slots.
static void place_weapon(Entity* e, int weapon_id, int start, int size) {
    for (int i = start; i < start + size; i++) {
        e->inv.slots[i] = weapon_id;
    }
}

// Swap out the minimum number of weapons to create `size_needed` contiguous space.
// Returns the start index of the freed space, or -1 on failure.
static int make_room(Entity* e, int size_needed) {
    // Try every possible starting position and find the one
    // that requires swapping out the fewest distinct weapons.
    int best_start = -1;
    int best_swap_count = INV_SLOTS + 1;

    for (int start = 0; start <= INV_SLOTS - size_needed; start++) {
        // Count distinct weapons in this window
        int weapon_ids[INV_SLOTS];
        int distinct = 0;
        bool all_usable = true;

        for (int i = start; i < start + size_needed; i++) {
            if (e->inv.slots[i] != 0) {
                // Check if this weapon ID is already counted
                bool found = false;
                for (int j = 0; j < distinct; j++) {
                    if (weapon_ids[j] == e->inv.slots[i]) { found = true; break; }
                }
                if (!found) {
                    weapon_ids[distinct++] = e->inv.slots[i];
                }
            }
        }
        (void)all_usable;

        if (distinct < best_swap_count) {
            best_swap_count = distinct;
            best_start = start;
        }
    }

    if (best_start == -1) return -1;

    // Now swap out the weapons in the chosen window
    // We need to move ENTIRE weapons (all their slots), not just the ones in the window
    for (int i = best_start; i < best_start + size_needed; i++) {
        if (e->inv.slots[i] != 0) {
            int wid = e->inv.slots[i];
            // Move this weapon to long-term storage
            if (e->inv.lt_count < LT_MAX) {
                e->inv.long_term[e->inv.lt_count++] = wid;
            }
            // Clear ALL slots of this weapon from inventory
            for (int j = 0; j < INV_SLOTS; j++) {
                if (e->inv.slots[j] == wid) {
                    e->inv.slots[j] = 0;
                }
            }
        }
    }

    return best_start;
}

bool allocate_weapon(Entity* e, const Weapon* w) {
    if (!w) return false;

    // First try: find contiguous free space
    int pos = find_contiguous_free(e, w->slot_size);
    if (pos >= 0) {
        place_weapon(e, w->id, pos, w->slot_size);
        return true;
    }

    // Second try: swap out minimum weapons to make room
    pos = make_room(e, w->slot_size);
    if (pos >= 0) {
        place_weapon(e, w->id, pos, w->slot_size);
        return true;
    }

    return false;
}

bool swap_in_weapon(Entity* e, int lt_index) {
    if (lt_index < 0 || lt_index >= e->inv.lt_count) return false;

    int wid = e->inv.long_term[lt_index];
    const Weapon* w = get_weapon_by_id(wid);
    if (!w) return false;

    // Remove from long-term storage
    for (int i = lt_index; i < e->inv.lt_count - 1; i++) {
        e->inv.long_term[i] = e->inv.long_term[i + 1];
    }
    e->inv.lt_count--;

    // Allocate in primary inventory
    return allocate_weapon(e, w);
}

int count_inventory_weapons(Entity* e) {
    // Count distinct weapon instances
    int count = 0;
    bool counted[INV_SLOTS];
    memset(counted, 0, sizeof(counted));

    for (int i = 0; i < INV_SLOTS; i++) {
        if (e->inv.slots[i] != 0 && !counted[i]) {
            count++;
            int wid = e->inv.slots[i];
            for (int j = i; j < INV_SLOTS; j++) {
                if (e->inv.slots[j] == wid) counted[j] = true;
            }
        }
    }
    return count;
}

void list_inventory_weapons(Entity* e, int* out_ids, int* out_count) {
    *out_count = 0;
    bool seen[INV_SLOTS];
    memset(seen, 0, sizeof(seen));

    for (int i = 0; i < INV_SLOTS; i++) {
        if (e->inv.slots[i] != 0 && !seen[i]) {
            out_ids[*out_count] = e->inv.slots[i];
            (*out_count)++;
            int wid = e->inv.slots[i];
            for (int j = i; j < INV_SLOTS; j++) {
                if (e->inv.slots[j] == wid) seen[j] = true;
            }
        }
    }
}

// ================================================================
// ACTION PROCESSING (Section 10)
// ================================================================

void process_action(GameState* gs, ActionMessage* msg) {
    Entity* actor = (msg->sender_type == 0)
        ? &gs->players[msg->sender_id]
        : &gs->enemies[msg->sender_id];
    Entity* target = NULL;

    if (msg->type == ACT_STRIKE || msg->type == ACT_EXHAUST || msg->type == ACT_USE_WEAPON) {
        target = (msg->sender_type == 0)
            ? &gs->enemies[msg->target_id]
            : &gs->players[msg->target_id];
    }

    char buf[128];

    switch (msg->type) {
    case ACT_STRIKE:
        if (target) {
            target->hp -= actor->damage;
            if (target->hp < 0) target->hp = 0;
            actor->stamina = 0;
            snprintf(buf, sizeof(buf), "[%s] %s strikes %s for %d damage!",
                msg->sender_type == 0 ? "P" : "E", actor->name,
                target->name, actor->damage);
            add_log(gs, buf);

            // Stun check: damage > 50 triggers stun
            if (actor->damage > 50 && target->is_alive && !target->is_stunned) {
                target->is_stunned = true;
                target->stun_start = time(NULL);
                // Send SIGUSR1 to target process for async interrupt
                if (target->process_id > 0) {
                    kill(target->process_id, SIGUSR1);
                }
                snprintf(buf, sizeof(buf), "  >> %s is STUNNED for 3 seconds!", target->name);
                add_log(gs, buf);
            }
        }
        break;

    case ACT_EXHAUST:
        if (target) {
            target->stamina -= actor->damage;
            if (target->stamina < 0) target->stamina = 0;
            actor->stamina = 0;
            snprintf(buf, sizeof(buf), "[%s] %s drains %s stamina by %d!",
                msg->sender_type == 0 ? "P" : "E", actor->name,
                target->name, actor->damage);
            add_log(gs, buf);
        }
        break;

    case ACT_USE_WEAPON: {
        const Weapon* w = get_weapon_by_id(msg->weapon_id);
        if (target && w) {
            target->hp -= w->damage;
            if (target->hp < 0) target->hp = 0;
            actor->stamina = 0;
            snprintf(buf, sizeof(buf), "[%s] %s uses %s on %s for %d damage!",
                msg->sender_type == 0 ? "P" : "E", actor->name,
                w->name, target->name, w->damage);
            add_log(gs, buf);

            // Weapon-based stun
            if (w->damage > 50 && target->is_alive && !target->is_stunned) {
                target->is_stunned = true;
                target->stun_start = time(NULL);
                if (target->process_id > 0) {
                    kill(target->process_id, SIGUSR1);
                }
                snprintf(buf, sizeof(buf), "  >> %s is STUNNED for 3 seconds!", target->name);
                add_log(gs, buf);
            }
        }
        break;
    }

    case ACT_SWAP_IN:
        // weapon_id here is the long-term storage index
        if (swap_in_weapon(actor, msg->weapon_id)) {
            int wid = actor->inv.slots[0]; // just for log
            snprintf(buf, sizeof(buf), "[P] %s swaps in a weapon from storage!", actor->name);
            add_log(gs, buf);
        }
        actor->stamina = 0;
        break;

    case ACT_HEAL:
        actor->hp += actor->max_hp / 10;
        if (actor->hp > actor->max_hp) actor->hp = actor->max_hp;
        actor->stamina = 0;
        snprintf(buf, sizeof(buf), "[%s] %s heals for %d HP!",
            msg->sender_type == 0 ? "P" : "E", actor->name, actor->max_hp / 10);
        add_log(gs, buf);
        break;

    case ACT_SKIP:
        actor->stamina = actor->max_stamina / 2;
        snprintf(buf, sizeof(buf), "[%s] %s skips their turn.",
            msg->sender_type == 0 ? "P" : "E", actor->name);
        add_log(gs, buf);
        break;

    case ACT_ULTIMATE:
        // Requires both Solar Core and Lunar Blade
        // Deal massive damage to all enemies
        if (msg->sender_type == 0) {
            snprintf(buf, sizeof(buf), ">>> %s activates ULTIMATE ABILITY! <<<", actor->name);
            add_log(gs, buf);

            for (int i = 0; i < gs->num_enemies; i++) {
                if (gs->enemies[i].is_alive) {
                    gs->enemies[i].hp -= 185; // Solar(95) + Lunar(90)
                    if (gs->enemies[i].hp < 0) gs->enemies[i].hp = 0;
                }
            }
            actor->stamina = 0;

            // Suspend ASP for 10 seconds
            gs->ultimate_active = true;
            if (gs->asp_pid > 0) {
                kill(gs->asp_pid, SIGSTOP);
            }
        }
        break;

    default:
        break;
    }

    // Check if target died
    if (target && target->hp <= 0 && target->is_alive) {
        target->is_alive = false;
        target->hp = 0;
        snprintf(buf, sizeof(buf), "*** %s has been defeated! ***", target->name);
        add_log(gs, buf);

        // Weapon drop on enemy kill (50% chance, Section 6)
        if (msg->sender_type == 0) {
            if (rand() % 2 == 0) {
                int drop_id = (rand() % NUM_WEAPONS) + 1;
                gs->weapon_drop.pending = true;
                gs->weapon_drop.weapon_id = drop_id;
                gs->weapon_drop.killer_id = msg->sender_id;
                gs->weapon_drop.player_chose = false;
                gs->weapon_drop.player_took = false;
                const Weapon* dw = get_weapon_by_id(drop_id);
                if (dw) {
                    snprintf(buf, sizeof(buf), ">> %s dropped! Pick it up? (Player %d decides)",
                        dw->name, msg->sender_id);
                    add_log(gs, buf);
                }
            }

            // Spawn Eclipse Relic after first enemy kill
            if (!gs->artifacts[2].exists) {
                spawn_eclipse_relic(gs);
            }
        }
    }
}

// ================================================================
// GAME STATE CHECKS
// ================================================================

int count_alive_players(GameState* gs) {
    int c = 0;
    for (int i = 0; i < gs->num_players; i++)
        if (gs->players[i].is_alive) c++;
    return c;
}

int count_alive_enemies(GameState* gs) {
    int c = 0;
    for (int i = 0; i < gs->num_enemies; i++)
        if (gs->enemies[i].is_alive) c++;
    return c;
}

bool check_game_over(GameState* gs) {
    if (count_alive_players(gs) == 0) {
        gs->game_over = true;
        gs->winner = 1; // enemies win
        add_log(gs, "=== ALL PLAYERS DEFEATED! ENEMIES WIN! ===");
        return true;
    }
    if (count_alive_enemies(gs) == 0) {
        gs->game_over = true;
        gs->winner = 0; // players win
        add_log(gs, "=== ALL ENEMIES DEFEATED! PLAYERS WIN! ===");
        return true;
    }
    return false;
}

// ================================================================
// DEADLOCK DETECTION (Section 7)
// ================================================================

bool check_deadlock(GameState* gs) {
    // Check for circular wait among artifact holders
    // Simple: if A holds artifact X and waits for Y, and B holds Y and waits for X
    for (int i = 0; i < 3; i++) {
        if (!gs->artifacts[i].locked || gs->artifacts[i].waiting_id < 0) continue;
        for (int j = 0; j < 3; j++) {
            if (i == j) continue;
            if (!gs->artifacts[j].locked || gs->artifacts[j].waiting_id < 0) continue;

            // Check: owner of i is waiting for j, owner of j is waiting for i
            if (gs->artifacts[i].owner_id == gs->artifacts[j].waiting_id &&
                gs->artifacts[i].owner_type == gs->artifacts[j].waiting_type &&
                gs->artifacts[j].owner_id == gs->artifacts[i].waiting_id &&
                gs->artifacts[j].owner_type == gs->artifacts[i].waiting_type) {
                return true;
            }
        }
    }
    return false;
}

void resolve_deadlock(GameState* gs) {
    // Force the first locked artifact to be released
    for (int i = 0; i < 3; i++) {
        if (gs->artifacts[i].locked) {
            char buf[128];
            const char* aname = (i == 0) ? "Solar Core" : (i == 1) ? "Lunar Blade" : "Eclipse Relic";
            snprintf(buf, sizeof(buf), "[DEADLOCK] Forcing release of %s!", aname);
            add_log(gs, buf);
            gs->artifacts[i].locked = false;
            gs->artifacts[i].owner_id = -1;
            gs->artifacts[i].owner_type = -1;
            break;
        }
    }
}

// ================================================================
// ECLIPSE RELIC (Section 7)
// ================================================================

void spawn_eclipse_relic(GameState* gs) {
    gs->artifacts[2].exists = true;
    gs->artifacts[2].locked = false;
    gs->artifacts[2].owner_id = -1;
    gs->artifacts[2].owner_type = -1;
    gs->artifacts[2].waiting_id = -1;
    gs->artifacts[2].waiting_type = -1;
    add_log(gs, "*** The Eclipse Relic has appeared! ***");
}
