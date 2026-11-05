local time = 0

function init()
end

function update(dt)
    time = time + dt
end

function draw()
    local r = (math.sin(time * 3) + 1) / 2
    local g = (math.sin(time * 3 + 2) + 1) / 2
    local b = (math.sin(time * 3 + 4) + 1) / 2

    local cx = screen_width / 2
    local cy = screen_height / 2

    gfx.sprite_color(r, g, b)
    gfx.draw_text(default_font, "Hello, World!", cx - 48, cy)
end
