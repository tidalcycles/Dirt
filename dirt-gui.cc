#include <SDL.h>
#include <SDL_opengl.h>

#include <filesystem>
#include <string>

#ifdef __ANDROID__
#include <fcntl.h>
#include <unistd.h>
#endif

// for version report
#include <lo/lo.h>
#include <samplerate.h>
#include <sndfile.h>
#ifdef JACK
#include <jack/jack.h>
#endif
#ifdef PULSE
#include <pulse/version.h>
#endif
#ifdef PORTAUDIO
#include <portaudio.h>
#endif

#include "dirt-imconfig.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#ifdef __ANDROID__
#include <imgui_impl_opengl3.h>
#else
#include <imgui_impl_opengl2.h>
#endif
#include <imfilebrowser.h>

#include "config.h"
#include "log-imgui.h"

extern "C"
{
#include "common.h"
extern int audio_init(const char *output, compressor_t compressor, bool autoconnect, bool late_trigger, int polyphony, unsigned int num_workers, const char *sampleroot, bool allow_unsafe_sample_paths, bool shape_gain_comp, bool preload_flag, bool output_time_flag);
extern int server_init(const char *osc_port);
};

const char *samples_go_here_html =
  "<!DOCTYPE html>\n"
  "<html><head><meta charset='UTF-8'>\n"
  "<title>Dirt samples go here</title>\n"
  "</head><body>\n"
  "<h1>Dirt samples go here</h1>\n"
  "<p>Put your folder of folder of WAV files next to this file.</p>\n"
  "<p>For more information see: "
  "<a href='https://github.com/tidalcycles/Dirt/tree/1.1-dev?tab=readme-ov-file#samples-on-android'>github.com/tidalcycles/Dirt</a>"
  "</p>\n"
  "</body></html>\n"
;

std::string pref_path = "";

void initialize_paths()
{
#ifdef __ANDROID__
  if (! SDL_Init(0))
  {
    char *p = SDL_GetPrefPath("org.tidalcycles", "dirt");
    if (p)
    {
      pref_path = std::string(p);
      SDL_free(p);
    }
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
    // SDL_Quit();
    int fd = open("README-samples-go-here.html", O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd != -1)
    {
      write(fd, samples_go_here_html, strlen(samples_go_here_html));
      close(fd);
    }
  }
#endif
}

char version_text[1024] = {0};

bool really_restart = false;
bool really_clear = false;

int active_osc_port = 0;
char osc_port_string[64] = {0};
int osc_port = 0;

bool jack_auto_connect_flag = true;
bool late_trigger_flag = true;
int num_workers = 2;
std::filesystem::path samples_path = "./samples/";
bool shape_gain_comp_flag = false;
bool preload_flag = false;
bool output_time_flag = true;
int num_channels = DEFAULT_CHANNELS;
int gain_db = DEFAULT_GAIN_DB;

const char *samplerate_names[] = { "16000", "22050", "32000", "44100", "48000", "88200", "96000" };
const int   samplerate_value[] = {  16000 ,  22050 ,  32000 ,  44100 ,  48000 ,  88200 ,  96000  };
int         samplerate_index = 3;

const char *polyphony_names[] = { "4", "8", "16", "32", "64", "128", "256", "512" };
const int   polyphony_value[] = {  4 ,  8 ,  16 ,  32 ,  64 ,  128 ,  256 ,  512  };
int         polyphony_index = 5;

const compressor_t compressor_value[] = { compressor_none, compressor_dirty, compressor_clean, compressor_dave };
int compressor_index = DEFAULT_COMPRESSOR;

const char *audioapi_names[] = { "SDL2"
#ifdef JACK
, "JACK"
#endif
#ifdef PULSE
, "PulseAudio"
#endif
#ifdef PORTAUDIO
, "PortAudio"
#endif
};
const char *audioapi_value[] = { "sdl2"
#ifdef JACK
, "jack"
#endif
#ifdef PULSE
, "pulse"
#endif
#ifdef PORTAUDIO
, "portaudio"
#endif
};
int audioapi_index = 0; // always available

bool display(bool server_running, bool audio_running, ImGui::FileBrowser *chooseDir)
{
  bool restart = false;
  ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImVec2 size(ImGui::GetIO().DisplaySize.x * 0.95f, ImGui::GetIO().DisplaySize.y * 0.95f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
  ImGui::Begin("Dirt");
  if (version_text[0] == 0)
  {
    char liblo_version_string[64] = {0};
    lo_version(liblo_version_string, sizeof(liblo_version_string), 0, 0, 0, 0, 0, 0, 0);
    char *libsamplerate_version_string = strdup(src_get_version());
    char *space = strchr(libsamplerate_version_string, ' ');
    if (space)
    {
      *space = 0;
    }
    const char *libsndfile_version_string = sf_version_string();
#ifdef JACK
    int jack_version_major, jack_version_minor, jack_version_micro, jack_version_proto;
    jack_get_version(&jack_version_major, &jack_version_minor, &jack_version_micro, &jack_version_proto);
#endif
#ifdef PULSE
    const char *pulseaudio_version_string = pa_get_library_version();
#endif
#ifdef PORTAUDIO
    const PaVersionInfo *portaudio_version = Pa_GetVersionInfo();
#endif
    SDL_version sdl_compiled;
    SDL_version sdl_linked;
    SDL_VERSION(&sdl_compiled);
    SDL_GetVersion(&sdl_linked);
    snprintf
      ( version_text
      , sizeof(version_text)
      , "Dirt-%s (c) 2015-2025 Alex McLean and contributors\n"
        "using:\n"
        "- SDL %d.%d.%d (using %d.%d.%d)\n"
        "- Dear ImGui %s\n"
        "- imgui-filebrowser %s\n"
#ifdef JACK
        "- JACK %d.%d.%d (protocol version %d)\n"
#endif
#ifdef PULSE
        "- PulseAudio %d.%d.%d (using %s)\n"
#endif
#ifdef PORTAUDIO
        "- PortAudio %d.%d.%d\n"
#endif
        "- liblo-%s\n"
        "- %s\n"
        "- %s\n"
#ifdef _WIN32
        "- win32ports/dirent_h\n"
#endif
      , DIRT_VERSION_STRING
      , sdl_compiled.major, sdl_compiled.minor, sdl_compiled.patch
      , sdl_linked.major, sdl_linked.minor, sdl_linked.patch
      , IMGUI_GIT_VERSION_STRING
      , IMGUI_FILE_BROWSER_GIT_VERSION_STRING
#ifdef JACK
      , jack_version_major, jack_version_minor, jack_version_micro, jack_version_proto
#endif
#ifdef PULSE
      , PA_MAJOR, PA_MINOR, PA_MICRO, pulseaudio_version_string
#endif
#ifdef PORTAUDIO
      , portaudio_version->versionMajor, portaudio_version->versionMinor, portaudio_version->versionSubMinor
#endif
      , liblo_version_string
      , libsndfile_version_string
      , libsamplerate_version_string
    );
    free(libsamplerate_version_string);
  }
  ImGui::TextUnformatted(version_text);

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

  if (ImGui::Combo("Audio Output", &audioapi_index, audioapi_names, IM_ARRAYSIZE(audioapi_names)))
  {
    // nop
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

  ImGui::Checkbox("Use Output DAC Time", &output_time_flag);

  if (ImGui::Combo("Compressor", &compressor_index, compressor_names, IM_ARRAYSIZE(compressor_value)))
  {
    // nop
  }

  ImGui::Checkbox("Shape Gain Compensation", &shape_gain_comp_flag);

  if (ImGui::InputInt("Gain dB", &gain_db))
  {
    if (! (MIN_GAIN_DB <= gain_db && gain_db <= MAX_GAIN_DB))
    {
      gain_db = DEFAULT_GAIN_DB;
    }
  }

  ImGui::Checkbox("Late Trigger", &late_trigger_flag);

  if (ImGui::Combo("Polyphony", &polyphony_index, polyphony_names, IM_ARRAYSIZE(polyphony_names)))
  {
    // nop
  }

  if (ImGui::InputInt("Workers", &num_workers))
  {
    if (! (1 <= num_workers && num_workers <= 8))
    {
      num_workers = DEFAULT_WORKERS;
    }
  }

  if (ImGui::Button("Samples Root Path"))
  {
    chooseDir->Open();
  }
  ImGui::Text("%s", samples_path.string().c_str());

  ImGui::EndDisabled();

  ImGui::Checkbox("##ReallyClear", &really_clear);
  ImGui::SameLine();
  if (ImGui::Button("Clear Log") && really_clear)
  {
    really_clear = false;
    log_clear();
  }
  log_display();

  ImGui::End();

  chooseDir->Display();
  if (chooseDir->HasSelected())
  {
    samples_path = std::filesystem::proximate(chooseDir->GetSelected(), std::filesystem::current_path());
    chooseDir->ClearSelected();
  }

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

bool gui(SDL_Window* window, bool &running, bool server_running, bool audio_running, ImGui::FileBrowser *chooseDir)
{
#ifdef __ANDROID__
  ImGui_ImplOpenGL3_NewFrame();
#else
  ImGui_ImplOpenGL2_NewFrame();
#endif
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  bool restart = display(server_running, audio_running, chooseDir);

  ImGui::Render();

  int win_pixel_width;
  int win_pixel_height;
  SDL_GL_GetDrawableSize(window, &win_pixel_width, &win_pixel_height);
  glViewport(0, 0, win_pixel_width, win_pixel_height);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

#ifdef __ANDROID__
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif
  SDL_GL_SwapWindow(window);

  return restart;
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
  int win_screen_height = 640;
  int ui_scale = 100;
#endif

  // decide GL+GLSL versions
  // see dirt-imconfig.h
#ifdef __ANDROID__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, OPENGL_FLAGS);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, OPENGL_PROFILE);
#endif
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
  SDL_GL_SetSwapInterval(1);

  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  // setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
#ifdef __ANDROID__
  ImGui_ImplOpenGL3_Init();
#else
  ImGui_ImplOpenGL2_Init();
#endif

  ImGui::GetIO().FontGlobalScale = ui_scale / 100.0f;

  ImGui::FileBrowser chooseDir(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_HideRegularFiles | ImGuiFileBrowserFlags_SkipItemsCausingError);
  chooseDir.SetTitle("Samples Root Path");
  chooseDir.SetWindowSize(win_screen_width - 50, 450);

  // main loop
  bool audio_running = false;
  bool server_running = false;
  bool running = true;
  SDL_Event e;
  while (running)
  {
    bool restart = gui(window, running, server_running, audio_running, &chooseDir);

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
          ( audioapi_value[audioapi_index]
          , compressor_value[compressor_index]
          , jack_auto_connect_flag
          , late_trigger_flag
          , polyphony_value[polyphony_index]
          , num_workers
          , strdup(samples_path.string().c_str()) // FIXME unicode issues, small memory leak
          , false /* allow unsafe sample paths */
          , shape_gain_comp_flag
          , preload_flag
          , output_time_flag
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
#ifdef __ANDROID__
  ImGui_ImplOpenGL3_Shutdown();
#else
  ImGui_ImplOpenGL2_Shutdown();
#endif
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  log_destroy();

  return 0;
}
