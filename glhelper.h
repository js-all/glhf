#define DEBUG
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "vector.h"
#include "maps.h"

//? should scale be a camera property ?, like a bigger camera displaying thing smaller
// in that case GlhCamera should just include a GlhTransform property
struct GlhCamera {
    vec3 position;
    vec3 rotation;
    bool perspective;
    float fov;
    float zNear;
    float zFar;
};

struct GlhTransforms {
    vec3 scale;
    vec3 translation;
    vec3 rotation;
};
// mostly internal stuff, should not be touched directly
struct GlhMeshBufferData {
    GLuint vertexBuffer;
    GLuint normalBuffer;
    GLuint indexsBuffer;
    GLuint tcoordBuffer;
    GLuint VAO;
    int vertexCount;
};

struct GlhMesh {
    struct GlhMeshBufferData bufferData;
    Vector verticies;
    Vector normals;
    Vector indexes;
    Vector texCoords;
};

enum GlhObjectTypes {
    regular,
    text
};
// here type must be the first member, to allow to check type before knowing what struct it is
struct GlhObject {
    enum GlhObjectTypes type;
    struct GlhTransforms transforms;
    // reference here, to be able to use the same mesh on multiple objects
    struct GlhMesh *mesh;
    // reference heren, to be able to use the same program on multiple objects
    struct GlhProgram *program;
    mat4 cachedModelMatrix;
    GLuint texture;
};

struct GlhFont {
    GLuint texture;
    Map glyphsData;
};

struct GlhFontGLyphData {
    int x0;
    int y0;
    int x1;
    int y1;
    int x_off;
    int y_off;
    int advance;
};

struct GlhTextObject {
    enum GlhObjectTypes type;
    struct GlhFont *font;
    char* _text;
    vec4 color;
    struct GlhTransforms transforms;
    Vector verticies;
    Vector uvs;
    GLuint VAO;
    GLuint vertBuffer;
    GLuint uvsBuffer;
};

//! as of now, applications should only have a single context, and would probably break otherwise
struct GlhContext {
    GLFWwindow *window;
    struct GlhCamera camera;
    mat4 cachedViewMatrix;
    mat4 cachedProjectionMatrix;
    Vector children;
};

struct GlhProgram {
    Vector uniforms;
    Vector uniformsLocation;
    Vector attributes;
    GLuint shaderProgram; 
    // a function pointer for a function setting the uniforms to their correct values.
    // it is not directly implemented in the helper as no shaders are provided by default
    // and as such should be dealt with by the user
    void (*setGlobalUniforms)(struct GlhObject*, struct GlhContext*);
};

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)());
void GlhFreeProgram(struct GlhProgram *prg);
// initialize context, windowWidth and windowHeight can be 0, windowTitle can be NULL
void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle);
void GlhFreeContext(struct GlhContext *ctx);
// add GlhObject child to context.
void GlhContextAppendChild(struct GlhContext *ctx, struct GlhObject *child);
// compute camera's view matrix, you most likely want to update it every frame
void GlhComputeContextViewMatrix(struct GlhContext *ctx);
// compute camera's projection matrix, does not need to be recomputed regularly unless specifics changes a made to the camera (fov, not transforms)
void GlhComputeContextProjectionMatrix(struct GlhContext *ctx);
// draw every object to the screen
void GlhRenderContext(struct GlhContext *ctx);
void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount);
// generates buffers for mesh, called internally, should not be called explicitly in most cases.
void GlhGenerateMeshBuffers(struct GlhMesh *mesh);
void GlhFreeMesh(struct GlhMesh *mesh);
// render an object, called internaly and doesn't do any buffer swaping and such, should not be called explicitly in most cases
void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx);
void GlhInitObject(struct GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program);
// update object matrix, should be called after any change to the transforms of an object
void GlhUpdateObjectModelMatrix(struct GlhObject *obj);
void GlhFreeObject(struct GlhObject *obj);
// used to load and setup a texture from a file
int loadTexture(GLuint *texture, char* filename, bool alpha);
// initialize freetype, needs to be called before working with fonts, only once (or after freeing the last one)
void GlhInitFreeType();
// stops freetype should be called when every fonts has been initilized or at the end or execution
void GlhFreeFreeType();
void GlhInitFont(struct GlhFont *font, char* ttfFileName, int size, int glyphCount);