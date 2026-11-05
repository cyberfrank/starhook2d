#include "gl_util.h"
#include "glad.h"
#include "foundation/log.h"

static gfx_id compile_shader(GLenum stage, const char *source, uint32_t length)
{
    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        if (log_len > 0)
        {
            GLchar log_buf[512];
            glGetShaderInfoLog(shader, 512, &log_len, log_buf);
            log_error("Shader %i compiled with errors:\n%s", shader, log_buf);
        }
        glDeleteShader(shader);
        shader = 0;
    }
    return shader;
}

gfx_id create_shader_program(const char *vs_source, const char *fs_source)
{
    GLuint program = glCreateProgram();
    bool result = true;

    if (vs_source)
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_source, (uint32_t)strlen(vs_source));
        glAttachShader(program, vs);
        result &= (vs != 0);
        glDeleteShader(vs);
    }

    if (fs_source)
    {
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_source, (uint32_t)strlen(fs_source));
        glAttachShader(program, fs);
        result &= (fs != 0);
        glDeleteShader(fs);
    }

    if (!result)
    {
        log_error("Unable to compile shader program %i", program);
        glDeleteProgram(program);
        return 0;
    }

    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status)
    {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        if (log_len > 0)
        {
            GLchar log_buf[512];
            glGetProgramInfoLog(program, 512, &log_len, log_buf);
            log_error("Program %i linked with errors:\n%s", program, log_buf);
        }
    }

    return (gfx_id)program;
}

gfx_id create_texture_2d(int width, int height, const void *data)
{
    const uint32_t target = GL_TEXTURE_2D;
    const uint32_t format = GL_RGBA;
    const uint32_t internal_format = GL_RGBA8;
    const uint32_t type = GL_UNSIGNED_BYTE;
    const uint32_t min_filter = GL_NEAREST_MIPMAP_NEAREST;
    const uint32_t mag_filter = GL_NEAREST;
    const uint32_t wrap_s = GL_CLAMP_TO_EDGE;
    const uint32_t wrap_t = GL_CLAMP_TO_EDGE;
    const uint32_t num_mips = 4;

    gfx_id id = 0;
    glGenTextures(1, &id);
    glBindTexture(target, id);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_t);

    glTexImage2D(target, 0, internal_format, width, height, 0, format, type, data);

    if (num_mips > 0)
    {
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, num_mips);
        glGenerateMipmap(target);
    }

    return id;
}

void destroy_texture(gfx_id id)
{
    glDeleteTextures(1, &id);
}