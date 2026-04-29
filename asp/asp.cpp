#include "../shared.h"
#include <iostream>

GameState* state = NULL;

// ===================== STUN HANDLER =====================
void handle_stun(int sig) {
    (void)sig;
    // Async 3-second stun — blocks entire ASP process
    sleep(3);
}

// ===================== ENEMY THREAD =====================
void* enemy_thread(void* arg) {
    int id = *(int*)arg;

    while (!state->game_over) {
        bool my_turn = false;

        sem_wait(&state->mutex);
        if (state->enemies[id].is_alive &&
            !state->enemies[id].is_stunned &&
            state->active_turn_type == 1 &&
            state->active_turn_id == id &&
            !state->pending_action.is_ready) {
            my_turn = true;
        }
        sem_post(&state->mutex);

        if (!my_turn) {
            usleep(150000);
            continue;
        }

        // AI Decision making
        sem_wait(&state->mutex);

        // Find a random alive player target
        int alive_players[MAX_PLAYERS];
        int alive_count = 0;
        for (int i = 0; i < state->num_players; i++) {
            if (state->players[i].is_alive) {
                alive_players[alive_count++] = i;
            }
        }

        if (alive_count == 0) {
            sem_post(&state->mutex);
            usleep(100000);
            continue;
        }

        int target = alive_players[rand() % alive_count];

        // AI choice: 60% Strike, 10% Use Weapon, 30% Skip
        int roll = rand() % 100;
        ActionType chosen_action;
        int weapon_id = 0;

        if (roll < 60) {
            chosen_action = ACT_STRIKE;
        } else if (roll < 70) {
            // Try to use a weapon if we have one
            bool has_weapon = false;
            for (int s = 0; s < INV_SLOTS; s++) {
                if (state->enemies[id].inv.slots[s] != 0) {
                    weapon_id = state->enemies[id].inv.slots[s];
                    has_weapon = true;
                    break;
                }
            }
            chosen_action = has_weapon ? ACT_USE_WEAPON : ACT_STRIKE;
        } else {
            chosen_action = ACT_SKIP;
        }

        // Submit action
        state->pending_action.sender_type = 1;
        state->pending_action.sender_id = id;
        state->pending_action.target_id = target;
        state->pending_action.type = chosen_action;
        state->pending_action.weapon_id = weapon_id;
        state->pending_action.is_ready = true;
        sem_post(&state->action_sem);

        sem_post(&state->mutex);

        usleep(200000);
    }
    return NULL;
}

// ===================== MAIN =====================
int main() {
    srand(ROLL_SEED + 1); // Slightly different seed for enemy randomness

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        std::cerr << "Error: Cannot open shared memory. Start the Arbiter first!\n";
        return 1;
    }
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    signal(SIGUSR1, handle_stun);

    // Register ASP
    sem_wait(&state->mutex);
    state->asp_pid = getpid();
    // Set enemy process IDs
    for (int i = 0; i < state->num_enemies; i++) {
        state->enemies[i].process_id = getpid();
    }
    state->asp_connected = true;
    sem_post(&state->mutex);

    std::cout << "[ASP] Automated Strategic Process connected. PID: " << getpid() << "\n";
    std::cout << "[ASP] Managing " << state->num_enemies << " enemies.\n";

    // Wait for battle
    while (state->phase == PHASE_WAITING && !state->game_over) {
        usleep(200000);
    }

    // Create enemy threads
    pthread_t tids[MAX_ENEMIES];
    int ids[MAX_ENEMIES];
    for (int i = 0; i < state->num_enemies; i++) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, enemy_thread, &ids[i]);
    }

    for (int i = 0; i < state->num_enemies; i++) {
        pthread_join(tids[i], NULL);
    }

    std::cout << "[ASP] Game Over. Shutting down.\n";
    return 0;
}