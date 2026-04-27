#include "../shared.h"
#include <iostream>

GameState* state;

// Asynchronous Stun Mechanic
void handle_stun(int sig) {
    if (sig == SIGUSR1) {
        sem_wait(&state->mutex);
        for(int i = 0; i < state->num_enemies; i++) {
            if(state->enemies[i].process_id == getpid()) state->enemies[i].is_stunned = true;
        }
        sem_post(&state->mutex);
    }
}

void* enemy_thread(void* arg) {
    int id = *(int*)arg;
    
    while(!state->game_over) {
        sem_wait(&state->mutex);
        if(state->enemies[id].is_alive && !state->enemies[id].is_stunned && state->enemies[id].stamina >= state->enemies[id].max_stamina && !state->pending_action.is_ready) {
            
            // 3 Second Rule Logic handled by immediate execution here
            int target = 0;
            for(int i=0; i<state->num_players; i++) {
                if(state->players[i].is_alive) { target = i; break; }
            }
            
            state->pending_action.sender_type = 1;
            state->pending_action.sender_id = id;
            state->pending_action.target_id = target;
            state->pending_action.type = STRIKE;
            state->pending_action.is_ready = true;
            
            sem_post(&state->action_sem);
        }
        sem_post(&state->mutex);
        usleep(200000); 
    }
    return NULL;
}

int main() {
    int shm_fd = shm_open("/chrono_shm", O_RDWR, 0666);
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    signal(SIGUSR1, handle_stun);

    sem_wait(&state->mutex);
    state->asp_pid = getpid(); // Register for Ultimate Ability pausing
    for(int i = 0; i < state->num_enemies; i++) {
        state->enemies[i].id = i;
        state->enemies[i].process_id = getpid();
        state->enemies[i].hp = 150;
        state->enemies[i].max_hp = 150;
        state->enemies[i].damage = 17; // 23i-0572 Enemy Base
        state->enemies[i].speed = 20;
        state->enemies[i].stamina = 0;
        state->enemies[i].max_stamina = 150;
        state->enemies[i].is_alive = true;
        state->enemies[i].is_stunned = false;
    }
    sem_post(&state->mutex);

    pthread_t tids[9];
    int ids[9];
    for(int i = 0; i < state->num_enemies; i++) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, enemy_thread, &ids[i]);
    }
    for(int i = 0; i < state->num_enemies; i++) pthread_join(tids[i], NULL);
    return 0;
}