#include "../shared.h"
#include <iostream>

GameState* state;

void* player_thread(void* arg) {
    int id = *(int*)arg;
    
    while(!state->game_over) {
        sem_wait(&state->mutex);
        if(state->players[id].is_alive && state->players[id].stamina >= state->players[id].max_stamina && !state->pending_action.is_ready) {
            
            std::cout << "\n[Player " << id << "] Turn Ready! Select Action:\n";
            std::cout << "1. Strike\n2. Exhaust\n3. Heal\n4. Skip\nChoice: ";
            int choice;
            std::cin >> choice;

            state->pending_action.sender_type = 0;
            state->pending_action.sender_id = id;
            
            // Auto target first alive enemy
            int target = 0;
            for(int i=0; i<state->num_enemies; i++) {
                if(state->enemies[i].is_alive) { target = i; break; }
            }
            state->pending_action.target_id = target;

            switch(choice) {
                case 1: state->pending_action.type = STRIKE; break;
                case 2: state->pending_action.type = EXHAUST; break;
                case 3: state->pending_action.type = HEAL; break;
                default: state->pending_action.type = SKIP; break;
            }
            
            state->pending_action.is_ready = true;
            sem_post(&state->action_sem); // Signal Arbiter
        }
        sem_post(&state->mutex);
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
        state->players[i].hp = 500;
        state->players[i].max_hp = 500;
        state->players[i].damage = 12; // 23i-0572 Base
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