#include <SDL.h>
#define GLAD_GLES2_IMPLEMENTATION
#include "gles2.h"
#undef GLAD_GLES2_IMPLEMENTATION

#include <filesystem>
#include <string>

#include <lo/lo.h>
#include <samplerate.h>
#include <sndfile.h>

#include "dirt-imconfig.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "config.h"
#include "log-imgui.h"

extern "C"
{
#include "common.h"
extern int audio_init(const char *output, bool dirty_compressor, bool autoconnect, bool late_trigger, unsigned int num_workers, const char *sampleroot, bool shape_gain_comp, bool preload_flag);
extern int server_init(const char *osc_port);
};

std::string pref_path = "";

void initialize_paths()
{
  if (! SDL_Init(0))
  {
#ifdef __ANDROID__
    const char *default_path = SDL_AndroidGetExternalStoragePath();
    if (default_path)
    {
      // can be accessed by other apps
      std::filesystem::current_path(std::string(default_path));
    }
    else
    {
      // last resort, cannot be accessed outside this app
      std::filesystem::current_path(pref_path);
    }
#endif
    // SDL_Quit();
  }
}

char liblo_version_string[64] = {0};
char *libsamplerate_version_string = nullptr;
const char *libsndfile_version_string = nullptr;

bool really_restart = false;
bool really_clear = false;

int active_osc_port = 0;
char osc_port_string[64] = {0};
int osc_port = 0;

bool dirty_compressor_flag = true;
bool jack_auto_connect_flag = true;
bool late_trigger_flag = true;
int num_workers = 2;
const char *sample_dir = "./samples/";
bool shape_gain_comp_flag = false;
bool preload_flag = false;
int num_channels = DEFAULT_CHANNELS;
int gain_db = DEFAULT_GAIN_DB;
int samplerate = DEFAULT_SAMPLERATE;

const char *samplerate_names[] = { "16000", "22050", "32000", "44100", "48000", "88200", "96000" };
const int   samplerate_value[] = {  16000 ,  22050 ,  32000 ,  44100 ,  48000 ,  88200 ,  96000  };
int         samplerate_index = 3;

bool display(bool server_running, bool audio_running)
{
  bool restart = false;
  ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImVec2 size(ImGui::GetIO().DisplaySize.x * 0.95f, ImGui::GetIO().DisplaySize.y * 0.95f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
  ImGui::Begin("Dirt");
  if (liblo_version_string[0] == 0)
  {
    lo_version(liblo_version_string, sizeof(liblo_version_string), 0, 0, 0, 0, 0, 0, 0);
    libsamplerate_version_string = strdup(src_get_version());
    char *space = strchr(libsamplerate_version_string, ' ');
    if (space)
    {
      *space = 0;
    }
    libsndfile_version_string = sf_version_string();
  }
  ImGui::Text
    ( "Dirt-%s (c) 2015-2025 Alex McLean and contributors\n"
      "using: "
      "liblo-%s, "
      "%s,\n"
      "%s, "
      "imgui-%s\n"
    , DIRT_VERSION_STRING
    , liblo_version_string
    , libsndfile_version_string
    , libsamplerate_version_string
    , IMGUI_GIT_VERSION_STRING
    );
  ImGui::Text("OSC server is %s", server_running ? "running" : "not running");
  ImGui::Text("Audio engine is %s", audio_running ? "running" : "not running");

  ImGui::BeginDisabled(audio_running || server_running);
  ImGui::Checkbox("##ReallyRestart", &really_restart);
  ImGui::SameLine();
  if (ImGui::Button((audio_running || server_running) ? "Restarting with new settings not yet supported" : "Start") && really_restart)
  {
    really_restart = false;
    restart = true;
  }
  ImGui::EndDisabled();

  if (osc_port == 0)
  {
    osc_port = atoi(osc_port_string);
  }
  if (ImGui::InputInt("OSC Port", &osc_port))
  {
    if (! (1024 <= osc_port && osc_port < 65536))
    {
      osc_port = atoi(DEFAULT_OSC_PORT);
    }
    snprintf(osc_port_string, sizeof(osc_port_string), "%d", osc_port);
  }

  if (ImGui::InputInt("Channels", &num_channels))
  {
    if (! (MIN_CHANNELS <= num_channels && num_channels <= MAX_CHANNELS))
    {
      num_channels = DEFAULT_CHANNELS;
    }
  }

  if (ImGui::Combo("Sample Rate", &samplerate_index, samplerate_names, IM_ARRAYSIZE(samplerate_names)))
  {
    // nop
  }

  ImGui::Checkbox("Dirty Compressor", &dirty_compressor_flag);

  ImGui::Checkbox("Shape Gain Compensation", &shape_gain_comp_flag);

  if (ImGui::InputInt("Gain dB", &gain_db))
  {
    if (! (MIN_GAIN_DB <= gain_db && gain_db <= MAX_GAIN_DB))
    {
      gain_db = DEFAULT_GAIN_DB;
    }
  }

  ImGui::Checkbox("Late Trigger", &late_trigger_flag);

  if (ImGui::InputInt("Workers", &num_workers))
  {
    if (! (1 <= num_workers && num_workers <= 8))
    {
      num_workers = DEFAULT_WORKERS;
    }
  }

  ImGui::Text("Samples Root Path: %s", sample_dir);

  ImGui::Checkbox("##ReallyClear", &really_clear);
  ImGui::SameLine();
  if (ImGui::Button("Clear Log") && really_clear)
  {
    really_clear = false;
    log_clear();
  }
  log_display();
  ImGui::End();

  return restart;
}

bool want_capture(int type)
{
  ImGuiIO& io = ImGui::GetIO();
  return
    (io.WantCaptureMouse && (
      type == SDL_MOUSEBUTTONDOWN ||
      type == SDL_MOUSEBUTTONUP ||
      type == SDL_MOUSEWHEEL ||
      type == SDL_MOUSEMOTION ||
      type == SDL_FINGERDOWN ||
      type == SDL_FINGERUP ||
      type == SDL_FINGERMOTION)) ||
    (io.WantCaptureKeyboard && (
      type == SDL_KEYDOWN ||
      type == SDL_KEYUP)) ;
}

bool gui(SDL_Window* window, bool &running, bool server_running, bool audio_running)
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  bool restart = display(server_running, audio_running);

  ImGui::Render();

  int win_pixel_width;
  int win_pixel_height;
  SDL_GL_GetDrawableSize(window, &win_pixel_width, &win_pixel_height);
  glViewport(0, 0, win_pixel_width, win_pixel_height);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window);

  return restart;
}

GLADapiproc get_proc_address(void *userptr, const char *name)
{
  (void) userptr;
  return (GLADapiproc) SDL_GL_GetProcAddress(name);
}

int main(int argc, char **argv)
{
#ifdef __ANDROID__
  SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
  SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "0");
#endif

  log_init();
  initialize_paths();
  snprintf(osc_port_string, sizeof(osc_port_string), "%s", DEFAULT_OSC_PORT);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    const std::string message = "SDL_Init: " + std::string(SDL_GetError());
    if (0 != SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dirt", message.c_str(), nullptr))
    {
      //std::cerr << progname << ": " << message << std::endl;
    }
    return 1;
  }

#ifdef __ANDROID__
  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
  {
    const std::string message = "SDL_GetDesktopDisplayMode: " + std::string(SDL_GetError());
    if (0 != SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dirt", message.c_str(), nullptr))
    {
      SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
    }
    SDL_Quit();
    return 1;
  }
  int win_screen_width = mode.w;
  int win_screen_height = mode.h;
  int ui_scale = 200;
#else
  int win_screen_width = 400;
  int win_screen_height = 600;
  int ui_scale = 100;
#endif

  // decide GL+GLSL versions
  // see dirt-imconfig.h
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, OPENGL_FLAGS);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, OPENGL_PROFILE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OPENGL_MAJOR);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, OPENGL_MINOR);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE /* | SDL_WINDOW_ALLOW_HIGHDPI */);
  SDL_Window* window = SDL_CreateWindow("Dirt", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_screen_width, win_screen_height, window_flags);
  if (! window)
  {
    const std::string message = "SDL_CreateWindow: " + std::string(SDL_GetError());
    if (0 != SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dirt", message.c_str(), nullptr))
    {
      SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
    }
    SDL_Quit();
    return 1;
  }
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (! gl_context)
  {
    const std::string message = "SDL_GL_CreateContext: " + std::string(SDL_GetError());
    if (0 != SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dirt", message.c_str(), window))
    {
      SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
    }
    SDL_Quit();
    return 1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  gladLoadGLES2UserPtr(get_proc_address, nullptr);
  const char *gl_version = (const char *) glGetString(GL_VERSION);

  SDL_GL_SetSwapInterval(1);
  int win_pixel_width = 0;
  int win_pixel_height = 0;
  SDL_GL_GetDrawableSize(window, &win_pixel_width, &win_pixel_height);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glClearColor(1, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  // setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(OPENGL_GLSL_VERSION);

  ImGui::GetIO().FontGlobalScale = ui_scale / 100.0f;

  // main loop
  bool audio_running = false;
  bool server_running = false;
  bool running = true;
  SDL_Event e;
  while (running)
  {
    bool restart = gui(window, running, server_running, audio_running);

    if (restart)
    {
#if 0
      if (server_running)
      {
        server_destroy();
        server_running = false;
      }
      if (audio_running)
      {
        audio_destroy();
        audio_running = false;
      }
#endif
      log_clear();

      g_num_channels = num_channels;
      g_samplerate = samplerate_value[samplerate_index];
      g_gain = 16.0f * powf(10.0f, gain_db / 20.0f);
      if (! audio_running)
      {
        audio_running = audio_init
          ( "sdl2"
          , dirty_compressor_flag
          , jack_auto_connect_flag
          , late_trigger_flag
          , num_workers
          , sample_dir
          , shape_gain_comp_flag
          , preload_flag
          );
      }
      if (! server_running)
      {
        server_running = server_init(osc_port_string);
      }
    }

    while (running && SDL_PollEvent(&e))
    {
      ImGui_ImplSDL2_ProcessEvent(&e);
      if (! want_capture(e.type))
      {
        switch (e.type)
        {
          case SDL_QUIT:
          {
            running = false;
            break;
          }
        }
      }
    }
  }

  // cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  log_destroy();

  return 0;
}
