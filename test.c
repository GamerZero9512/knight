#include "knight.h"

int main(void) {
  kn_context kn;
  kn_texture grey, marble;
  kn_mesh sphere, cow;
  kn_scene *scene;
  kn_object osphere, ocow;

  kn_initUncapped(&kn, 800, 800, "Knight Demo");
  scene = kn_createScene(&kn);
  sphere = kn_sphereMesh(&kn, (kn_point){0, 0, 0}, 2, 16, 32);
  cow = kn_loadMesh(&kn, "objects/cow.obj");
  grey = kn_solidTexture(&kn, GRAY);
  marble = kn_loadTexture(&kn, "images/blue_marble.png");

  kn_setBackground(scene, (kn_colour){10, 0, 20, 255});

  osphere = kn_addObject(scene, sphere, marble, (kn_transform){{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});
  ocow = kn_addObject(scene, cow, grey, (kn_transform){{0, 0, 0}, {0, 0, 4}, {1, 1, 1}});

  while(!kn_shouldExit()) {
    float t = kn_getTime();

    kn_setTransform(scene, ocow, (kn_transform){{-0.3, -t, 0}, {0, 8.5, 20}, {1, 1, 1}});
    kn_setTransform(scene, osphere, (kn_transform){{-0.3, -t, 0}, {0, -1, 4}, {1, 1, 1}});

    kn_beginFrame(&kn);
    kn_renderScene(&kn, scene);
    kn_end3D(&kn);
    kn_drawFPS(10, 10);
    kn_endFrame(&kn);
  }

  kn_close(&kn);
  return 0;
}
