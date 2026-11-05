#include "input_system.h"
#include "foundation/log.h"
#include "foundation/murmur_hash64.h"
#include "render/window.h"
#include "math.h"
#include <string.h>

enum {
    BUTTON_DOWN_BIT = 1 << 0,
    BUTTON_PRESSED_BIT = 1 << 1,
    BUTTON_RELEASED_BIT = 1 << 2,
};

typedef struct Input_State {
    // Input mapping
    Input_Binding bindings[MAX_NUM_INPUT_BINDINGS];
    Input_Button_Mapping button_mappings[MAX_NUM_BUTTON_MAPPINGS];
    Input_Axis_Mapping axis_mappings[MAX_NUM_AXIS_MAPPINGS];

    int num_input_bindings;
    int num_button_mappings;
    int num_axis_bindings;

    // Keyboard/mouse/gamepad button state
    uint8_t button_state[MAX_NUM_BUTTONS];

    // axes state
    float gamepad_axes[MAX_NUM_GAMEPAD_AXES];

    // Current mouse position
    Vec2 mouse_position;
    // Current mouse scroll
    Vec2 mouse_scroll;

    // True if gamepad currently has focus, i.e. keyboard or mouse
    // has not been touched since gamepad was used
    bool is_using_gamepad;
} Input_State;

Input_State *input_state = &(Input_State) { 
    .gamepad_axes = {
        0, 0, 0, 0, -1.f, -1.f,
    }
};

static void private__notify_input_method_changed(bool is_using_gamepad)
{
    log_info("Input method changed: using gamepad = %i", is_using_gamepad);
    input_state->is_using_gamepad = is_using_gamepad;
}

void input_feed_event(Input_Event_Type type, Input_Event data)
{   
    // Use window scale to scale mouse position correctly
    // Just so we don't have to handle everything in game code
    extern Window_Settings *window_settings;
    const float inv_window_scale = 1.f / window_settings->scale;

    if (type == INPUT_EVENT_GAMEPAD)
    {
        bool gamepad_touched = false;

        for (int idx = 0; idx < MAX_NUM_GAMEPAD_BUTTONS; ++idx)
        {
            const int offset = GAMEPAD_FACE_BTN_DOWN + idx;
            switch (data.gamepad.buttons[idx])
            {
            case INPUT_ACTION__PRESSED:
                input_state->button_state[offset] |= BUTTON_DOWN_BIT;
                input_state->button_state[offset] |= BUTTON_PRESSED_BIT;
                gamepad_touched = true;
                break;
            case INPUT_ACTION__RELEASED:
                // NOTE: Released is always true for gamepad...
                input_state->button_state[offset] &= ~BUTTON_DOWN_BIT;
                input_state->button_state[offset] |= BUTTON_RELEASED_BIT;
                break;
            }
        }

        for (int idx = 0; idx < MAX_NUM_GAMEPAD_AXES; ++idx)
        {
            if (fabs(data.gamepad.axes[idx] - input_state->gamepad_axes[idx]) > 0.05f)
            {
                gamepad_touched = true;
            }
            input_state->gamepad_axes[idx] = data.gamepad.axes[idx];
        }

        if (gamepad_touched && !input_state->is_using_gamepad)
        {
            private__notify_input_method_changed(true);
        }
    }
    else if (input_state->is_using_gamepad)
    {
        private__notify_input_method_changed(false);
    }

    if (type == INPUT_EVENT_BUTTON)
    {
        switch (data.button.action)
        {
        case INPUT_ACTION__PRESSED:
            input_state->button_state[data.button.code] |= BUTTON_DOWN_BIT;
            input_state->button_state[data.button.code] |= BUTTON_PRESSED_BIT;
            break;
        case INPUT_ACTION__RELEASED:
            input_state->button_state[data.button.code] &= ~BUTTON_DOWN_BIT;
            input_state->button_state[data.button.code] |= BUTTON_RELEASED_BIT;
            break;
        }
    }
    else if (type == INPUT_EVENT_MOUSE_MOVE)
    {
        input_state->mouse_position = (Vec2) { (float)data.mouse_move.pos_x * inv_window_scale, (float)data.mouse_move.pos_y * inv_window_scale };
    }
    else if (type == INPUT_EVENT_MOUSE_SCROLL)
    {
        input_state->mouse_scroll = (Vec2) { (float)data.mouse_scroll.scroll_x, (float)data.mouse_scroll.scroll_y };
    }
}

void input_clear()
{
    for (uint32_t idx = 0; idx < MAX_NUM_BUTTONS; ++idx)
    {
        input_state->button_state[idx] &= ~(BUTTON_PRESSED_BIT | BUTTON_RELEASED_BIT);
    }
    input_state->mouse_scroll = (Vec2) { 0.f, 0.f };
}

int input_add_binding(int binding_type)
{
    check(input_state->num_input_bindings < MAX_NUM_INPUT_BINDINGS);

    input_state->bindings[input_state->num_input_bindings++] = (Input_Binding) {
        .indices = 0,
        .count = 0,
        .binding_type = binding_type,
    };
    return input_state->num_input_bindings - 1;
}

bool input_add_axis_mapping(int id, int axis, float scale, float bias)
{
    check(id >= 0 && id < input_state->num_input_bindings);
    Input_Binding *binding = &input_state->bindings[id];
    
    check(binding->binding_type == INPUT_BINDING_TYPE__AXIS);
    check(binding->count < MAX_NUM_INPUT_MAPPINGS_PER_BINDING);
    check(input_state->num_axis_bindings < MAX_NUM_AXIS_MAPPINGS);

    int index = input_state->num_axis_bindings;
    binding->indices[binding->count++] = index;

    input_state->axis_mappings[index] = (Input_Axis_Mapping) {
        .axis = axis,
        .scale = scale,
        .bias = bias,
    };

    ++input_state->num_axis_bindings;
    return true;
}

bool input_add_button_mapping(int id, int button)
{
    check(id >= 0 && id < input_state->num_input_bindings);
    Input_Binding *binding = &input_state->bindings[id];
    
    check(binding->binding_type == INPUT_BINDING_TYPE__BUTTON);
    check(binding->count < MAX_NUM_INPUT_MAPPINGS_PER_BINDING);
    check(input_state->num_button_mappings < MAX_NUM_BUTTON_MAPPINGS);
    check(button >= 0 && button < MAX_NUM_BUTTONS);

    int index = input_state->num_button_mappings;
    binding->indices[binding->count++] = index;

    input_state->button_mappings[index] = (Input_Button_Mapping) {
        .button = button,
    };

    ++input_state->num_button_mappings;
    return true;
}

float input_get_axis(int id)
{
    const Input_Binding *binding = &input_state->bindings[id];
    if (binding->binding_type != INPUT_BINDING_TYPE__AXIS)
    {
        // Binding is not an axis binding
        return 0.0f;
    }

    check(binding->binding_type == INPUT_BINDING_TYPE__AXIS);

    // TODO: Allow mouse move / mouse scroll as virtual axes
    
    float sum = 0;
    for (int idx = 0; idx < binding->count; ++idx)
    {
        const Input_Axis_Mapping *map = &input_state->axis_mappings[binding->indices[idx]];
        if(map->axis >= 0 && map->axis < MAX_NUM_GAMEPAD_AXES)
        {
            sum += input_state->gamepad_axes[map->axis] * map->scale + map->bias;
        }
        else if (map->axis < MAX_NUM_BUTTONS)
        {
            sum += ((input_state->button_state[map->axis] & BUTTON_DOWN_BIT) ? 1.f : 0.f) * map->scale + map->bias;
        }
    }

    // Clamp output
    if (sum > 1.0f) sum = 1.0f;
    if (sum < -1.0f) sum = -1.0f;
    
    return sum;
}

static inline bool private__get_button_state(int id, int button_bit)
{
    const Input_Binding *binding = &input_state->bindings[id];
    if (binding->binding_type != INPUT_BINDING_TYPE__BUTTON)
    {
        // Binding is not an action binding
        return false;
    }

    bool result = false;
    for (int idx = 0; idx < binding->count; ++idx)
    {
        const Input_Button_Mapping *map = &input_state->button_mappings[binding->indices[idx]];
        result |= (input_state->button_state[map->button] & button_bit);
    }
    return result;
}

bool input_button_down(int id)
{
    return private__get_button_state(id, BUTTON_DOWN_BIT);
}

bool input_button_pressed(int id)
{
    return private__get_button_state(id, BUTTON_PRESSED_BIT);
}

bool input_button_released(int id)
{
    return private__get_button_state(id, BUTTON_RELEASED_BIT);
}

Vec2 input_mouse_position()
{
    return input_state->mouse_position;
}

Vec2 input_mouse_scroll()
{
    return input_state->mouse_scroll;
}

bool input_is_using_gamepad()
{
    return input_state->is_using_gamepad;
}
