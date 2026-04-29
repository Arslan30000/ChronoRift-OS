#ifndef RENDERER_H
#define RENDERER_H

#include "../shared.h"
#include <SFML/Graphics.hpp>

// The render thread function — runs the SFML window loop
void* render_thread_func(void* arg);

#endif
