#pragma once
#include "foundation/basic.h"

typedef struct Input_Binding Input_Binding;
typedef struct Input_Button_Mapping Input_Button_Mapping;
typedef struct Input_Axis_Mapping Input_Axis_Mapping;
typedef enum Input_Event_Type Input_Event_Type;
typedef union Input_Event Input_Event; 

enum {
    /* Mouse buttons */
    MOUSE_BUTTON_1 = 0,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,
    MAX_NUM_MOUSE_BUTTONS,
    MOUSE_BUTTON_LEFT = MOUSE_BUTTON_1,
    MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_2,
    MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3,
};

enum {
    /* Printable keys */
    KEY_SPACE = 32,
    KEY_APOSTROPHE = 39,    /* ' */
    KEY_COMMA = 44,         /* , */
    KEY_MINUS,              /* - */
    KEY_PERIOD,             /* . */
    KEY_SLASH,              /* / */
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_SEMICOLON = 59,     /* ; */
    KEY_EQUAL = 61,         /* = */
    KEY_A = 65,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_LEFT_BRACKET,       /* [ */
    KEY_BACKSLASH,          /* \ */
    KEY_RIGHT_BRACKET,      /* ] */
    KEY_GRAVE_ACCENT,       /* ` */
    KEY_WORLD_1,            /* non-US #1 */
    KEY_WORLD_2,            /* non-US #2 */
    /* Function keys */
    KEY_ESCAPE = 256,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK = 280,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1 = 290,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_F25,
    KEY_KP_0  = 320,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,
    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER,
    KEY_MENU,
    /* Gamepad generic buttons */
    GAMEPAD_FACE_BTN_DOWN,
    GAMEPAD_FACE_BTN_RIGHT,
    GAMEPAD_FACE_BTN_LEFT,
    GAMEPAD_FACE_BTN_UP,
    GAMEPAD_LEFT_BUMPER,
    GAMEPAD_RIGHT_BUMPER,
    GAMEPAD_SPECIAL_1,
    GAMEPAD_SPECIAL_2,
    GAMEPAD_SPECIAL_3,
    GAMEPAD_LEFT_STICK,
    GAMEPAD_RIGHT_STICK,
    GAMEPAD_DPAD_UP,
    GAMEPAD_DPAD_RIGHT,
    GAMEPAD_DPAD_DOWN,
    GAMEPAD_DPAD_LEFT,
    MAX_NUM_BUTTONS,
};

enum {
    MAX_NUM_GAMEPAD_BUTTONS = 15,
    MAX_NUM_GAMEPAD_AXES = 6,
};

enum {
    GAMEPAD_LEFT_STICK_X,
    GAMEPAD_LEFT_STICK_Y,
    GAMEPAD_RIGHT_STICK_X,
    GAMEPAD_RIGHT_STICK_Y,
    GAMEPAD_LEFT_TRIGGER,
    GAMEPAD_RIGHT_TRIGGER,
};

enum {
    INPUT_ACTION__PRESSED,
    INPUT_ACTION__RELEASED,
    INPUT_ACTION__REPEATED,
};

enum {
    INPUT_BINDING_TYPE__BUTTON,
    INPUT_BINDING_TYPE__AXIS,
};

enum {
    MAX_NUM_INPUT_BINDINGS = 64,
    MAX_NUM_BUTTON_MAPPINGS = 64,
    MAX_NUM_AXIS_MAPPINGS = 64,
    MAX_NUM_INPUT_MAPPINGS_PER_BINDING = 4,
};

struct Input_Binding {
    int indices[MAX_NUM_INPUT_MAPPINGS_PER_BINDING];
    int count;
    int binding_type; // Any of INPUT_BINDING_TYPE__*
};

struct Input_Button_Mapping {
    int button;
};

struct Input_Axis_Mapping {
    int axis;
    float scale;
    float bias;
};

enum Input_Event_Type {
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_SCROLL,
    INPUT_EVENT_BUTTON, // Key or mouse button
    INPUT_EVENT_GAMEPAD,
};

union Input_Event {
    struct {
        double pos_x;
        double pos_y;
    } mouse_move;
    struct {
        double scroll_x;
        double scroll_y;
    } mouse_scroll;
    struct {
        int code;
        int action;
    } button;
    struct {
        uint8_t buttons[MAX_NUM_GAMEPAD_BUTTONS];
        float axes[MAX_NUM_GAMEPAD_AXES];
    } gamepad;
};

void input_feed_event(Input_Event_Type type, Input_Event event);
void input_clear();

int  input_add_binding(int binding_type);
bool input_add_axis_mapping(int id, int axis, float scale, float bias);
bool input_add_button_mapping(int id, int button);

float input_get_axis(int id);

bool input_button_down(int id);
bool input_button_pressed(int id);
bool input_button_released(int id);

Vec2 input_mouse_position();
Vec2 input_mouse_scroll();

bool input_is_using_gamepad();