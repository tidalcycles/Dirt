#include <SDL.h>
#define GLAD_GLES2_IMPLEMENTATION
#include "gles2.h"
#undef GLAD_GLES2_IMPLEMENTATION

#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <filesystem>

#include <imgui.h>

#include "dirt-imconfig.h"

#include "config.h"
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
    char *p = SDL_GetPrefPath("uk.co.mathr", "dirt");
    if (p)
    {
      pref_path = std::string(p);
      SDL_free(p);
    }
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
    SDL_Quit();
  }
}

bool really_stop = false;
void running_modal(bool &open)
{
  if (open)
  {
    ImGui::OpenPopup("Dirt");
  }
  ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Dirt", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::Text("Dirt is running.");
    ImGui::Checkbox("##ReallyStop", &really_stop);
    ImGui::SameLine();
    if (ImGui::Button("Stop") && really_stop)
    {
      open = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
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

void gui(SDL_Window* window, bool &running, int server_ok, int audio_ok)
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  running_modal(running);

  ImGui::Render();

  int win_pixel_width;
  int win_pixel_height;
  SDL_GL_GetDrawableSize(window, &win_pixel_width, &win_pixel_height);
  glViewport(0, 0, win_pixel_width, win_pixel_height);
  glClearColor(! server_ok, (server_ok + audio_ok) * 0.5, ! audio_ok, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window);
}

GLADapiproc get_proc_address(void *userptr, const char *name)
{
  (void) userptr;
  return (GLADapiproc) SDL_GL_GetProcAddress(name);
}

int main(int argc, char **argv)
{
  initialize_paths();

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
#else
  int win_screen_width = 1024;
  int win_screen_height = 576;
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

  // initialize_clipboard();
  float ui_scale = 200; // FIXME
  ImGui::GetIO().FontGlobalScale = ui_scale / 100.0f;

  // start dirt
  bool dirty_compressor_flag = true;
  bool jack_auto_connect_flag = true;
  bool late_trigger_flag = true;
  int num_workers = 2;
  const char *sampleroot = "./samples/";
  bool shape_gain_comp_flag = false;
  bool preload_flag = false;
  const char *osc_port = DEFAULT_OSC_PORT;
  g_num_channels = DEFAULT_CHANNELS;
  g_samplerate = DEFAULT_SAMPLERATE;
  g_gain = DEFAULT_GAIN;
  int audio_ok = audio_init
    ( "sdl2"
    , dirty_compressor_flag
    , jack_auto_connect_flag
    , late_trigger_flag
    , num_workers
    , sampleroot
    , shape_gain_comp_flag
    , preload_flag
    );
  int server_ok = server_init(osc_port);

  bool running = true;
  SDL_Event e;
  while (running)
  {
    gui(window, running, server_ok, audio_ok);
    if (running && SDL_WaitEvent(&e))
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
  return 0;
}
