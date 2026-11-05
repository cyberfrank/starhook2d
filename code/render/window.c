#include "window.h"
#include "foundation/log.h"
#include "input_system.h"
#include "gl/glad.h"
#include <GLFW/glfw3.h>

#if defined(OS_WINDOWS)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <glfw/glfw3native.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#endif

// Dummy vertex array object
uint32_t global_vao;

static void GLAPIENTRY gl_debug_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity, 
    GLsizei length, const GLchar *message, const void *ud)
{
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
    {
       return;
    }

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        log_error("%s (id=%u)", message, id);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        log_warn("%s (id=%u)", message, id);
        break;
    case GL_DEBUG_SEVERITY_LOW:
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        log_info("%s (id=%u)", message, id);
        break;
    }
}

static int from_glfw_action(int Action)
{
    switch (Action)
    {
    case GLFW_PRESS:   return INPUT_ACTION__PRESSED;
    case GLFW_RELEASE: return INPUT_ACTION__RELEASED;
    case GLFW_REPEAT:  return INPUT_ACTION__REPEATED;
    }
    return 0;
}

static void key_callback(GLFWwindow *win, int key, int scancode, int action, int mods)
{
    Input_Event event;
    event.button.code = key;
    event.button.action = from_glfw_action(action);
    input_feed_event(INPUT_EVENT_BUTTON, event);
}

static void mouse_move_callback(GLFWwindow *win, double x, double y)
{
    Input_Event event;
    event.mouse_move.pos_x = x;
    event.mouse_move.pos_y = y;
    input_feed_event(INPUT_EVENT_MOUSE_MOVE, event);
}

static void mouse_button_callback(GLFWwindow *win, int button, int action, int mods)
{
    Input_Event event;
    event.button.code = button;
    event.button.action = from_glfw_action(action);
    input_feed_event(INPUT_EVENT_BUTTON, event);
}

static void mouse_scroll_callback(GLFWwindow *win, double xoffset, double yoffset)
{
    Input_Event event;
    event.mouse_scroll.scroll_x = xoffset;
    event.mouse_scroll.scroll_y = yoffset;
    input_feed_event(INPUT_EVENT_MOUSE_SCROLL, event);
}

static void char_callback(GLFWwindow *win, unsigned int codepoint)
{
    // TODO
}

static void window_size_callback(GLFWwindow *win, int w, int h)
{
    glViewport(0, 0, w, h);
}

void *window_setup(Window_Settings *settings)
{
    const uint32_t msaa_samples = 0;
    const uint32_t color_bits = 8;
    const uint32_t alpha_bits = 8;
    const uint32_t depth_bits = 32;
    const uint32_t stencil_bits = 0;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, msaa_samples);
    glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);
    glfwWindowHint(GLFW_RED_BITS, color_bits);
    glfwWindowHint(GLFW_GREEN_BITS, color_bits);
    glfwWindowHint(GLFW_BLUE_BITS, color_bits);
    glfwWindowHint(GLFW_ALPHA_BITS, alpha_bits);
    glfwWindowHint(GLFW_DEPTH_BITS, depth_bits);
    glfwWindowHint(GLFW_STENCIL_BITS, stencil_bits);
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_DECORATED, true);
    glfwWindowHint(GLFW_VISIBLE, true);

#if defined(SHIPPING_BUILD)
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, false);
#else
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    int window_width = (int)(settings->width * settings->scale);
    int window_height = (int)(settings->height * settings->scale);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    GLFWwindow *window = glfwCreateWindow(window_width, window_height, settings->title, 0, 0);
    
    // Center window on screen
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    int x_pos = (mode->width - window_width) / 2;
    int y_pos = (mode->height - window_height) / 2;
    glfwSetWindowPos(window, x_pos, y_pos);
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(settings->vsync);

    // Set border color
#if defined(OS_WINDOWS)
    HWND hwnd = glfwGetWin32Window(window);

    COLORREF color = 0xffffffff;
    const bool colored_titlebar = true;
    if (colored_titlebar)
    {
        Vec4 background = settings->titlebar_color;
        uint32_t r = (uint32_t)(background.x * 255.f);
        uint32_t g = (uint32_t)(background.y * 255.f);
        uint32_t b = (uint32_t)(background.z * 255.f);

        // COLORREF format is 0x00bbggrr
        color = 0x00000000;
        color |= b << 16;
        color |= g << 8;
        color |= r;
    }
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));

    BOOL darkmode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkmode, sizeof(darkmode));
#endif

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
    {
        log_error("Failed to create OpenGL context");
        return 0;
    }

    // Create dummy vertex array object
    glGenVertexArrays(1, &global_vao);
    glBindVertexArray(global_vao);

    // Setup default state
    glDisable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_TRUE);
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_move_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    if (settings->hide_cursor)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_callback, 0);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
    }

    return window;
}

void window_destroy(void *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool is_window_close_requested(void *window)
{
    return glfwWindowShouldClose(window);
}

Rect window_rect(void *window)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    return (Rect) { 0.0f, 0.0f, (float)width, (float)height };
}

void poll_events()
{
    input_clear();

    glfwPollEvents();

    // Read gamepad state
    GLFWgamepadstate gamepad_state;
    int res = glfwGetGamepadState(0, &gamepad_state);
    if (res == GLFW_TRUE)
    {
        Input_Event event;

        for (uint32_t i = 0; i < MAX_NUM_GAMEPAD_BUTTONS; ++i)
        {
            event.gamepad.buttons[i] = (uint8_t)from_glfw_action(gamepad_state.buttons[i]);
        }

        for (uint32_t i = 0; i < MAX_NUM_GAMEPAD_AXES; ++i)
        {
            event.gamepad.axes[i] = gamepad_state.axes[i];
        }
        
        input_feed_event(INPUT_EVENT_GAMEPAD, event);
    }
}

void window_swap_buffers(void *window)
{
    glfwSwapBuffers(window);
}