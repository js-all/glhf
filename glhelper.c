#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "glhelper.h"

static const char* attributes[] = {
    "vPos"/*,
    "vNormal",
    "vTexCoord"*/
};

int readFile(char* filename, int* size,char **content) {
	FILE* file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	long f_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	*content = malloc((f_size + 1) * sizeof(char));
	if(!*content) {
		printf("ERROR: readFile, unable to allocate memory");
		return -1;
	}
	fread(*content, f_size, 1, file);
	fclose(file);
    (*content)[f_size] = '\0';
	*size = f_size;
    printf("read file %s, content:\n\033[36m%s\033[0m\n", filename, *content);
	return 0;
}

int loadShader(char* filename, int type, GLuint *shader) {    
    char* source;
    int size;
    if(readFile(filename, &size, &source) != 0) {
        return -1;
    }; 
    const char* shader_source = source;
    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &shader_source, NULL);
    glCompileShader(*shader);
    GLint status;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    printf("compiled shader %s, logs: %i (GL_TRUE: %i)\n", filename, status, GL_TRUE);
    free(source);
    return 0;
}

int initProgram(char* fragSourceFilename, char* vertSourceFilename, GLuint *program) {
    GLuint vert;
    GLuint frag;
    loadShader(vertSourceFilename, GL_VERTEX_SHADER, &vert);
    loadShader(fragSourceFilename, GL_FRAGMENT_SHADER, &frag);
    glAttachShader(*program, vert);
    glAttachShader(*program, frag);
    GLint validated;
    GLint status;
    GLenum error;
    int logsLength;
    glLinkProgram(*program);
    error = glGetError();
    glGetProgramiv(*program, GL_LINK_STATUS, &status);
    glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &logsLength);
    char logs[logsLength+1];
    glGetProgramInfoLog(*program, logsLength, NULL, logs);
    glValidateProgram(*program);
    glGetProgramiv(*program, GL_VALIDATE_STATUS, &validated);
    printf("linking status: %i, validated: %i, error: %i, logs: [%i] \n%s\n", status, validated, error, logsLength, logs);
    return 0;
}

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)()) {
    int attributesCount = sizeof(attributes) / sizeof(attributes[0]);
    prg->shaderProgram = glCreateProgram();

    for(int i = 0; i < attributesCount; i++) {
        glBindAttribLocation(prg->shaderProgram, i, attributes[i]);
    }

    initProgram(fragSourceFilename, vertSourceFilename, &prg->shaderProgram);
    vector_init(&prg->uniforms, uniformsCount, sizeof(char*));
    vector_init(&prg->uniformsLocation, uniformsCount, sizeof(GLint));
    vector_init(&prg->attributes, attributesCount, sizeof(char*));

    vector_push_array(prg->uniforms, uniforms, uniformsCount);
    vector_push_array(prg->attributes, attributes, attributesCount);

    for(int i = 0; i < uniformsCount; i++) {
        GLint v = glGetUniformLocation(prg->shaderProgram, vector_get(prg->uniforms.data, i, char*));
        vector_push(&prg->uniformsLocation, &v);
    }

    printf("\n");
    prg->setGlobalUniforms = setUniforms;
}

void GlhFreeProgram(struct GlhProgram *prg) {
    vector_free(&prg->attributes);
    vector_free(&prg->uniforms);
    vector_free(&prg->uniformsLocation);
}

void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ctx->window = glfwCreateWindow(OPT(windowWidth, 640, 0), OPT(windowHeight, 480, 0), OPT(windowTitle, "window", NULL), NULL, NULL);
    glfwMakeContextCurrent(ctx->window);
    glewInit();
    glfwSwapInterval(1);
    ctx->camera.fov = glm_rad(90);
    glm_vec3_zero(ctx->camera.position);
    glm_vec3_zero(ctx->camera.rotation);
    ctx->camera.zNear = 0.1;
    ctx->camera.zFar = 100;
    vector_init(&ctx->children, 2, sizeof(void*));
}

void GlhFreeContext(struct GlhContext *ctx) {
    vector_free(&ctx->children);
}

void GlhContextAppendChild(struct GlhContext *ctx, struct GlhObject *child) {
    vector_push(&ctx->children, &child);
}

void GlhComputeContextViewMatrix(struct GlhContext *ctx) {
    vec3 translation;
    vec3 rotation;
    glm_vec3_negate_to(ctx->camera.position, translation);
    glm_vec3_negate_to(ctx->camera.rotation, rotation);
    glm_mat4_identity(ctx->cachedViewMatrix);
    glm_translate(ctx->cachedViewMatrix, translation);
    glm_rotate_x(ctx->cachedViewMatrix, rotation[0], ctx->cachedViewMatrix);
    glm_rotate_y(ctx->cachedViewMatrix, rotation[1], ctx->cachedViewMatrix);
    glm_rotate_z(ctx->cachedViewMatrix, rotation[2], ctx->cachedViewMatrix);
}

void GlhComputeContextProjectionMatrix(struct GlhContext *ctx) { 
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    float aspect = (float) width / (float) height;
    float f = tanf(M_PI * 0.5 - 0.5 * glm_rad(90));
    float fn = 1.0f / (ctx->camera.zNear - ctx->camera.zFar);
    // mat4 p = {
    //     {f / aspect, 0, 0, 0},
    //     {0, f, 0, 0},
    //     {0, 0, (ctx->camera.zNear + ctx->camera.zFar) * fn, -1},
    //     {0, 0, ctx->camera.zNear * ctx->camera.zFar * fn * 2, 0}
    // };
    mat4 p = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 1}
    };
    glm_mat4_identity(ctx->cachedProjectionMatrix);
    glm_mat4_mul(ctx->cachedProjectionMatrix, p, ctx->cachedProjectionMatrix);
}

void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx) {
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    glUseProgram(obj->program->shaderProgram);
    printf("glUseProgram, error: %i\n", glGetError());
    (*obj->program->setGlobalUniforms)(obj, ctx);
    glViewport(0, 0, width, height);
    glBindVertexArray(obj->mesh->bufferData.VAO);
    printf("glBindVertexArray, error: %i\n", glGetError());
    GLint validated;
    glValidateProgram(obj->program->shaderProgram);
    glGetProgramiv(obj->program->shaderProgram, GL_VALIDATE_STATUS, &validated);
    printf("shader program validation: %i\n", validated);
    glDrawElements(GL_TRIANGLES, obj->mesh->bufferData.vertexCount, GL_UNSIGNED_INT, NULL);
    printf("glDrawElements, error: %i\n", glGetError());
}

void GlhRenderContext(struct GlhContext *ctx) {
    glClear(GL_COLOR_BUFFER_BIT);
    for(int i = 0; i < ctx->children.size; i++) {
        struct GlhObject* obj = vector_get(ctx->children.data, i, struct GlhObject*);
        GlhRenderObject(obj, ctx);
    }
}

void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount) {
    struct GlhMeshBufferData data = {};
    mesh->bufferData = data;
    vector_init(&mesh->verticies, verticiesCount, sizeof(vec3));
    vector_init(&mesh->normals, verticiesCount, sizeof(vec3));
    vector_init(&mesh->indexes, indicesCount, sizeof(vec3));
    vector_init(&mesh->texCoords, texcoordsCount, sizeof(vec2));
    vector_push_array(mesh->verticies, verticies, verticiesCount);
    vector_push_array(mesh->normals, normals, verticiesCount);
    vector_push_array(mesh->indexes, indices, indicesCount);
    vector_push_array(mesh->texCoords, texcoords, texcoordsCount);
    GlhGenerateMeshBuffers(mesh);
    glGenVertexArrays(1, &mesh->bufferData.VAO);
    printf("glGenVertexArray, error: %i\n", glGetError());
    glBindVertexArray(mesh->bufferData.VAO);
    #define attrib(type, buffer, loc, comp) \
    glBindBuffer(GL_ARRAY_BUFFER, buffer); \
    glVertexAttribPointer(loc, comp, GL_FLOAT, GL_FALSE, 0, 0); \
    printf("glVertexAttribPointer: %i (no error = %i)\n", glGetError(), GL_NO_ERROR); \
    glEnableVertexAttribArray(loc); \
    printf("glEnableVertexAttribeArray: %i (no error = %i)\n", glGetError(), GL_NO_ERROR)

    attrib(GL_FLOAT, mesh->bufferData.vertexBuffer, 0, 3);
    // attrib(GL_FLOAT, obj->mesh->bufferData.normalBuffer, 1, 3);
    // attrib(GL_FLOAT, obj->mesh->bufferData.tcoordBuffer, 2, 2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->bufferData.indexsBuffer);
    printf("glBindBuffer: %i\n", glGetError());
    #undef attrib
}

void GlhFreeMesh(struct GlhMesh *mesh) {
    vector_free(&mesh->verticies);
    vector_free(&mesh->normals);
    vector_free(&mesh->indexes);
    vector_free(&mesh->texCoords);
}

void GlhInitObject(struct GlhObject *obj, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program) {
    struct GlhTransforms tsfm = {};
    glm_vec3_dup(scale, tsfm.scale);
    glm_vec3_dup(rotation, tsfm.rotation);
    glm_vec3_dup(translation, tsfm.translation);
    obj->transforms = tsfm;
    obj->mesh = mesh;
    obj->program = program;
}

void GlhUpdateObjectModelMatrix(struct GlhObject *obj) {
    glm_mat4_identity(obj->cachedModelMatrix);
    glm_rotate_x(obj->cachedModelMatrix, obj->transforms.rotation[0], obj->cachedModelMatrix);
    glm_rotate_y(obj->cachedModelMatrix, obj->transforms.rotation[1], obj->cachedModelMatrix);
    glm_rotate_z(obj->cachedModelMatrix, obj->transforms.rotation[2], obj->cachedModelMatrix);
    glm_scale(obj->cachedModelMatrix, obj->transforms.scale);
    glm_translate(obj->cachedModelMatrix, obj->transforms.translation);
}


void GlhGenerateMeshBuffers(struct GlhMesh *mesh) {
    #define gbuffer(buffer, btype, components, arrName, vector, t, t2, h) \
    glGenBuffers(1, &buffer); \
    glBindBuffer(btype, buffer); \
    t2 arrName[vector.size * components]; \
    for(int i = 0; i < (vector.size) * components; i += components) { \
        arrName[i] = (t2)(vector_get(vector.data, i / components, t)[0]); \
        arrName[i+1] = (t2)(vector_get(vector.data, i / components, t)[1]); \
        if(components > 2) \
            arrName[i+2] = (t2)(vector_get(vector.data, i / components, t)[2]); \
    } \
    glBufferData(btype, vector.size * sizeof(unsigned int), &arrName[0], GL_STATIC_DRAW); \
    printf(#arrName); \
    printf(": "); \
    for(int i = 0; i < sizeof(arrName) / sizeof(arrName[0]); i++) { \
        printf(h, arrName[i]); \
        if((float)((i+1) / components) - ((float) (i+1)) / (float)(components) == 0.0) { \
            printf(", "); \
        } \
    } \
    printf("\n");

    gbuffer(mesh->bufferData.vertexBuffer, GL_ARRAY_BUFFER, 3, verticies, mesh->verticies, vec3, float, " %f");
    gbuffer(mesh->bufferData.normalBuffer, GL_ARRAY_BUFFER, 3, normals, mesh->normals, vec3, float, " %f");
    gbuffer(mesh->bufferData.indexsBuffer, GL_ELEMENT_ARRAY_BUFFER, 3, indexes, mesh->indexes, vec3, int, " %i");
    gbuffer(mesh->bufferData.tcoordBuffer, GL_ARRAY_BUFFER, 2, texcoords, mesh->texCoords, vec2, float, " %f");


    mesh->bufferData.vertexCount = mesh->indexes.size * 3;
    #undef gbuffer
}

void GlhFreeObject(struct GlhObject *obj) {
    //GlhFreeMesh(obj->mesh);
}