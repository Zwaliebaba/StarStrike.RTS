#include "pch.h"
#include "config.h"
#include "gfx.h"
#include "elite.h"
#include "space.h"
#include "threed.h"
#include "random.h"
#include "shipface.h"
#include "OpenGLContext.h"
#include "GL/gl.h"

#include "GdiBitmap.h"

#define PI 3.1415926535898

#define FILLED_CIRCLE    1
#define WIREFRAME_CIRCLE 2

// GL_BGR_EXT may not be defined in older OpenGL headers
#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif

GLuint texture[20];
GLuint Elite_1_base;
GLuint Elite_2_base;

std::unique_ptr<Neuron::GdiBitmap> scanner_image;

int clip_tx;
int clip_ty;
int clip_bx;
int clip_by;

Matrix4 camera;

/*
 * Return the pixel value at (x, y) from a GdiBitmap
 */
uint32_t gfx_get_pixel(const Neuron::GdiBitmap* bmp, int x, int y)
{
  return Neuron::GdiBitmapLoader::GetPixel(bmp, x, y);
}

void gfx_get_char_size(const Neuron::GdiBitmap* bmp, int x, int y, int* size_x, int* size_y)
{
  *size_x = 0;
  *size_y = 0;

  // Auto-detect mask convention by sampling the corner pixel (should be background)
  uint32_t background = gfx_get_pixel(bmp, (x * 32), (y * 32));

  for (int dy = 0; dy < 32; dy++)
  {
    for (int dx = 0; dx < 32; dx++)
    {
      uint32_t pixel = gfx_get_pixel(bmp, (x * 32) + dx, (y * 32) + dy);

      // If this pixel differs from background, it's a character pixel
      if (pixel != background)
      {
        if (dx > *size_x) *size_x = dx;
        if (dy > *size_y) *size_y = dy;
      }
    }
  }

  // Add 1 to convert from max index to size, ensure minimum spacing
  (*size_x)++;
  (*size_y)++;
}

std::unique_ptr<Neuron::GdiBitmap> gfx_load_bitmap(const char* filename)
{
  auto fname = FileSys::GetHomeDirectoryA() + filename;
  auto bmap = Neuron::GdiBitmapLoader::LoadBMP(fname);
  if (!bmap)
  {
    Fatal("Failed to load bitmap '{}'", fname);
  }
  return bmap;
}

GLuint gfx_load_texture(const char* filename, int x, int y, int size)
{
  GLuint tex;

  auto bmp = gfx_load_bitmap(filename);
  if (!bmp)
  {
    Fatal("Error reading bitmap file: {}.\n", filename);
  }

  auto bmp1 = Neuron::GdiBitmapLoader::CreateRGB(size, size, 24);
  if (!bmp1)
  {
    Fatal("Error creating RGB surface for texture.\n");
  }

  // Blit the source region to the destination
  Neuron::GdiBitmapLoader::Blit(bmp.get(), x, y, size, size, bmp1.get(), 0, 0);

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, size, size, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, bmp1->pixels);

  return tex;
}

GLuint gfx_load_font(const char* filename, const char* maskname)
{
  int size_x, size_y;
  int cx;

  auto fontmask = gfx_load_bitmap(maskname);
  if (!fontmask)
  {
    Fatal("Error reading font bitmap file: {}.\n", maskname);
  }

  GLuint texture1 = gfx_load_texture(filename, 0, 0, 256);
  GLuint texture2 = gfx_load_texture(filename, 256, 0, 256);

  GLuint base = glGenLists(96);

  for (int y = 0; y < 6; y++)
  {
    for (int x = 0; x < 16; x++)
    {
      glNewList(base + (y * 16) + x, GL_COMPILE);
      if (x > 7)
      {
        glBindTexture(GL_TEXTURE_2D, texture2);
        cx = x - 8;
      }
      else
      {
        glBindTexture(GL_TEXTURE_2D, texture1);
        cx = x;
      }

      gfx_get_char_size(fontmask.get(), x, y, &size_x, &size_y);

      glBegin(GL_QUADS);
      glTexCoord2d(cx * 0.125, y * 0.125);
      glVertex2i(0, 0);
      glTexCoord2d((cx + 1) * 0.125, y * 0.125);
      glVertex2i(32, 0);
      glTexCoord2d((cx + 1) * 0.125, (y + 1) * 0.125);
      glVertex2i(32, 32);
      glTexCoord2d(cx * 0.125, (y + 1) * 0.125);
      glVertex2i(0, 32);
      glEnd();
      glTranslated(size_x, 0, 0);
      glEndList();
    }
  }

  return base;
}

void gfx_build_gl_circles(void)
{
  GLuint i;
  GLdouble cosine, sine;

  glNewList(FILLED_CIRCLE, GL_COMPILE);
  glBegin(GL_POLYGON);
  for (i = 0; i < 100; i++)
  {
    cosine = cos(i * 2 * PI / 100.0);
    sine = sin(i * 2 * PI / 100.0);
    glVertex2d(cosine, sine);
  }
  glEnd();
  glEndList();

  glNewList(WIREFRAME_CIRCLE, GL_COMPILE);
  glBegin(GL_LINE_LOOP);
  for (i = 0; i < 100; i++)
  {
    cosine = cos(i * 2 * PI / 100.0);
    sine = sin(i * 2 * PI / 100.0);
    glVertex2d(cosine, sine);
  }
  glEnd();
  glEndList();
}

int gfx_graphics_startup(void)
{
  // Initialize OpenGL context using Win32 WGL (replaces SDL video mode)
  if (!OpenGLContext::Startup(ClientEngine::Window()))
  {
    Fatal("Failed to initialize OpenGL context");
  }

  glClearColor(0, 0, 0, 1.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE);

  glCullFace(GL_BACK);
  glFrontFace(GL_CW);

  // Set initial viewport and projection using ClientEngine window size
  gfx_set_clip_region(0, 0, 512, 512);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  scanner_image = gfx_load_bitmap(scanner_filename);

  if (!scanner_image)
  {
    Fatal("Error reading scanner bitmap file: {}.\n", scanner_filename);
  }

  texture[IMG_SCANNER_1] = gfx_load_texture("scanner.bmp", 0, 0, 128);
  texture[IMG_SCANNER_2] = gfx_load_texture("scanner.bmp", 128, 0, 128);
  texture[IMG_SCANNER_3] = gfx_load_texture("scanner.bmp", 256, 0, 128);
  texture[IMG_SCANNER_4] = gfx_load_texture("scanner.bmp", 384, 0, 128);

  texture[IMG_BLAKE] = gfx_load_texture("blake.bmp", 0, 1, 128);
  texture[IMG_ELITE_TXT] = gfx_load_texture("elitetx3.bmp", 0, 0, 256);
  texture[IMG_GREEN_DOT] = gfx_load_texture("greendot.bmp", 0, 0, 16);
  texture[IMG_MISSILE_GREEN] = gfx_load_texture("missgrn.bmp", 0, 0, 16);
  texture[IMG_MISSILE_RED] = gfx_load_texture("missred.bmp", 0, 0, 16);
  texture[IMG_MISSILE_YELLOW] = gfx_load_texture("missyell.bmp", 0, 0, 16);

  texture[IMG_BIG_E] = gfx_load_texture("ecm.bmp", 0, 0, 32);
  texture[IMG_RED_DOT] = gfx_load_texture("reddot.bmp", 0, 0, 16);
  texture[IMG_BIG_S] = gfx_load_texture("safe.bmp", 0, 0, 32);

  gfx_build_gl_circles();

  Elite_1_base = gfx_load_font("verd2.bmp", "verd2msk.bmp");
  Elite_2_base = gfx_load_font("verd4.bmp", "verd4msk.bmp");

  return 0;
}

void gfx_set_color(int index)
{
  if (scanner_image && scanner_image->palette && index < scanner_image->paletteSize)
  {
    const RGBQUAD& color = scanner_image->palette[index];
    glColor3ub(color.rgbRed, color.rgbGreen, color.rgbBlue);
  }
  else
  {
    glColor3ub(255, 255, 255);
  }
}

void apply_standard_transformation(void) { glTranslatef(GFX_X_OFFSET, GFX_Y_OFFSET, 0.0); }

void gfx_gl_print(int x, int y, const char *string, int base, int col)
{
  glLoadIdentity();
  apply_standard_transformation();
  glTranslated(x, y - 1, 0);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  gfx_set_color(col);

  glListBase(base - 32);
  glCallLists(strlen(string),GL_BYTE, string);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

}

GLvoid gfx_gl_cen_print(int x, int y, const char *string, int base, int col)
{
  GLfloat matrix[16];
  GLint buffer;

  glGetIntegerv(GL_DRAW_BUFFER, &buffer);
  glLoadIdentity();
  glDrawBuffer(GL_NONE);
  glListBase(base - 32);
  glCallLists(strlen(string),GL_BYTE, string);

  glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  glLoadIdentity();
  apply_standard_transformation();
  glTranslated(x - (matrix[12] / 2), y - 1, 0);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  glDrawBuffer(buffer);
  gfx_set_color(col);

  glListBase(base - 32);
  glCallLists(strlen(string),GL_BYTE, string);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

}

void gfx_graphics_shutdown(void)
{
  scanner_image.reset();

  // Shutdown OpenGL context
  OpenGLContext::Shutdown();
}

void gfx_update_screen(void)
{
  glFlush();
#ifdef DOUBLEBUFFER
  OpenGLContext::SwapBuffers();
#endif
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDepthMask(GL_FALSE);
}

void gfx_plot_pixel(int x, int y, int col)
{
  glLoadIdentity();
  apply_standard_transformation();
  gfx_set_color(col);
  glBegin(GL_POINTS);
  glVertex2d(x, y);
  glEnd();
}

void gfx_draw_gl_circle(int cx, int cy, int radius, int circle_colour, int type)
{
  glLoadIdentity();
  apply_standard_transformation();
  glTranslated(cx, cy, 0.0);
  glScaled(radius, radius, 1.0);
  gfx_set_color(circle_colour);
  glCallList(type);
}

void gfx_draw_filled_circle(int cx, int cy, int radius, int circle_colour) { gfx_draw_gl_circle(cx, cy, radius, circle_colour, FILLED_CIRCLE); }

void gfx_draw_circle(int cx, int cy, int radius, int circle_colour) { gfx_draw_gl_circle(cx, cy, radius, circle_colour, WIREFRAME_CIRCLE); }

void gfx_draw_line(int x1, int y1, int x2, int y2) { gfx_draw_colour_line(x1, y1, x2, y2, GFX_COL_WHITE); }

void gfx_draw_colour_line(int x1, int y1, int x2, int y2, int line_colour)
{
  glLoadIdentity();
  apply_standard_transformation();
  gfx_set_color(line_colour);
  glBegin(GL_LINES);
  glVertex2d(x1, y1);
  glVertex2d(x2, y2);
  glEnd();
}

void gfx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int col)
{
  glLoadIdentity();
  apply_standard_transformation();
  gfx_set_color(col);
  glBegin(GL_TRIANGLES);
  glVertex2d(x1, y1);
  glVertex2d(x2, y2);
  glVertex2d(x3, y3);
  glEnd();
}

void gfx_display_text(int x, int y, const char *txt) { gfx_display_colour_text(x, y, txt, GFX_COL_WHITE); }

void gfx_display_colour_text(int x, int y, const char *txt, int col) { gfx_gl_print(x, y, txt, Elite_1_base, col); }

void gfx_display_centre_text(int y, const char *str, int psize, int col)
{
  int txt_size;
  int txt_colour;

  if (psize == 140)
  {
    txt_size = Elite_2_base;
    txt_colour = GFX_COL_WHITE;
  }
  else
  {
    txt_size = Elite_1_base;
    txt_colour = col;
  }
  gfx_gl_cen_print((128 * GFX_SCALE), y, str, txt_size, txt_colour);
}

void gfx_draw_rectangle(int tx, int ty, int bx, int by, int col)
{
  glLoadIdentity();
  apply_standard_transformation();
  gfx_set_color(col);
  glBegin(GL_QUADS);
  glVertex2d(tx, ty);
  glVertex2d(bx, ty);
  glVertex2d(bx, by);
  glVertex2d(tx, by);
  glEnd();
}

void gfx_display_pretty_text(int tx, int ty, int bx, int by, const char *txt)
{
  char strbuf[100];
  char *bptr;

  int maxlen = (bx - tx) / 8;

  const char *str = txt;
  int len = strlen(txt);

  while (len > 0)
  {
    int pos = maxlen;
    if (pos > len) pos = len;

    while ((str[pos] != ' ') && (str[pos] != ',') && (str[pos] != '.') && (str[pos] != '\0')) { pos--; }

    len = len - pos - 1;

    for (bptr = strbuf; pos >= 0; pos--) *bptr++ = *str++;

    *bptr = '\0';

    gfx_display_colour_text(tx, ty, strbuf, GFX_COL_WHITE);
    ty += (8 * GFX_SCALE);
  }
}

void gfx_draw_scanner(void)
{
  int scanner_textures[4] = {IMG_SCANNER_1, IMG_SCANNER_2, IMG_SCANNER_3, IMG_SCANNER_4};

  glLoadIdentity();
  apply_standard_transformation();
  glTranslatef(0.0, 385.0, 0.0);

  glColor4f(1.0, 1.0, 1.0, 1.0);
  glEnable(GL_TEXTURE_2D);

  for (int i = 0; i < 4; i++)
  {
    glBindTexture(GL_TEXTURE_2D, texture[scanner_textures[i]]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2d(0.0, 0.0);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2d(128.0, 0.0);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2d(128.0, 128.0);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2d(0.0, 128.0);
    glEnd();
    glTranslatef(128.0, 0.0, 0.0);
  }

  glDisable(GL_TEXTURE_2D);

  gfx_draw_line(0, 1, 0, 384);
  gfx_draw_line(0, 1, 511, 1);
  gfx_draw_line(511, 1, 511, 384);
}

void gfx_set_clip_region(int tx, int ty, int bx, int by)
{
  clip_tx = tx;
  clip_ty = ty;
  clip_bx = bx;
  clip_by = by;

  int screen_w = static_cast<int>(ClientEngine::OutputSize().Width);
  int screen_h = static_cast<int>(ClientEngine::OutputSize().Height);
  glViewport((tx + GFX_X_OFFSET) * screen_w / 800, (600 - (by + GFX_Y_OFFSET)) * screen_h / 600, (bx - tx) * screen_w / 800, (by - ty) * screen_h / 600);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(tx + GFX_X_OFFSET, bx + GFX_X_OFFSET, by + GFX_Y_OFFSET, ty + GFX_Y_OFFSET, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
}

void gfx_resize_window(int width, int height)
{
  // Size is now managed by ClientEngine::OutputSize()
  // Just refresh the clip region
  gfx_set_clip_region(clip_tx, clip_ty, clip_bx, clip_by);
}

void gfx_draw_sprite(int sprite_no, int x, int y)
{
  GLfloat height, width;

  glLoadIdentity();
  apply_standard_transformation();

  glColor4f(1.0f, 1.0f, 1.0f, 1.0);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  glBindTexture(GL_TEXTURE_2D, texture[sprite_no]);
  glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

  if (x == -1) x = ((256 * GFX_SCALE) - 192) / 2;
  glTranslatef(static_cast<float>(x), static_cast<float>(y), 0.0f);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(0.0, 0.0);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(width, 0.0);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(width, height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(0.0, height);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

}

void gfx_draw_view(void)
{
  struct univ_object copy;

  gfx_set_clip_region(1, 2, 510, 383);

  for (int i = 0; i < MAX_UNIV_OBJECTS; i++)
  {
    if ((universe[i].type == 0) || (universe[i].distance < 170)) continue;

    copy = universe[i];

    quaternion_slerp(&universe[i].oldquat, &universe[i].quat, timeslice, &copy.quat);
    quat_to_matrix(&copy.quat, copy.rotmat);

    copy.location.x = ((universe[i].location.x - universe[i].oldlocation.x) * timeslice) + universe[i].oldlocation.x;
    copy.location.y = ((universe[i].location.y - universe[i].oldlocation.y) * timeslice) + universe[i].oldlocation.y;
    copy.location.z = ((universe[i].location.z - universe[i].oldlocation.z) * timeslice) + universe[i].oldlocation.z;

    draw_ship(&copy);

    universe[i].flags &= ~FLG_FIRING;
  }

  gfx_set_clip_region(0, 0, 512, 512);
}

void gfx_advance_frame(void)
{
  for (int i = 0; i < MAX_UNIV_OBJECTS; i++)
  {
    universe[i].oldlocation = universe[i].location;
    universe[i].oldquat = universe[i].quat;
  }
}

void gfx_set_camera(int angle)
{

  double radians = static_cast<double>(angle) * PI / 180.0;

  camera[0].x = cos(radians);
  camera[1].x = 0;
  camera[2].x = -sin(radians);
  camera[3].x = 0;
  camera[0].y = 0;
  camera[1].y = 1;
  camera[2].y = 0;
  camera[3].y = 0;
  camera[0].z = sin(radians);
  camera[1].z = 0;
  camera[2].z = cos(radians);
  camera[3].z = 0;
  camera[0].w = 0;
  camera[1].w = 0;
  camera[2].w = 0;
  camera[3].w = 1;
}

void get_object_matrix(univ_object *univ, Matrix4 matrix)
{
  matrix[0].x = univ->rotmat[0].x;
  matrix[0].y = univ->rotmat[0].y;
  matrix[0].z = -univ->rotmat[0].z;
  matrix[0].w = 0;
  matrix[1].x = univ->rotmat[1].x;
  matrix[1].y = univ->rotmat[1].y;
  matrix[1].z = -univ->rotmat[1].z;
  matrix[1].w = 0;
  matrix[2].x = univ->rotmat[2].x;
  matrix[2].y = univ->rotmat[2].y;
  matrix[2].z = -univ->rotmat[2].z;
  matrix[2].w = 0;
  matrix[3].x = univ->location.x;
  matrix[3].y = univ->location.y;
  matrix[3].z = -univ->location.z;
  matrix[3].w = 1;
}

void transform_ship_point(struct univ_object *univ, struct ship_point *point, Vector *result)
{
  Matrix4 trans_mat;

  get_object_matrix(univ, trans_mat);
  result->x = point->x;
  result->y = point->y;
  result->z = point->z;

  mult_vector4(result, trans_mat);
}

void draw_gl_face(struct ship_face *face_data, struct ship_point *points)
{

  int num_points = face_data->points;

  glBegin((num_points == 2) ? GL_LINES : GL_POLYGON);

  glVertex3iv((GLint *) &points[face_data->p1]);
  glVertex3iv((GLint *) &points[face_data->p2]);
  if (num_points > 2) glVertex3iv((GLint *) &points[face_data->p3]);
  if (num_points > 3) glVertex3iv((GLint *) &points[face_data->p4]);
  if (num_points > 4) glVertex3iv((GLint *) &points[face_data->p5]);
  if (num_points > 5) glVertex3iv((GLint *) &points[face_data->p6]);
  if (num_points > 6) glVertex3iv((GLint *) &points[face_data->p7]);
  if (num_points > 7) glVertex3iv((GLint *) &points[face_data->p8]);

  glEnd();
}

void gfx_draw_gl_ship(struct univ_object *univ)
{
  int i;
  struct ship_face *face_data;

  Vector laser_point;
  //	char outputx[100], outputy[100], outputz[100];
  Matrix4 object_matrix;

  struct ship_data* ship = ship_list[univ->type];
  struct ship_solid* solid_data = &ship_solids[univ->type];
  face_data = solid_data->face_data;
  /*
    sprintf(outputx, "X: %f", univ->location.x);
    sprintf(outputy, "Y: %f", univ->location.y);
    sprintf(outputz, "Z: %f", univ->location.z);
  
    gfx_display_text(16, 16, outputx);
    gfx_display_text(16, 32, outputy);
    gfx_display_text(16, 48, outputz);
  */

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glFrustum(-8, 8, -6, 6, 16, 65535);
  glMatrixMode(GL_MODELVIEW);

  glLoadMatrixd((GLdouble *) camera);

  get_object_matrix(univ, object_matrix);
  glMultMatrixd((GLdouble *) object_matrix);

  glEnable(GL_CULL_FACE);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  if (wireframe)
  {
    for (i = 0; i < solid_data->num_faces; i++)
    {
      glColor3d(0, 0, 0);
      draw_gl_face(&face_data[i], ship->points);

      gfx_set_color(GFX_COL_WHITE);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      draw_gl_face(&face_data[i], ship->points);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
  }
  else
  {
    for (i = 0; i < solid_data->num_faces; i++)
    {
      gfx_set_color(face_data[i].colour);
      draw_gl_face(&face_data[i], ship->points);
    }
  }

  /*
glColor3d(1.0,1.0,0.0);
glBegin(GL_LINES);
glVertex3d(0.0,0.0,0.0);
glVertex3d(0.0,0.0,1000.0);
glVertex3d(0.0,0.0,0.0);
glVertex3d(0.0,1000.0,0.0);
glVertex3d(0.0,0.0,0.0);
glVertex3d(1000.0,0.0,0.0);
glEnd();*/

  glDepthMask(GL_TRUE);
  glColor4d(0, 0, 0, 0);
  glEnable(GL_BLEND);

  //	glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

  for (i = 0; i < solid_data->num_faces; i++) draw_gl_face(&face_data[i], ship->points);

  //	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

  glDisable(GL_BLEND);
  glDepthMask(GL_FALSE);

  glDisable(GL_DEPTH_TEST);

  glDisable(GL_CULL_FACE);

  if (univ->flags & FLG_FIRING)
  {
    int lasv = ship_list[univ->type]->front_laser;
    int col = (univ->type == SHIP_VIPER && (!wireframe)) ? GFX_COL_CYAN : GFX_COL_WHITE;

    glLoadIdentity();
    transform_ship_point(univ, &ship->points[lasv], &laser_point);
    mult_vector4(&laser_point, camera);

    gfx_set_color(col);

    glBegin(GL_LINES);
    glVertex3dv((GLdouble *) &laser_point);
    glVertex3i(laser_point.x > 0 ? -8 : 8, rand255() / 16 - 10, -16);
    glEnd();
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}