local player = {
    x = 0,
    y = 0,
    vx = 0,
    vy = 0,
    angle = 0,
    thrust = 0,
    spritesheet = {},
    v_dir = 1,
    time_since_last_fire = 0
}

time = 0

function init()
    print("Hello, Space Game!")

    player.x = screen_width / 2
    player.y = screen_height / 2
    player.spritesheet = gfx.load_sprite_sheet('data/assets/player_ship.spritesheet')

    -- Define input bindings
    left_button = input.add_input_binding('action')
    right_button = input.add_input_binding('action')
    up_button = input.add_input_binding('action')
    down_button = input.add_input_binding('action')
    fire_button = input.add_input_binding('action')
    fire_trigger = input.add_input_binding('axis')
    rotate_axis = input.add_input_binding('axis')
    thrust_axis = input.add_input_binding('axis')

    -- Create mappings
    input.add_button_input_mapping(left_button, 'a')
    input.add_button_input_mapping(left_button, 'left')
    input.add_button_input_mapping(right_button, 'd')
    input.add_button_input_mapping(right_button, 'right')
    input.add_button_input_mapping(up_button, 'w')
    input.add_button_input_mapping(up_button, 'up')
    input.add_button_input_mapping(down_button, 's')
    input.add_button_input_mapping(down_button, 'down')
    input.add_button_input_mapping(fire_button, 'space')
    input.add_button_input_mapping(fire_button, 'f')

    input.add_axis_input_mapping(thrust_axis, 1, -1.0, 0.0) -- GAMEPAD_LEFT_STICK_Y
    input.add_axis_input_mapping(rotate_axis, 2, 1.0, 0.0)  -- GAMEPAD_RIGHT_STICK_X
    input.add_axis_input_mapping(fire_trigger, 5, 1.0, 0.0) -- GAMEPAD_RIGHT_TRIGGER
end

function apply_deadzone(value)
    local deadzone = 0.15
    if math.abs(value) < deadzone then
        return 0
    end
    return value
end

function update(dt)
    time = time + dt

    local turn_speed = 4.0
    if input.button(left_button) then
        player.angle = player.angle - turn_speed * dt
    end

    if input.button(right_button) then
        player.angle = player.angle + turn_speed * dt
    end

    local gamepad_rotate = apply_deadzone(input.axis(rotate_axis))
    player.angle = player.angle + gamepad_rotate * turn_speed * dt

    local thrust = 0
    if input.button(up_button) then
        thrust = 1.0
    elseif input.button(down_button) then
        thrust = -1.0
    end
    thrust = thrust + apply_deadzone(input.axis(thrust_axis)) * 100
    if thrust > 1.0 then thrust = 1.0 end
    if thrust < -0.5 then thrust = -0.5 end
    player.thrust = thrust

    if input.button(fire_button) or input.axis(fire_trigger) > 0.5 then
        -- TODO: implement:)
        if player.time_since_last_fire > 0.2 then
            print('Fire!')
            player.time_since_last_fire = 0
        end
    end
    player.time_since_last_fire = player.time_since_last_fire + dt

    local accel = 300
    local max_speed = 300
    local fx = math.sin(player.angle)
    local fy = -math.cos(player.angle)

    player.vx = player.vx + fx * thrust * accel * dt
    player.vy = player.vy + fy * thrust * accel * dt

    local speed = math.sqrt(player.vx * player.vx + player.vy * player.vy)

    if speed > max_speed then
        local scale = max_speed / speed
        player.vx = player.vx * scale
        player.vy = player.vy * scale
    end

    local drag = 2.0
    local damping = math.exp(-drag * dt)

    player.vx = player.vx * damping
    player.vy = player.vy * damping

    player.x = player.x + player.vx * dt
    player.y = player.y + player.vy * dt

    local area_w = screen_width
    local area_h = screen_height

    if player.x < 0 then player.x = area_w
    elseif player.x > area_w then player.x = 0 end

    if player.y < 0 then player.y = area_h
    elseif player.y > area_h then player.y = 0 end

    local side = player.vx * fy + player.vy * -fx
    local deadzone = 40
    if side > deadzone then
        player.v_dir = 2  -- lean right
    elseif side < -deadzone then
        player.v_dir = 0  -- lean left
    else
        player.v_dir = 1  -- center
    end
end

function draw()
    if player.thrust > 0 then
        local booster_offset = 15
        local booster_frame = math.floor(time * 12) % 2
        local booster_x = player.x - math.sin(player.angle) * booster_offset
        local booster_y = player.y + math.cos(player.angle) * booster_offset
        gfx.draw_sprite_sheet(player.spritesheet, {player.v_dir, 1 + booster_frame}, booster_x, booster_y, player.angle, 1, 1, 8, 8)
    end
    gfx.draw_sprite_sheet(player.spritesheet, {player.v_dir, 0}, player.x, player.y, player.angle, 1, 1, 8, 8)
end