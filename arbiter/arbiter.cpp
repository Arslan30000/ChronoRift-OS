#include "../shared.h"
#include <SFML/Graphics.hpp>
#include <iostream>

GameState* state;

void allocate_weapon(Entity* e, Weapon w) {
    int consecutive = 0;
    int start_idx = -1;
    
    for(int i = 0; i < 20; i++) {
        if(e->inv.slots[i] == 0) {
            if(consecutive == 0) start_idx = i;
            consecutive++;
            if(consecutive == w.slot_size) break;
        } else {
            consecutive = 0;
        }
    }

    if(consecutive < w.slot_size) {
        for(int i = 0; i < 20; i++) {
            if(e->inv.slots[i] != 0) {
                e->inv.long_term[e->inv.lt_count++] = e->inv.slots[i];
                e->inv.slots[i] = 0;
            }
        }
        start_idx = 0; 
    }

    for(int i = start_idx; i < start_idx + w.slot_size; i++) {
        e->inv.slots[i] = w.id;
    }
}

void* deadlock_monitor(void* arg) {
    (void)arg;
    while(!state->game_over) {
        sem_wait(&state->mutex);
        if(state->solar_core.locked && state->lunar_blade.locked) {
            if(state->solar_core.waiting_id == state->lunar_blade.owner_id && 
               state->lunar_blade.waiting_id == state->solar_core.owner_id) {
                std::cout << "[DEADLOCK DETECTED] Forcing release of Solar Core.\n";
                state->solar_core.locked = false;
                state->solar_core.owner_id = -1;
            }
        }
        sem_post(&state->mutex);
        usleep(500000);
    }
    return NULL;
}

void* render_thread(void* arg) {
    (void)arg;
    sf::RenderWindow window(sf::VideoMode(800, 600), "Chrono Rift - Arbiter");
    window.setFramerateLimit(30);
    sf::Font font;
    font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

    while (window.isOpen() && !state->game_over) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        window.clear(sf::Color::Black);
        sem_wait(&state->mutex);

        sf::Text title("CHRONO RIFT | Opr: Muhammad Arslan | Seed: 23i-0572", font, 18);
        title.setFillColor(sf::Color::Cyan);
        title.setPosition(20, 20);
        window.draw(title);

        int y = 70;
        for(int i=0; i<state->num_players; i++) {
            if(state->players[i].is_alive) {
                sf::Text t; t.setFont(font); t.setCharacterSize(14); t.setFillColor(sf::Color::Green);
                char buf[128];
                snprintf(buf, sizeof(buf), "[P%d] HP:%d/%d STAM:%d/%d", i, state->players[i].hp, state->players[i].max_hp, state->players[i].stamina, state->players[i].max_stamina);
                t.setString(buf); t.setPosition(20, y); window.draw(t); y+=25;
            }
        }
        y += 20;
        for(int i=0; i<state->num_enemies; i++) {
            if(state->enemies[i].is_alive) {
                sf::Text t; t.setFont(font); t.setCharacterSize(14); t.setFillColor(sf::Color::Red);
                char buf[128];
                snprintf(buf, sizeof(buf), "[E%d] HP:%d/%d STAM:%d/%d %s", i, state->enemies[i].hp, state->enemies[i].max_hp, state->enemies[i].stamina, state->enemies[i].max_stamina, state->enemies[i].is_stunned ? "STUN" : "");
                t.setString(buf); t.setPosition(20, y); window.draw(t); y+=25;
            }
        }
        sem_post(&state->mutex);
        window.display();
    }
    return NULL;
}

void handle_alarm(int sig) {
    if (sig == SIGALRM) {
        sem_wait(&state->mutex);
        state->ultimate_active = false;
        if (state->asp_pid > 0) {
            kill(state->asp_pid, SIGCONT); 
        }
        sem_post(&state->mutex);
    }
}

int main() {
    srand(572);
    shm_unlink("/chrono_shm");
    int shm_fd = shm_open("/chrono_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(GameState));
    state = (GameState*)mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    // CRITICAL FIX 1: Wipe memory cleanly BEFORE semaphore initialization
    memset(state, 0, sizeof(GameState));
    
    sem_init(&state->mutex, 1, 1);
    sem_init(&state->action_sem, 1, 0); 
    
    state->num_enemies = rand() % 8 + 2;
    state->solar_core.locked = false;
    state->lunar_blade.locked = false;

    signal(SIGALRM, handle_alarm);

    pthread_t render_tid, dead_tid;
    pthread_create(&render_tid, NULL, render_thread, NULL);
    pthread_create(&dead_tid, NULL, deadlock_monitor, NULL);

    while(!state->game_over) {
        sem_wait(&state->mutex);
        bool someone_ready = false;
        
        // CRITICAL FIX 4: Check if anyone is currently ready (Time Freeze mechanism)
        for(int i = 0; i < state->num_players; i++) {
            if(state->players[i].is_alive && state->players[i].stamina >= state->players[i].max_stamina) someone_ready = true;
        }
        for(int i = 0; i < state->num_enemies; i++) {
            if(state->enemies[i].is_alive && !state->enemies[i].is_stunned && state->enemies[i].stamina >= state->enemies[i].max_stamina) someone_ready = true;
        }

        // Only progress time if no one is waiting to take a turn
        if (!state->ultimate_active && !state->pending_action.is_ready && !someone_ready) {
            for(int i = 0; i < state->num_players; i++) {
                if(state->players[i].is_alive && state->players[i].stamina < state->players[i].max_stamina) {
                    state->players[i].stamina += state->players[i].speed;
                    // CRITICAL FIX 3: Clamp stamina strictly to max
                    if(state->players[i].stamina >= state->players[i].max_stamina) {
                        state->players[i].stamina = state->players[i].max_stamina;
                        someone_ready = true; // Instantly freeze time for next iteration
                    }
                }
            }
            for(int i = 0; i < state->num_enemies; i++) {
                if(state->enemies[i].is_alive && !state->enemies[i].is_stunned && state->enemies[i].stamina < state->enemies[i].max_stamina) {
                    state->enemies[i].stamina += state->enemies[i].speed;
                    // CRITICAL FIX 3: Clamp stamina strictly to max
                    if(state->enemies[i].stamina >= state->enemies[i].max_stamina) {
                        state->enemies[i].stamina = state->enemies[i].max_stamina;
                        someone_ready = true; // Instantly freeze time for next iteration
                    }
                }
            }
        }
        sem_post(&state->mutex);

        // Process Action Commitment
        if (someone_ready || state->pending_action.is_ready) {
            // CRITICAL FIX 2: Non-blocking semaphore check prevents render thread starvation
            if (sem_trywait(&state->action_sem) == 0) { 
                sem_wait(&state->mutex);
                
                ActionMessage msg = state->pending_action;
                Entity* actor = msg.sender_type == 0 ? &state->players[msg.sender_id] : &state->enemies[msg.sender_id];
                Entity* target = msg.sender_type == 0 ? &state->enemies[msg.target_id] : &state->players[msg.target_id];

                if (msg.type == STRIKE) {
                    target->hp -= actor->damage;
                    actor->stamina = 0;
                    
                    if(actor->damage > 50 && !target->is_stunned) {
                        kill(target->process_id, SIGUSR1);
                    }
                } else if (msg.type == EXHAUST) {
                    target->stamina -= actor->damage;
                    if(target->stamina < 0) target->stamina = 0;
                    actor->stamina = 0;
                } else if (msg.type == HEAL) {
                    actor->hp += actor->max_hp / 10;
                    // CRITICAL FIX 3: Clamp HP healing strictly to max
                    if (actor->hp > actor->max_hp) {
                        actor->hp = actor->max_hp;
                    }
                    actor->stamina = 0;
                } else if (msg.type == SKIP) {
                    actor->stamina = actor->max_stamina / 2;
                }

                if (target->hp <= 0) {
                    target->is_alive = false;
                    target->hp = 0; // Prevent negative HP rendering
                    if (msg.sender_type == 0) allocate_weapon(actor, WEAPONS[rand() % 8]); 
                }

                state->pending_action.is_ready = false;
                sem_post(&state->mutex);
            }
        }

        usleep(50000); 
    }

    pthread_join(render_tid, NULL);
    sem_destroy(&state->mutex);
    sem_destroy(&state->action_sem);
    shm_unlink("/chrono_shm");

    return 0;
}