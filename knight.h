#ifndef KNIGHT_H
#define KNIGHT_H

#include <raylib.h>
#include <stddef.h>

#ifndef KN_MAX_LIGHTS
#define KN_MAX_LIGHTS 8
#endif
#define KN_NO_OBJECT ((size_t)-1)
#define KN_FOVY 90.0f
#define KN_NEAR 0.05f
#define KN_FAR  4000.0f

typedef Texture2D kn_texture;
typedef Vector3   kn_point;
typedef Vector3   kn_vec3;
typedef Vector2   kn_vec2;
typedef Camera3D  kn_camera;
typedef Color     kn_colour;

typedef struct {
  kn_point pos;
  kn_vec2 uv;
  kn_vec3 normal;
} kn_vertex;

typedef struct {
  kn_vertex a, b, c;
} kn_triangle;

typedef struct {
  kn_triangle *tris;
  size_t count;
  float radius;
} kn_mesh;

typedef struct {
  kn_vec3 rotation;
  kn_vec3 position;
  kn_vec3 scale;
} kn_transform;

typedef struct {
  kn_vec3   dir;
  kn_colour colour;
  float     intensity;
} kn_light;

typedef struct {
  kn_mesh      mesh;
  kn_texture   tex;
  kn_transform xf;
  int          alive;
} kn_sceneObject;

typedef struct {
  kn_sceneObject *objects;
  size_t          count, cap;
  kn_light        lights[KN_MAX_LIGHTS];
  int             lightCount;
  kn_colour       ambient;
  kn_colour       background;
} kn_scene;

typedef struct {
  kn_camera camera;
  kn_texture *textures;
  size_t texCount, texCap;
  int in3d;
  void **owned;
  size_t ownedCount, ownedCap;
  kn_vec3 lightDir;
  float ambient;
  kn_scene **scenes;
  size_t sceneCount, sceneCap;
} kn_context;

typedef struct {
  size_t index;
} kn_object;

typedef struct {
  kn_point toLight;
  float r, g, b;
} kn_localLight;

void        kn_initUncapped(kn_context *kn, int width, int height, const char *title);
void        kn_init(kn_context *kn, int width, int height, const char *title);
void        kn_close(kn_context *kn);
void        kn_beginFrame(kn_context *kn);
void        kn_endFrame(kn_context *kn);
void        kn_drawMesh(kn_context *kn, kn_mesh mesh, kn_transform xf, kn_texture tex);
kn_texture  kn_loadTexture(kn_context *kn, const char *path);
int         kn_shouldExit(void);
void        kn_end3D(kn_context *kn);
void        kn_drawFPS(int x, int y);
float       kn_getTime(void);
float       kn_getDelta(void);
int         kn_getFPS(void);
kn_mesh     kn_makeMesh(kn_triangle *tris, size_t count);
kn_mesh     kn_cubeMesh(kn_context *kn, kn_point centre, kn_vec3 size);
kn_mesh     kn_quadMesh(kn_context *kn, kn_point centre, float width, float height);
kn_mesh     kn_sphereMesh(kn_context *kn, kn_point centre, float radius, int rings, int segments);
kn_mesh     kn_floorMesh(kn_context *kn, kn_point centre, float size, float repeat);
kn_mesh     kn_loadMesh(kn_context *kn, const char *path);
kn_texture  kn_solidTexture(kn_context *kn, kn_colour col);
void        kn_setLight(kn_context *kn, kn_point dir, float ambient);
kn_scene   *kn_createScene(kn_context *kn);
kn_object   kn_addObject(kn_scene *s, kn_mesh mesh, kn_texture tex, kn_transform xf);
void        kn_setTransform(kn_scene *s, kn_object o, kn_transform xf);
void        kn_removeObject(kn_scene *s, kn_object o);
void        kn_renderScene(kn_context *kn, kn_scene *s);
int         kn_addLight(kn_scene *s, kn_light light);
void        kn_clearLights(kn_scene *s);
void        kn_setAmbient(kn_scene *s, kn_colour ambient);
void        kn_setBackground(kn_scene *s, kn_colour col);
void        kn_drawMeshWireframe(kn_context *kn, kn_mesh mesh, kn_transform xf, kn_colour colour);
void        kn_renderSceneWireframe(kn_context *kn, kn_scene *s, kn_colour colour);
void        kn_scaleMesh(kn_mesh *mesh, float x, float y, float z);

#endif
