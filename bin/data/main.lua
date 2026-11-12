local NUM_GLYPHS = 5555
local glyphs = {}

function init()
    for i = 1, NUM_GLYPHS do
        local tile = math.random(1, 94)

        glyphs[i] = {
            cell = { tile % 32, tile // 32 },
            x = math.random() * screen_width,
            y = math.random() * screen_height,
            vx = math.random(-60, 60),
            vy = math.random(-60, 60),
            angle = math.random() * math.pi * 2,
            spin = math.random() * 4 - 2,
            r = math.random(),
            g = math.random(),
            b = math.random(),
        }
    end
end

function update(dt)
    for _, glyph in ipairs(glyphs) do
        glyph.x = (glyph.x + glyph.vx * dt) % screen_width
        glyph.y = (glyph.y + glyph.vy * dt) % screen_height
        glyph.angle = glyph.angle + glyph.spin * dt
    end
end

function draw()
    for _, glyph in ipairs(glyphs) do
        gfx.sprite_color(glyph.r, glyph.g, glyph.b)
        -- texture_id, cell, x, y, rotation, scale_x, scale_y, origin_x, origin_y
        gfx.draw_sprite_sheet(default_font, glyph.cell, glyph.x, glyph.y, glyph.angle, 1, 1, 3, 6)
    end
end
