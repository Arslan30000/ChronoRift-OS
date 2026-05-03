#include "../shared.h"
#include <iostream>

GameState* state = NULL;

// ===================== STUN HANDLER =====================
void handle_stun(int sig) {
    (void)sig;
    // Async stun: sleep blocks this process for 3 seconds
    sleep(3);
}

// ===================== PLAYER THREAD =====================
// Reads input from GUI (shared memory) instead of terminal
void* player_thread(void* arg) {
    int id = *(int*)arg;

    while (!state->game_over) {
        bool my_turn = false;

        sem_wait(&state->mutex);
        if (state->players[id].is_alive &&
            state->active_turn_type == 0 &&
            state->active_turn_id == id &&
            !state->pending_action.is_ready &&
            !state->players[id].is_stunned) {
            my_turn = true;
        }
        sem_post(&state->mutex);

        if (!my_turn) {
            usleep(100000);
            continue;
        }

        // ---- Request action from GUI ----
        sem_wait(&state->mutex);
        state->gui.phase = 1; // action selection
        state->gui.waiting = true;
        state->gui.for_player_id = id;
        state->gui.input_ready = false;
        state->gui.selected_action = 0;
        state->gui.selected_target = -1;
        state->gui.selected_weapon = 0;
        sem_post(&state->mutex);

        // Wait for GUI to provide action (and target if needed)
        while (!state->game_over) {
            sem_wait(&state->mutex);
            bool ready = state->gui.input_ready;
            sem_post(&state->mutex);
            if (ready) break;
            usleep(50000);
        }
        if (state->game_over) break;

        // Read the chosen action and target
        sem_wait(&state->mutex);
        int choice = state->gui.selected_action;
        int target = state->gui.selected_target;
        int weapon_id = state->gui.selected_weapon;
        state->gui.waiting = false;
        state->gui.input_ready = false;

        // For "Use Weapon" (choice 3), respect selected_weapon or auto-pick
        if (choice == 3) {
            bool has_it = false;
            for (int s = 0; s < INV_SLOTS; s++) {
                if (state->players[id].inv.slots[s] == weapon_id && weapon_id != 0) {
                    has_it = true; break;
                }
            }
            if (!has_it) {
                weapon_id = 0;
                for (int s = 0; s < INV_SLOTS; s++) {
                    if (state->players[id].inv.slots[s] != 0) {
                        weapon_id = state->players[id].inv.slots[s];
                        break;
                    }
                }
            }
            if (weapon_id == 0) choice = 1; // fallback to Strike
        }

        // For "Swap In" (choice 4), auto-pick first long-term weapon
        if (choice == 4) {
            if (state->players[id].inv.lt_count > 0) {
                weapon_id = 0; // lt_index 0
            } else {
                choice = 6; // fallback to Skip
            }
        }

        // Validate target for offensive actions
        if ((choice >= 1 && choice <= 3) && (target < 0 || target >= state->num_enemies || !state->enemies[target].is_alive)) {
            // Auto-target first alive enemy
            for (int i = 0; i < state->num_enemies; i++) {
                if (state->enemies[i].is_alive) { target = i; break; }
            }
        }

        // Submit action if still valid
        if (state->players[id].is_alive &&
            state->active_turn_type == 0 &&
            state->active_turn_id == id &&
            !state->pending_action.is_ready) {

            state->pending_action.sender_type = 0;
            state->pending_action.sender_id = id;
            state->pending_action.target_id = (target >= 0) ? target : id;
            state->pending_action.weapon_id = weapon_id;

            switch (choice) {
                case 1: state->pending_action.type = ACT_STRIKE; break;
                case 2: state->pending_action.type = ACT_EXHAUST; break;
                case 3: state->pending_action.type = ACT_USE_WEAPON; break;
                case 4: state->pending_action.type = ACT_SWAP_IN; break;
                case 5: state->pending_action.type = ACT_HEAL; break;
                case 7: state->pending_action.type = ACT_ULTIMATE; break;
                default: state->pending_action.type = ACT_SKIP; break;
            }

            state->pending_action.is_ready = true;
            sem_post(&state->action_sem);
        }
        sem_post(&state->mutex);

        usleep(100000);
    }
    return NULL;
}

// ===================== MAIN =====================
int main() {
    srand(time(NULL) ^ getpid());

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        std::cerr << "Error: Cannot open shared memory. Start the Arbiter first!\n";
        return 1;
    }
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    signal(SIGUSR1, handle_stun);

    // Party selection — still via terminal (one-time at startup)
    int num_p;
    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout << "║        CHRONO RIFT - HIP             ║\n";
    std::cout << "║   Human Interfacing Process          ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║   Enter party size (1-4): ";
    std::cin >> num_p;
    if (num_p < 1) num_p = 1;
    if (num_p > 4) num_p = 4;

    sem_wait(&state->mutex);
    state->num_players = num_p;
    for (int i = 0; i < num_p; i++) {
        state->players[i].id = i;
        strncpy(state->players[i].name, PLAYER_NAMES[i], 31);
        state->players[i].process_id = getpid();
        state->players[i].max_hp = ROLL_SEED + (rand() % 901 + 100);
        state->players[i].hp = state->players[i].max_hp;
        state->players[i].damage = ROLL_LAST + 10;
        state->players[i].speed = 100 / num_p;
        state->players[i].stamina = 0;
        state->players[i].max_stamina = 100;
        state->players[i].is_alive = true;
        state->players[i].is_stunned = false;
        state->players[i].stun_start = 0;
        memset(&state->players[i].inv, 0, sizeof(Inventory));
    }
    state->hip_connected = true;
    // Initialize GUI input
    memset(&state->gui, 0, sizeof(GuiInput));
    sem_post(&state->mutex);

    std::cout << "║   Party of " << num_p << " ready!\n";
    std::cout << "║   All controls are on the SFML window.\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    // Wait for battle to start
    while (state->phase == PHASE_WAITING && !state->game_over) {
        usleep(200000);
    }

    // Create player threads
    pthread_t tids[MAX_PLAYERS];
    int ids[MAX_PLAYERS];
    for (int i = 0; i < num_p; i++) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, player_thread, &ids[i]);
    }

    for (int i = 0; i < num_p; i++) {
        pthread_join(tids[i], NULL);
    }

    std::cout << "\n=== Game Over! ===\n";
    return 0;
}