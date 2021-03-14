#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifndef glClear
#include <GL/gl.h>
#endif
#define DEBUG
#include <cglm/cglm.h>
#include "glhelper.h"

struct GlhContext ctx;

void setUniforms(struct GlhObject *obj, struct GlhContext *ctx) {
    mat4 mvp, mv;
    glm_mat4_mul(ctx->cachedViewMatrix, obj->cachedModelMatrix, mv);
    glm_mat4_mul(ctx->cachedProjectionMatrix, mv, mvp);
    glUniformMatrix4fv(vector_get(obj->program->uniformsLocation.data, 0, GLint), 1, GL_FALSE,(float*) mvp);
}

void handleDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) { 
    char* sev;
    char* typ;
    if(severity & GL_DEBUG_SEVERITY_HIGH) sev = "\033[33m[HIGH]\033[0m";
    else if(severity & GL_DEBUG_SEVERITY_MEDIUM) sev = "[MEDIUM]";
    else if(severity & GL_DEBUG_SEVERITY_LOW) sev = "[LOW]";
    else if(severity & GL_DEBUG_SEVERITY_NOTIFICATION) sev = "[NOTE]";
    if(type & GL_DEBUG_TYPE_ERROR) typ = "\033[33m[ERROR]\033[0m";
    else if(type & GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) typ = "\033[31m[DEPRECATED]\033[0m";
    else if(type & GL_DEBUG_TYPE_MARKER) typ = "[MARKER]";
    else if(type & GL_DEBUG_TYPE_PERFORMANCE) typ = "[PERFORMANCE]";
    else if(type & GL_DEBUG_TYPE_POP_GROUP) typ = "[POPGRP]";
    else if(type & GL_DEBUG_TYPE_PORTABILITY) typ = "[PORTABILITY]";
    else if(type & GL_DEBUG_TYPE_PUSH_GROUP) typ = "[PSHGRP]";
    else if(type & GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) typ = "[UNDEFINED_BEHAVIOUR]";
    printf("\033[32m[GLDEBUG]\033[0m%s%s: %s\n", sev, typ, message);
}

int main() {
    
    if(!glfwInit()) {
        printf("glfw did not initialize\n");
        return -1;
    }

    GlhInitContext(&ctx, 640, 480, "nothing here");

    struct GlhMesh mesh;
    // fill mesh
    {
        vec3 verticies[] = {
            {-1, -1, -1},
            {-1, 1, -1},
            {1, 1, -1},
            {1, -1, -1},
            {-1, -1, 1},
            {-1, 1, 1},
            {1, 1, 1},
            {1, -1, 1}
        };
        vec3 normals[] = {
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1}
        };
        vec3 indices[] = {
            {0, 1, 2}, {2, 3, 0}, {5, 1, 2}, {2, 6, 5}, {3, 2, 6}, {6, 7, 3}, {4, 0, 3}, {3, 7, 4}, {0, 1, 5}, {5, 4, 0}, {4, 5, 6}, {6, 7, 4}
        };
        vec2 texcoords[] = {
            {0, 0},
            {0, 1},
            {1, 1},
            {1, 0},
            {0, 0},
            {0, 1},
            {1, 1},
            {1, 0}
        };
        GlhInitMesh(&mesh, verticies, sizeof(verticies) / sizeof(verticies[0]), normals, indices, sizeof(indices) / sizeof(indices[0]), texcoords, sizeof(texcoords) / sizeof(texcoords[0]));
        printf("\n");
    }
    char* uniforms[] = {
        "MVP"
    };
    GLuint tex;
    loadTexture(&tex, "images/texture.jpeg");
    printf("GL version: %s\n", glGetString(GL_VERSION));
    struct GlhProgram prg;
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(handleDebugMessage, NULL);
    GlhInitProgram(&prg, "shaders/shader.frag", "shaders/shader.vert", uniforms, 1, setUniforms);
    struct GlhObject obj;
    GlhInitObject(&obj,tex, GLM_VEC3_ONE, GLM_VEC3_ZERO, GLM_VEC3_ZERO, &mesh, &prg);
    obj.transforms.translation[2] = -3;
    GlhContextAppendChild(&ctx, &obj);

    GlhComputeContextProjectionMatrix(&ctx);
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    while(!(glfwWindowShouldClose(ctx.window))) {
        double time = glfwGetTime();
        obj.transforms.rotation[2] = -time;
        obj.transforms.rotation[1] = time;
        GlhComputeContextViewMatrix(&ctx);
        GlhUpdateObjectModelMatrix(&obj);
        GlhRenderContext(&ctx);
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    GlhFreeMesh(&mesh);
    GlhFreeObject(&obj);
    GlhFreeProgram(&prg);
    GlhFreeContext(&ctx);
    glfwTerminate();

    return 0;
}