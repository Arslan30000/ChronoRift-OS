#include "../shared.h"
#include "game_logic.h"
#include "renderer.h"
#include <iostream>
#include <time.h>

GameState* state = NULL;

// ===================== SIGNAL HANDLERS =====================
void handle_alarm(int sig) {
    if (sig == SIGALRM && state) {
        state->ultimate_active = false;
        if (state->asp_pid > 0) {
            kill(state->asp_pid, SIGCONT);
        }
        add_log(state, ">> Ultimate Ability window has ended. ASP resumed.");
    }
}

// ===================== DEADLOCK MONITOR THREAD =====================
void* deadlock_monitor(void* arg) {
    GameState* gs = (GameState*)arg;
    while (!gs->game_over) {
        sem_wait(&gs->artifact_mutex);
        if (check_deadlock(gs)) {
            resolve_deadlock(gs);
        }
        sem_post(&gs->artifact_mutex);
        usleep(500000);
    }
    return NULL;
}

// ===================== STUN TIMER THREAD =====================
void* stun_timer_thread(void* arg) {
    GameState* gs = (GameState*)arg;
    while (!gs->game_over) {
        sem_wait(&gs->mutex);
        time_t now = time(NULL);
        for (int i = 0; i < gs->num_players; i++) {
            if (gs->players[i].is_stunned && gs->players[i].stun_start > 0) {
                if (now - gs->players[i].stun_start >= 3) {
                    gs->players[i].is_stunned = false;
                    gs->players[i].stun_start = 0;
                    char buf[128];
                    snprintf(buf, sizeof(buf), ">> %s recovered from stun!", gs->players[i].name);
                    add_log(gs, buf);
                }
            }
        }
        for (int i = 0; i < gs->num_enemies; i++) {
            if (gs->enemies[i].is_stunned && gs->enemies[i].stun_start > 0) {
                if (now - gs->enemies[i].stun_start >= 3) {
                    gs->enemies[i].is_stunned = false;
                    gs->enemies[i].stun_start = 0;
                    char buf[128];
                    snprintf(buf, sizeof(buf), ">> %s recovered from stun!", gs->enemies[i].name);
                    add_log(gs, buf);
                }
            }
        }
        sem_post(&gs->mutex);
        usleep(200000);
    }
    return NULL;
}

// ===================== MAIN =====================
int main() {
    srand(ROLL_SEED);

    // Clean up any stale shared memory
    shm_unlink(SHM_NAME);

    // Create shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(GameState));
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Zero out and initialize
    memset(state, 0, sizeof(GameState));
    sem_init(&state->mutex, 1, 1);
    sem_init(&state->action_sem, 1, 0);
    sem_init(&state->artifact_mutex, 1, 1);

    state->phase = PHASE_WAITING;
    state->active_turn_type = -1;
    state->active_turn_id = -1;
    state->game_over = false;

    // Initialize enemy count (random 2-9)
    state->num_enemies = rand() % 8 + 2;

    // Initialize enemies with roll-number formulas
    for (int i = 0; i < state->num_enemies; i++) {
        state->enemies[i].id = i;
        strncpy(state->enemies[i].name, ENEMY_NAMES[i], 31);
        state->enemies[i].max_hp = ROLL_LAST_TWO + (rand() % 151 + 50); // 72 + rand(50..200)
        state->enemies[i].hp = state->enemies[i].max_hp;
        state->enemies[i].damage = ROLL_SECOND_LAST + 10;               // 7 + 10 = 17
        state->enemies[i].speed = rand() % 21 + 10;                     // rand(10..30)
        state->enemies[i].stamina = 0;
        state->enemies[i].max_stamina = 150;
        state->enemies[i].is_alive = true;
        state->enemies[i].is_stunned = false;
        state->enemies[i].stun_start = 0;
        memset(&state->enemies[i].inv, 0, sizeof(Inventory));
    }

    // Initialize artifacts
    state->artifacts[0].exists = true;  // Solar Core
    state->artifacts[0].locked = false;
    state->artifacts[0].owner_id = -1;
    state->artifacts[0].owner_type = -1;
    state->artifacts[0].waiting_id = -1;
    state->artifacts[1].exists = true;  // Lunar Blade
    state->artifacts[1].locked = false;
    state->artifacts[1].owner_id = -1;
    state->artifacts[1].owner_type = -1;
    state->artifacts[1].waiting_id = -1;
    state->artifacts[2].exists = false; // Eclipse Relic (spawns later)

    // Setup signal handler for Ultimate Ability
    signal(SIGALRM, handle_alarm);

    // Start threads
    pthread_t render_tid, deadlock_tid, stun_tid;
    pthread_create(&render_tid, NULL, render_thread_func, state);
    pthread_create(&deadlock_tid, NULL, deadlock_monitor, state);
    pthread_create(&stun_tid, NULL, stun_timer_thread, state);

    add_log(state, "=== Chrono Rift Arbiter initialized ===");

    char buf[128];
    snprintf(buf, sizeof(buf), "Enemies spawned: %d", state->num_enemies);
    add_log(state, buf);
    add_log(state, "Waiting for HIP and ASP to connect...");

    // Wait for both processes to connect
    while (!state->game_over) {
        sem_wait(&state->mutex);
        bool ready = state->hip_connected && state->asp_connected;
        sem_post(&state->mutex);
        if (ready) break;
        usleep(100000);
    }

    // Transition to battle
    sem_wait(&state->mutex);
    state->phase = PHASE_BATTLE;
    add_log(state, "=== BATTLE BEGINS! ===");
    snprintf(buf, sizeof(buf), "Players: %d vs Enemies: %d", state->num_players, state->num_enemies);
    add_log(state, buf);
    sem_post(&state->mutex);

    // ===================== MAIN GAME LOOP =====================
    while (!state->game_over) {
        sem_wait(&state->mutex);

        // Check game over
        if (check_game_over(state)) {
            state->phase = PHASE_GAME_OVER;
            sem_post(&state->mutex);
            break;
        }

        // Handle weapon drop timeout (if pending and player chose)
        if (state->weapon_drop.pending && state->weapon_drop.player_chose) {
            if (state->weapon_drop.player_took) {
                const Weapon* dw = get_weapon_by_id(state->weapon_drop.weapon_id);
                if (dw) {
                    allocate_weapon(&state->players[state->weapon_drop.killer_id], dw);
                    snprintf(buf, sizeof(buf), ">> Player %d picked up %s!", state->weapon_drop.killer_id, dw->name);
                    add_log(state, buf);
                }
            } else {
                // Enemy guaranteed to pick it up
                const Weapon* dw = get_weapon_by_id(state->weapon_drop.weapon_id);
                if (dw) {
                    for (int i = 0; i < state->num_enemies; i++) {
                        if (state->enemies[i].is_alive) {
                            allocate_weapon(&state->enemies[i], dw);
                            snprintf(buf, sizeof(buf), ">> %s picked up %s!", state->enemies[i].name, dw->name);
                            add_log(state, buf);
                            break;
                        }
                    }
                }
            }
            state->weapon_drop.pending = false;
        }

        // If no active turn and no pending action and not ultimate
        if (state->active_turn_type < 0 && !state->pending_action.is_ready && !state->ultimate_active) {
            // Check if anyone is ready
            bool someone_ready = false;
            int ready_type = -1, ready_id = -1;

            for (int i = 0; i < state->num_players; i++) {
                if (state->players[i].is_alive && !state->players[i].is_stunned &&
                    state->players[i].stamina >= state->players[i].max_stamina) {
                    someone_ready = true;
                    if (ready_type < 0) { ready_type = 0; ready_id = i; }
                }
            }
            for (int i = 0; i < state->num_enemies; i++) {
                if (state->enemies[i].is_alive && !state->enemies[i].is_stunned &&
                    state->enemies[i].stamina >= state->enemies[i].max_stamina) {
                    someone_ready = true;
                    if (ready_type < 0) { ready_type = 1; ready_id = i; }
                }
            }

            if (someone_ready) {
                // Assign the turn
                state->active_turn_type = ready_type;
                state->active_turn_id = ready_id;
                Entity* ae = (ready_type == 0) ? &state->players[ready_id] : &state->enemies[ready_id];
                snprintf(buf, sizeof(buf), "--- %s's turn! ---", ae->name);
                add_log(state, buf);

                if (ready_type == 1) {
                    state->npc_turn_start = time(NULL);
                }
            } else {
                // Tick stamina for all alive, non-stunned entities
                for (int i = 0; i < state->num_players; i++) {
                    if (state->players[i].is_alive && !state->players[i].is_stunned &&
                        state->players[i].stamina < state->players[i].max_stamina) {
                        state->players[i].stamina += state->players[i].speed;
                        if (state->players[i].stamina > state->players[i].max_stamina)
                            state->players[i].stamina = state->players[i].max_stamina;
                    }
                }
                for (int i = 0; i < state->num_enemies; i++) {
                    if (state->enemies[i].is_alive && !state->enemies[i].is_stunned &&
                        state->enemies[i].stamina < state->enemies[i].max_stamina) {
                        state->enemies[i].stamina += state->enemies[i].speed;
                        if (state->enemies[i].stamina > state->enemies[i].max_stamina)
                            state->enemies[i].stamina = state->enemies[i].max_stamina;
                    }
                }
                state->tick_count++;
            }
        }

        // NPC turn timeout (3 seconds - Section 8)
        if (state->active_turn_type == 1 && !state->pending_action.is_ready) {
            time_t now = time(NULL);
            if (now - state->npc_turn_start >= 3) {
                // Auto-skip
                state->pending_action.sender_type = 1;
                state->pending_action.sender_id = state->active_turn_id;
                state->pending_action.type = ACT_SKIP;
                state->pending_action.target_id = 0;
                state->pending_action.is_ready = true;
                sem_post(&state->action_sem);
                add_log(state, ">> NPC timed out! Turn skipped.");
            }
        }

        sem_post(&state->mutex);

        // Process committed action
        if (sem_trywait(&state->action_sem) == 0) {
            sem_wait(&state->mutex);
            if (state->pending_action.is_ready) {
                process_action(state, &state->pending_action);
                state->pending_action.is_ready = false;
                state->active_turn_type = -1;
                state->active_turn_id = -1;

                // Handle ultimate ability timing
                if (state->pending_action.type == ACT_ULTIMATE) {
                    alarm(10); // 10-second timer for SIGALRM
                }

                check_game_over(state);
                if (state->game_over) {
                    state->phase = PHASE_GAME_OVER;
                }
            }
            sem_post(&state->mutex);
        }

        usleep(50000); // 20 ticks per second
    }

    // Wait a bit for game over screen to display
    sleep(10);

    // Cleanup
    pthread_join(render_tid, NULL);
    sem_destroy(&state->mutex);
    sem_destroy(&state->action_sem);
    sem_destroy(&state->artifact_mutex);
    shm_unlink(SHM_NAME);

    return 0;
}