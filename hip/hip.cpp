#include "../shared.h"
#include <iostream>

GameState* state;

void* player_thread(void* arg) {
    int id = *(int*)arg;
    
    while(!state->game_over) {
        bool turn_ready = false;
        
        // 1. Lock briefly just to check if it is our turn
        sem_wait(&state->mutex);
        if(state->players[id].is_alive && state->players[id].stamina >= state->players[id].max_stamina && !state->pending_action.is_ready) {
            turn_ready = true;
        }
        sem_post(&state->mutex); // 2. IMMEDIATELY RELEASE THE LOCK

        // 3. If it is our turn, handle the blocking human input OUTSIDE the mutex
        if(turn_ready) {
            std::cout << "\n=========================================\n";
            std::cout << "[Player " << id << "] Turn Ready! Select Action:\n";
            std::cout << "1. Strike (Standard Attack)\n";
            std::cout << "2. Exhaust (Drain Enemy Stamina)\n";
            std::cout << "3. Heal (Recover 10% HP)\n";
            std::cout << "4. Skip Turn\n";
            std::cout << "Choice: ";
            
            int choice;
            std::cin >> choice;

            int target = -1;
            
            // Only ask for a target if the action is offensive
            if (choice == 1 || choice == 2) {
                std::cout << "Select Target Enemy (ID): ";
                std::cin >> target;
                
                // Basic validation
                sem_wait(&state->mutex);
                if (target < 0 || target >= state->num_enemies || !state->enemies[target].is_alive) {
                    std::cout << "Invalid target! Defaulting to first alive enemy.\n";
                    target = -1;
                }
                sem_post(&state->mutex);

                // Auto-target fallback if human entered a dead enemy
                if (target == -1) {
                    sem_wait(&state->mutex);
                    for(int i=0; i<state->num_enemies; i++) {
                        if(state->enemies[i].is_alive) { target = i; break; }
                    }
                    sem_post(&state->mutex);
                }
            } else {
                target = id; // Self-target for Heal/Skip
            }

            // 4. Lock again to safely submit the chosen action to the Arbiter
            sem_wait(&state->mutex);
            
            // Double check that we are still alive and haven't been skipped by an interrupt
            if(state->players[id].is_alive && state->players[id].stamina >= state->players[id].max_stamina && !state->pending_action.is_ready) {
                state->pending_action.sender_type = 0;
                state->pending_action.sender_id = id;
                state->pending_action.target_id = target;

                switch(choice) {
                    case 1: state->pending_action.type = STRIKE; break;
                    case 2: state->pending_action.type = EXHAUST; break;
                    case 3: state->pending_action.type = HEAL; break;
                    default: state->pending_action.type = SKIP; break;
                }
                
                state->pending_action.is_ready = true;
                sem_post(&state->action_sem); // Signal the Arbiter that a move is waiting
            }
            sem_post(&state->mutex);
        }
        
        usleep(100000); 
    }
    return NULL;
}

int main() {
    int shm_fd = shm_open("/chrono_shm", O_RDWR, 0666);
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    int num_p;
    std::cout << "Enter party size (1-4): ";
    std::cin >> num_p;

    sem_wait(&state->mutex);
    state->num_players = num_p;
    for(int i = 0; i < state->num_players; i++) {
        state->players[i].id = i;
        state->players[i].process_id = getpid();
        
        // Project Rule: Player HP = Roll No (572) + Random(100 to 1000)
        state->players[i].max_hp = 572 + (rand() % 901 + 100); 
        state->players[i].hp = state->players[i].max_hp;
        
        // Project Rule: Player Damage = Last digit of Roll No (2) + 10
        state->players[i].damage = 12; 
        
        // Project Rule: Player Speed = 100 / Number of Players
        state->players[i].speed = 100 / num_p;
        
        state->players[i].stamina = 0;
        state->players[i].max_stamina = 100;
        state->players[i].is_alive = true;
        memset(state->players[i].inv.slots, 0, sizeof(int)*20);
    }
    sem_post(&state->mutex);

    pthread_t tids[4];
    int ids[4];
    for(int i = 0; i < num_p; i++) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, player_thread, &ids[i]);
    }
    for(int i = 0; i < num_p; i++) pthread_join(tids[i], NULL);
    return 0;
}