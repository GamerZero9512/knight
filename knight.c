#include "knight.h"
#include <rlgl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ==================== PRIVATE FUNCTIONS ==================== */

static inline kn_vec3 rotateX(kn_vec3 p, float a) {
  float c = cosf(a), s = sinf(a);
  return (kn_vec3){ p.x, p.y*c - p.z*s, p.y*s + p.z*c };
}

static inline kn_vec3 rotateY(kn_vec3 p, float a) {
  float c = cosf(a), s = sinf(a);
  return (kn_vec3){ p.x*c + p.z*s, p.y, -p.x*s + p.z*c };
}

static inline kn_vec3 rotateZ(kn_vec3 p, float a) {
  float c = cosf(a), s = sinf(a);
  return (kn_vec3){ p.x*c - p.y*s, p.x*s + p.y*c, p.z };
}

static kn_point normalise(kn_point v) {
  float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  if(len < 1e-8f) return (kn_point){0, 0, 0};
  return (kn_point){ v.x / len, v.y / len, v.z / len };
}

static kn_point faceNormal(const kn_triangle *t) {
  kn_point e1 = { t->c.pos.x - t->a.pos.x, t->c.pos.y - t->a.pos.y, t->c.pos.z - t->a.pos.z };
  kn_point e2 = { t->b.pos.x - t->a.pos.x, t->b.pos.y - t->a.pos.y, t->b.pos.z - t->a.pos.z };

  return normalise((kn_point){
    e1.y * e2.z - e1.z * e2.y,
    e1.z * e2.x - e1.x * e2.z,
    e1.x * e2.y - e1.y * e2.x
  });
}

static void setFaceNormals(kn_triangle *tris, size_t count) {
  size_t i;
  for(i = 0; i < count; i++) {
    kn_point n = faceNormal(&tris[i]);
    tris[i].a.normal = tris[i].b.normal = tris[i].c.normal = n;
  }
}

static kn_texture trackTexture(kn_context *kn, kn_texture tex) {
  if(tex.id == 0) return tex;

  if(kn->texCount == kn->texCap) {
    size_t cap = kn->texCap ? kn->texCap * 2 : 8;
    kn_texture *p = realloc(kn->textures, cap * sizeof *p);
    if(!p) {
      UnloadTexture(tex);
      return (kn_texture){0};
    }
    kn->textures = p;
    kn->texCap = cap;
  }
  kn->textures[kn->texCount++] = tex;
  return tex;
}

static unsigned char toByte(float v) {
  if(v < 0.0f) v = 0.0f;
  if(v > 1.0f) v = 1.0f;
  return (unsigned char)(v * 255.0f + 0.5f);
}

static int sphereVisible(kn_point c, float r) {
  float ty = tanf(KN_FOVY * 0.5f * DEG2RAD);
  float tx = ty * ((float)GetScreenWidth() / (float)GetScreenHeight());
  float nx = sqrtf(1.0f + tx * tx);
  float ny = sqrtf(1.0f + ty * ty);

  if(c.z + r < KN_NEAR) return 0;
  if(c.z - r > KN_FAR)  return 0;
  if((c.z * tx - c.x) / nx < -r) return 0;   /* right  */
  if((c.z * tx + c.x) / nx < -r) return 0;   /* left   */
  if((c.z * ty - c.y) / ny < -r) return 0;   /* top    */
  if((c.z * ty + c.y) / ny < -r) return 0;   /* bottom */
  return 1;
}

static kn_triangle *allocTris(kn_context *kn, size_t count) {
  kn_triangle *p;

  if(kn->ownedCount == kn->ownedCap) {
    size_t cap = kn->ownedCap ? kn->ownedCap * 2 : 8;
    void **np = realloc(kn->owned, cap * sizeof *np);
    if(!np) return NULL;
    kn->owned = np;
    kn->ownedCap = cap;
  }
  p = malloc(count * sizeof *p);
  if(!p) return NULL;
  kn->owned[kn->ownedCount++] = p;
  return p;
}

static kn_vertex spherePoint(kn_point c, float r, int i, int j, int rings, int segments) {
  float lat = PI / 2.0f - PI * (float)i / (float)rings;
  float lon = 2.0f * PI * (float)j / (float)segments;
  float cl = cosf(lat);
  kn_vec3 n = { cl * sinf(lon), sinf(lat), -cl * cosf(lon) };

  return (kn_vertex){
    { c.x + r * n.x, c.y + r * n.y, c.z + r * n.z },
    { (float)j / (float)segments, (float)i / (float)rings },
    n
  };
}

static kn_mesh fromTemplate(kn_context *kn, const kn_triangle *src, size_t count, kn_point centre, kn_vec3 size) {
  kn_triangle *dst = allocTris(kn, count);
  size_t i, j;

  if(!dst) return (kn_mesh){0};

  for(i = 0; i < count; i++) {
    kn_vertex *dv[3] = { &dst[i].a, &dst[i].b, &dst[i].c };
    const kn_vertex *sv[3] = { &src[i].a, &src[i].b, &src[i].c };

    for(j = 0; j < 3; j++) {
      *dv[j] = *sv[j];
      dv[j]->pos.x = sv[j]->pos.x * size.x + centre.x;
      dv[j]->pos.y = sv[j]->pos.y * size.y + centre.y;
      dv[j]->pos.z = sv[j]->pos.z * size.z + centre.z;
    }
  }
  setFaceNormals(dst, count);
  return kn_makeMesh(dst, count);
}

/* Stuff for parsing meshes */

static const char *skipBlanks(const char *s) {
  while(*s == ' ' || *s == '\t') s++;
  return s;
}

static const char *nextLine(const char *s) {
  while(*s && *s != '\n') s++;
  return *s ? s + 1 : s;
}

static int atEol(const char *s) {
  return *s == '\0' || *s == '\n' || *s == '\r';
}

static int isTag(const char *s, const char *tag) {
  size_t n = strlen(tag);
  return strncmp(s, tag, n) == 0 && (s[n] == ' ' || s[n] == '\t');
}

static const char *parseFaceVert(const char *s, long *v, long *vt, long *vn) {
  char *end;

  *v = strtol(s, &end, 10);
  if(end == s) return NULL;
  s = end;
  *vt = 0;
  *vn = 0;

  if(*s == '/') {
    s++;
    if(*s != '/') {
      *vt = strtol(s, &end, 10);
      s = end;
    }
    if(*s == '/') {
      s++;
      *vn = strtol(s, &end, 10);
      s = end;
    }
  }
  return s;
}

static int resolveIndex(long i, size_t count, size_t *out) {
  if(i > 0 && (size_t)i <= count)  { *out = (size_t)i - 1;     return 1; }
  if(i < 0 && (size_t)(-i) <= count) { *out = count - (size_t)(-i); return 1; }
  return 0;
}

static int makeFaceVertex(long vi, long ti, long ni, const float *pos, size_t np, const float *uvs, size_t nu, const float *nrm, size_t nn,
                          kn_vertex *out) {
  size_t p, t, k;
  float u = 0.0f, v = 0.0f;
  kn_vec3 n = {0, 0, 0};

  if(!resolveIndex(vi, np, &p)) return 0;
  if(ti != 0) {
    if(!resolveIndex(ti, nu, &t)) return 0;
    u = uvs[2 * t];
    v = 1.0f - uvs[2 * t + 1];
  }
  if(ni != 0) {
    if(!resolveIndex(ni, nn, &k)) return 0;
    n = normalise((kn_vec3){ nrm[3*k], nrm[3*k + 1], -nrm[3*k + 2] });   /* z negated like positions */
  }
  *out = (kn_vertex){ { pos[3*p], pos[3*p + 1], -pos[3*p + 2] }, { u, v }, n };
  return 1;
}

static void emitLitVertex(const kn_vertex *v, kn_point fallback, const kn_localLight *ll, int nLights, float ar, float ag, float ab) {
  kn_point n = v->normal;
  float r = ar, g = ag, b = ab;
  int j;

  if(n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) n = fallback;

  for(j = 0; j < nLights; j++) {
    float d = n.x * ll[j].toLight.x + n.y * ll[j].toLight.y + n.z * ll[j].toLight.z;
    if(d > 0.0f) {
      r += ll[j].r * d;
      g += ll[j].g * d;
      b += ll[j].b * d;
    }
  }
  rlColor4ub(toByte(r), toByte(g), toByte(b), 255);
  rlTexCoord2f(v->uv.x, v->uv.y);
  rlVertex3f(v->pos.x, v->pos.y, v->pos.z);
}

static void drawLit(kn_mesh mesh, kn_transform xf, kn_texture tex, const kn_light *lights, int nLights, kn_colour ambient) {
  kn_localLight ll[KN_MAX_LIGHTS];
  float ar = ambient.r / 255.0f, ag = ambient.g / 255.0f, ab = ambient.b / 255.0f;
  size_t i;
  int j;

  if(!sphereVisible(xf.position, mesh.radius)) return;
  if(nLights > KN_MAX_LIGHTS) nLights = KN_MAX_LIGHTS;

  for(j = 0; j < nLights; j++) {
    kn_point d = { -lights[j].dir.x, -lights[j].dir.y, -lights[j].dir.z };
    float k = lights[j].intensity / 255.0f;

    d = rotateZ(d, -xf.rotation.z);
    d = rotateX(d, -xf.rotation.x);
    d = rotateY(d, -xf.rotation.y);
    ll[j].toLight = d;
    ll[j].r = lights[j].colour.r * k;
    ll[j].g = lights[j].colour.g * k;
    ll[j].b = lights[j].colour.b * k;
  }

  rlPushMatrix();
  rlScalef(xf.scale.x, xf.scale.y, -xf.scale.z);
  rlTranslatef(xf.position.x, xf.position.y, xf.position.z);
  rlRotatef(xf.rotation.z * RAD2DEG, 0, 0, 1);
  rlRotatef(xf.rotation.x * RAD2DEG, 1, 0, 0);
  rlRotatef(xf.rotation.y * RAD2DEG, 0, 1, 0);

  rlSetTexture(tex.id);
  rlBegin(RL_TRIANGLES);
  for(i = 0; i < mesh.count; i++) {
    const kn_triangle *t = &mesh.tris[i];
    kn_point fn = faceNormal(t);

    emitLitVertex(&t->a, fn, ll, nLights, ar, ag, ab);
    emitLitVertex(&t->b, fn, ll, nLights, ar, ag, ab);
    emitLitVertex(&t->c, fn, ll, nLights, ar, ag, ab);
  }
  rlEnd();
  rlSetTexture(0);
  rlPopMatrix();
}

static void drawWireframe(kn_mesh mesh, kn_transform xf, kn_colour colour) {
  size_t i;

  if(!sphereVisible(xf.position, mesh.radius)) return;

  rlPushMatrix();
  rlScalef(xf.scale.x, xf.scale.y, -xf.scale.z);
  rlTranslatef(xf.position.x, xf.position.y, xf.position.z);
  rlRotatef(xf.rotation.z * RAD2DEG, 0, 0, 1);
  rlRotatef(xf.rotation.x * RAD2DEG, 1, 0, 0);
  rlRotatef(xf.rotation.y * RAD2DEG, 0, 1, 0);

  rlSetTexture(0);
  rlColor4ub(colour.r, colour.g, colour.b, colour.a);

  rlBegin(RL_LINES);

  for(i = 0; i < mesh.count; i++) {
    const kn_triangle *t = &mesh.tris[i];

    rlVertex3f(t->a.pos.x, t->a.pos.y, t->a.pos.z);
    rlVertex3f(t->b.pos.x, t->b.pos.y, t->b.pos.z);

    rlVertex3f(t->b.pos.x, t->b.pos.y, t->b.pos.z);
    rlVertex3f(t->c.pos.x, t->c.pos.y, t->c.pos.z);

    rlVertex3f(t->c.pos.x, t->c.pos.y, t->c.pos.z);
    rlVertex3f(t->a.pos.x, t->a.pos.y, t->a.pos.z);
  }

  rlEnd();
  rlSetTexture(0);
  rlPopMatrix();
}

/* ==================== PUBLIC FUNCTIONS ==================== */

void kn_initUncapped(kn_context *kn, int width, int height, const char *title) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(width, height, title);

  kn->camera            = (kn_camera){0};
  kn->camera.position   = (kn_point){0, 0, 0};
  kn->camera.target     = (kn_point){0, 0, -1};
  kn->camera.up         = (kn_point){0, 1, 0};
  kn->camera.fovy       = KN_FOVY;
  kn->camera.projection = CAMERA_PERSPECTIVE;

  kn->textures = NULL;
  kn->texCount = 0;
  kn->texCap = 0;

  kn->owned = NULL;
  kn->ownedCount = 0;
  kn->ownedCap = 0;

  kn->in3d = 0;

  kn_setLight(kn, (kn_point){0.4f, -0.8f, 0.6f}, 0.25f);

  kn->scenes = NULL;
  kn->sceneCount = 0;
  kn->sceneCap = 0;
}

void kn_init(kn_context *kn, int width, int height, const char *title) {
  kn_initUncapped(kn, width, height, title);
  SetTargetFPS(60);
}

void kn_close(kn_context *kn) {
  size_t i;

  for(i = 0; i < kn->texCount; i++) UnloadTexture(kn->textures[i]);
  free(kn->textures);
  kn->textures = NULL;
  kn->texCount = kn->texCap = 0;

  for(i = 0; i < kn->ownedCount; i++) free(kn->owned[i]);
  free(kn->owned);
  kn->owned = NULL;
  kn->ownedCount = kn->ownedCap = 0;

  for(i = 0; i < kn->sceneCount; i++) {
    free(kn->scenes[i]->objects);
    free(kn->scenes[i]);
  }
  free(kn->scenes);
  kn->scenes = NULL;
  kn->sceneCount = kn->sceneCap = 0;

  CloseWindow();
}

void kn_beginFrame(kn_context *kn) {
  BeginDrawing();
  ClearBackground(BLACK);
  BeginMode3D(kn->camera);
  kn->in3d = 1;
}

void kn_end3D(kn_context *kn) {
  if(kn->in3d) {
    EndMode3D();
    kn->in3d = 0;
  }
}

void kn_endFrame(kn_context *kn) {
  kn_end3D(kn);
  EndDrawing();
}

void kn_drawMesh(kn_context *kn, kn_mesh mesh, kn_transform xf, kn_texture tex) {
  kn_light l = { kn->lightDir, (kn_colour){255, 255, 255, 255}, 1.0f };
  unsigned char a = toByte(kn->ambient);

  drawLit(mesh, xf, tex, &l, 1, (kn_colour){a, a, a, 255});
}

kn_mesh kn_makeMesh(kn_triangle *tris, size_t count) {
  size_t i, j;
  float maxSq = 0.0f;

  for(i = 0; i < count; i++) {
    const kn_vertex *v[3] = { &tris[i].a, &tris[i].b, &tris[i].c };
    for(j = 0; j < 3; j++) {
      float d = v[j]->pos.x * v[j]->pos.x + v[j]->pos.y * v[j]->pos.y + v[j]->pos.z * v[j]->pos.z;
      if(d > maxSq) maxSq = d;
    }
  }
  return (kn_mesh){ tris, count, sqrtf(maxSq) };
}

#define KN_V(x, y, z, u, v) {{x, y, z}, {u, v}, {0, 0, 0}}

static const kn_triangle kn_cubeTris[] = {
  /* front */
  {KN_V(-.5f, .5f,-.5f,0,0), KN_V(-.5f,-.5f,-.5f,0,1), KN_V( .5f, .5f,-.5f,1,0)},
  {KN_V( .5f, .5f,-.5f,1,0), KN_V(-.5f,-.5f,-.5f,0,1), KN_V( .5f,-.5f,-.5f,1,1)},
  /* back */
  {KN_V( .5f, .5f, .5f,0,0), KN_V( .5f,-.5f, .5f,0,1), KN_V(-.5f, .5f, .5f,1,0)},
  {KN_V(-.5f, .5f, .5f,1,0), KN_V( .5f,-.5f, .5f,0,1), KN_V(-.5f,-.5f, .5f,1,1)},
  /* left */
  {KN_V(-.5f, .5f, .5f,0,0), KN_V(-.5f,-.5f, .5f,0,1), KN_V(-.5f, .5f,-.5f,1,0)},
  {KN_V(-.5f, .5f,-.5f,1,0), KN_V(-.5f,-.5f, .5f,0,1), KN_V(-.5f,-.5f,-.5f,1,1)},
  /* right */
  {KN_V( .5f, .5f,-.5f,0,0), KN_V( .5f,-.5f,-.5f,0,1), KN_V( .5f, .5f, .5f,1,0)},
  {KN_V( .5f, .5f, .5f,1,0), KN_V( .5f,-.5f,-.5f,0,1), KN_V( .5f,-.5f, .5f,1,1)},
  /* top */
  {KN_V(-.5f, .5f, .5f,0,0), KN_V(-.5f, .5f,-.5f,0,1), KN_V( .5f, .5f, .5f,1,0)},
  {KN_V( .5f, .5f, .5f,1,0), KN_V(-.5f, .5f,-.5f,0,1), KN_V( .5f, .5f,-.5f,1,1)},
  /* bottom */
  {KN_V(-.5f,-.5f,-.5f,0,0), KN_V(-.5f,-.5f, .5f,0,1), KN_V( .5f,-.5f,-.5f,1,0)},
  {KN_V( .5f,-.5f,-.5f,1,0), KN_V(-.5f,-.5f, .5f,0,1), KN_V( .5f,-.5f, .5f,1,1)},
};

static const kn_triangle kn_quadTris[] = {
  {KN_V(-.5f, .5f,0,0,0), KN_V(-.5f,-.5f,0,0,1), KN_V( .5f, .5f,0,1,0)},
  {KN_V( .5f, .5f,0,1,0), KN_V(-.5f,-.5f,0,0,1), KN_V( .5f,-.5f,0,1,1)},
};


kn_mesh kn_cubeMesh(kn_context *kn, kn_point centre, kn_vec3 size) {
  return fromTemplate(kn, kn_cubeTris, sizeof(kn_cubeTris) / sizeof(kn_cubeTris[0]), centre, size);
}

kn_mesh kn_quadMesh(kn_context *kn, kn_point centre, float width, float height) {
  return fromTemplate(kn, kn_quadTris, sizeof(kn_quadTris) / sizeof(kn_quadTris[0]), centre, (kn_vec3){ width, height, 1.0f });
}

kn_mesh kn_sphereMesh(kn_context *kn, kn_point centre, float radius, int rings, int segments) {
  kn_triangle *tris;
  size_t n = 0;
  int i, j;

  if(rings < 2) rings = 2;
  if(segments < 3) segments = 3;

  tris = allocTris(kn, (size_t)rings * (size_t)segments * 2);
  if(!tris) return (kn_mesh){0};

  for(i = 0; i < rings; i++) {
    for(j = 0; j < segments; j++) {
      kn_vertex tl = spherePoint(centre, radius, i,     j,     rings, segments);
      kn_vertex tr = spherePoint(centre, radius, i,     j + 1, rings, segments);
      kn_vertex bl = spherePoint(centre, radius, i + 1, j,     rings, segments);
      kn_vertex br = spherePoint(centre, radius, i + 1, j + 1, rings, segments);

      if(i != 0)         tris[n++] = (kn_triangle){ tl, bl, tr };
      if(i != rings - 1) tris[n++] = (kn_triangle){ tr, bl, br };
    }
  }
  return kn_makeMesh(tris, n);
}

kn_mesh kn_floorMesh(kn_context *kn, kn_point centre, float size, float repeat) {
  kn_triangle *t = allocTris(kn, 2);
  float h = size * 0.5f;
  float x0 = centre.x - h, x1 = centre.x + h;
  float z0 = centre.z - h, z1 = centre.z + h;
  float y = centre.y;

  if(!t) return (kn_mesh){0};

  t[0] = (kn_triangle){ KN_V(x0,y,z1,0,0),      KN_V(x0,y,z0,0,repeat),      KN_V(x1,y,z1,repeat,0) };
  t[1] = (kn_triangle){ KN_V(x1,y,z1,repeat,0), KN_V(x0,y,z0,0,repeat),      KN_V(x1,y,z0,repeat,repeat) };
  setFaceNormals(t, 2);
  return kn_makeMesh(t, 2);
}

kn_texture kn_loadTexture(kn_context *kn, const char *path) {
  return trackTexture(kn, LoadTexture(path));
}

int kn_shouldExit(void) {
  return WindowShouldClose();
}

void kn_drawFPS(int x, int y) {
  DrawFPS(x, y);
}

float kn_getTime(void) {
  return (float)GetTime();
}

float kn_getDelta(void) {
  return GetFrameTime();
}

int kn_getFPS(void) {
  return GetFPS();
}

kn_mesh kn_loadMesh(kn_context *kn, const char *path) {
  char *text = LoadFileText(path);
  const char *line;
  float *pos = NULL, *uvs = NULL, *nrm = NULL;
  kn_triangle *tris = NULL;
  size_t nv = 0, nt = 0, nvn = 0, ntri = 0;   /* totals from pass 1 */
  size_t np = 0, nu = 0, nnr = 0, n = 0;      /* running counts in pass 2 */
  kn_mesh result = {0};

  if(!text) return result;

  for(line = text; *line; line = nextLine(line)) {
    const char *s = skipBlanks(line);

    if(isTag(s, "v"))       nv++;
    else if(isTag(s, "vt")) nt++;
    else if(isTag(s, "vn")) nvn++;
    else if(isTag(s, "f")) {
      size_t k = 0;
      s++;
      for(;;) {
        long vi, ti, ni;
        s = skipBlanks(s);
        if(atEol(s)) break;
        s = parseFaceVert(s, &vi, &ti, &ni);
        if(!s) break;
        k++;
      }
      if(k >= 3) ntri += k - 2;
    }
  }

  if(ntri == 0) goto done;

  pos  = malloc((nv  ? nv  : 1) * 3 * sizeof *pos);
  uvs  = malloc((nt  ? nt  : 1) * 2 * sizeof *uvs);
  nrm  = malloc((nvn ? nvn : 1) * 3 * sizeof *nrm);
  tris = allocTris(kn, ntri);
  if(!pos || !uvs || !nrm || !tris) goto done;

  for(line = text; *line; line = nextLine(line)) {
    const char *s = skipBlanks(line);
    char *e;

    if(isTag(s, "v")) {
      pos[3*np]     = strtof(s + 1, &e);
      pos[3*np + 1] = strtof(e, &e);
      pos[3*np + 2] = strtof(e, &e);
      np++;
    } else if(isTag(s, "vt")) {
      uvs[2*nu]     = strtof(s + 2, &e);
      uvs[2*nu + 1] = strtof(e, &e);
      nu++;
    } else if(isTag(s, "vn")) {
      nrm[3*nnr]     = strtof(s + 2, &e);
      nrm[3*nnr + 1] = strtof(e, &e);
      nrm[3*nnr + 2] = strtof(e, &e);
      nnr++;
    } else if(isTag(s, "f")) {
      kn_vertex first = {0}, prev = {0}, cur = {0};
      size_t k = 0;

      s++;
      for(;;) {
        long vi, ti, ni;
        s = skipBlanks(s);
        if(atEol(s)) break;
        s = parseFaceVert(s, &vi, &ti, &ni);
        if(!s || !makeFaceVertex(vi, ti, ni, pos, np, uvs, nu, nrm, nnr, &cur)) {
          TraceLog(LOG_ERROR, "kn_loadMesh: bad face in '%s'", path);
          n = 0;
          goto done;
        }
        if(k == 0) first = cur;
        else if(k >= 2) tris[n++] = (kn_triangle){ first, prev, cur };
        prev = cur;
        k++;
      }
    }
  }

  if(n > 0) result = kn_makeMesh(tris, n);

done:
  free(pos);
  free(uvs);
  free(nrm);
  UnloadFileText(text);
  return result;
}

kn_texture kn_solidTexture(kn_context *kn, kn_colour col) {
  Image img = GenImageColor(1, 1, col);
  Texture2D tex = LoadTextureFromImage(img);

  UnloadImage(img);
  return trackTexture(kn, tex);
}

void kn_setLight(kn_context *kn, kn_point dir, float ambient) {
  if(ambient < 0.0f) ambient = 0.0f;
  if(ambient > 1.0f) ambient = 1.0f;
  kn->lightDir = normalise(dir);
  kn->ambient = ambient;
}

kn_scene *kn_createScene(kn_context *kn) {
  kn_scene *s = calloc(1, sizeof *s);

  if(!s) return NULL;

  if(kn->sceneCount == kn->sceneCap) {
    size_t cap = kn->sceneCap ? kn->sceneCap * 2 : 4;
    kn_scene **p = realloc(kn->scenes, cap * sizeof *p);
    if(!p) {
      free(s);
      return NULL;
    }
    kn->scenes = p;
    kn->sceneCap = cap;
  }
  kn->scenes[kn->sceneCount++] = s;

  s->background = (kn_colour){0, 0, 0, 255};
  s->ambient    = (kn_colour){64, 64, 64, 255};
  s->lights[0]  = (kn_light){ normalise((kn_vec3){0.4f, -0.8f, 0.6f}), (kn_colour){255, 255, 255, 255}, 1.0f };
  s->lightCount = 1;
  return s;
}

kn_object kn_addObject(kn_scene *s, kn_mesh mesh, kn_texture tex, kn_transform xf) {
  kn_sceneObject *o;

  if(s->count == s->cap) {
    size_t cap = s->cap ? s->cap * 2 : 16;
    kn_sceneObject *p = realloc(s->objects, cap * sizeof *p);
    if(!p) return (kn_object){ KN_NO_OBJECT };
    s->objects = p;
    s->cap = cap;
  }
  o = &s->objects[s->count];
  o->mesh = mesh;
  o->tex = tex;
  o->xf = xf;
  o->alive = 1;
  return (kn_object){ s->count++ };
}

void kn_setTransform(kn_scene *s, kn_object o, kn_transform xf) {
  if(o.index < s->count && s->objects[o.index].alive) s->objects[o.index].xf = xf;
}

void kn_removeObject(kn_scene *s, kn_object o) {
  if(o.index < s->count) s->objects[o.index].alive = 0;
}

void kn_renderScene(kn_context *kn, kn_scene *s) {
  size_t i;

  (void)kn;
  ClearBackground(s->background);
  rlDrawRenderBatchActive();

  for(i = 0; i < s->count; i++) {
    const kn_sceneObject *o = &s->objects[i];
    if(o->alive) drawLit(o->mesh, o->xf, o->tex, s->lights, s->lightCount, s->ambient);
  }
}

int kn_addLight(kn_scene *s, kn_light light) {
  if(s->lightCount >= KN_MAX_LIGHTS) return -1;
  light.dir = normalise(light.dir);
  s->lights[s->lightCount] = light;
  return s->lightCount++;
}

void kn_clearLights(kn_scene *s) {
  s->lightCount = 0;
}

void kn_setAmbient(kn_scene *s, kn_colour ambient) {
  s->ambient = ambient;
}

void kn_setBackground(kn_scene *s, kn_colour col) {
  s->background = col;
}

void kn_drawMeshWireframe(kn_context *kn, kn_mesh mesh, kn_transform xf, kn_colour colour) {
  (void)kn;
  drawWireframe(mesh, xf, colour);
}

void kn_renderSceneWireframe(kn_context *kn, kn_scene *s, kn_colour colour) {
  size_t i;

  (void)kn;

  ClearBackground(s->background);
  rlDrawRenderBatchActive();

  for(i = 0; i < s->count; i++) {
    const kn_sceneObject *o = &s->objects[i];

    if(o->alive)
      drawWireframe(o->mesh, o->xf, colour);
  }
}

void kn_scaleMesh(kn_mesh *mesh, float x, float y, float z) {
  size_t i;

  for(i = 0; i < mesh->count; i++) {
    kn_triangle *t = (kn_triangle *)&mesh->tris[i];

    t->a.pos.x *= x;
    t->a.pos.y *= y;
    t->a.pos.z *= z;

    t->b.pos.x *= x;
    t->b.pos.y *= y;
    t->b.pos.z *= z;

    t->c.pos.x *= x;
    t->c.pos.y *= y;
    t->c.pos.z *= z;
  }

  mesh->radius *= fmaxf(fabsf(x), fmaxf(fabsf(y), fabsf(z)));
  setFaceNormals(mesh->tris, mesh->count);
}
