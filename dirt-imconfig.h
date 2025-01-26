#pragma once

#define OPENGL_MAJOR 2
#define OPENGL_MINOR 0

#ifdef __ANDROID__

#define OPENGL_PROFILE SDL_GL_CONTEXT_PROFILE_ES
#define OPENGL_FLAGS 0

#endif
