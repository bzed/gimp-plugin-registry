/*
	normalmap GIMP plugin

	Copyright (C) 2002-2008 Shawn Kirst <skirst@insightbb.com>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; see the file COPYING.  If not, write to
	the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
	Boston, MA 02111-1307, USA.
*/

#include <string.h>
#include <math.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <GL/glew.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "scale.h"

#include "objects/quad.h"
#include "objects/cube.h"
#include "objects/sphere.h"
#include "objects/torus.h"
#include "objects/teapot.h"

#include "pixmaps/object.xpm"
#include "pixmaps/light.xpm"
#include "pixmaps/scene.xpm"
#include "pixmaps/full.xpm"

#define IS_POT(x)  (((x) & ((x) - 1)) == 0)

typedef float matrix[16];
typedef float vec4[4];
typedef float vec3[3];
typedef float vec2[2];

typedef enum
{
   BUMPMAP_NORMAL = 0, BUMPMAP_PARALLAX, BUMPMAP_POM, BUMPMAP_RELIEF,
   BUMPMAP_MAX
} BUMPMAP_TYPE;

typedef enum
{
   ROTATE_OBJECT = 0, ROTATE_LIGHT, ROTATE_SCENE,
   ROTATE_MAX
} ROTATE_TYPE;

typedef enum
{
   OBJECT_QUAD = 0, OBJECT_CUBE, OBJECT_SPHERE, OBJECT_TORUS, OBJECT_TEAPOT,
   OBJECT_MAX
} OBJECT_TYPE;

static int _active = 0;
static int _gl_error = 0;
static gint32 normalmap_drawable_id = -1;
static GtkWidget *window = 0;
static GtkWidget *glarea = 0;
static GtkWidget *rotate_obj_btn = 0;
static GtkWidget *object_opt = 0;
static GtkWidget *controls_table = 0;
static GtkWidget *bumpmapping_opt = 0;
static GtkWidget *specular_check = 0;
static GtkWidget *gloss_opt = 0;
static GtkWidget *specular_exp_range = 0;
static GtkWidget *ambient_color_btn = 0;
static GtkWidget *diffuse_color_btn = 0;
static GtkWidget *specular_color_btn = 0;
static GtkWidget *uvscale_spin1 = 0;
static GtkWidget *uvscale_spin2 = 0;

static int fullscreen = 0;

static GLuint diffuse_tex = 0;
static GLuint gloss_tex = 0;
static GLuint normal_tex = 0;
static GLuint white_tex = 0;

static struct
{
   float *verts;
   unsigned short *indices;
   unsigned int num_verts;
   unsigned int num_indices;
   GLuint vbo;
} object_info[OBJECT_MAX] =
{
   {quad_verts,   quad_indices,   QUAD_NUM_VERTS,   QUAD_NUM_INDICES,   0},
   {cube_verts,   cube_indices,   CUBE_NUM_VERTS,   CUBE_NUM_INDICES,   0},
   {sphere_verts, sphere_indices, SPHERE_NUM_VERTS, SPHERE_NUM_INDICES, 0},
   {torus_verts,  torus_indices,  TORUS_NUM_VERTS,  TORUS_NUM_INDICES,  0},
   {teapot_verts, teapot_indices, TEAPOT_NUM_VERTS, TEAPOT_NUM_INDICES, 0}
};

static const float anisotropy = 4.0f;

static int has_glsl = 0;
static int has_npot = 0;
static int has_generate_mipmap = 0;
static int has_aniso = 0;
static int num_mtus = 0;

static int max_instructions = 0;
static int max_indirections = 0;

static GLhandleARB programs[BUMPMAP_MAX];

static const char *vert_source =
   "varying vec2 tex;\n"
   "varying vec3 vpos;\n"
   "varying vec3 normal;\n"
   "varying vec3 tangent;\n"
   "varying vec3 binormal;\n"
   "\n"
   "uniform vec2 uvscale;\n"
   "\n"
   "void main()\n"
   "{\n"
   "   gl_Position = ftransform();\n"
   "   tex = gl_MultiTexCoord0.xy * uvscale;\n"
   "   vpos = (gl_ModelViewMatrix * gl_Vertex).xyz;\n"
   "   tangent  = gl_NormalMatrix * gl_MultiTexCoord3.xyz;\n"
   "   binormal = gl_NormalMatrix * gl_MultiTexCoord4.xyz;\n"
   "   normal   = gl_NormalMatrix * gl_Normal;\n"
   "}\n";

static const char *normal_frag_source =
   "varying vec2 tex;\n"
   "varying vec3 vpos;\n"
   "varying vec3 normal;\n"
   "varying vec3 tangent;\n"
   "varying vec3 binormal;\n"

   "uniform sampler2D sNormal;\n"
   "uniform sampler2D sDiffuse;\n"
   "uniform sampler2D sGloss;\n\n"

   "uniform vec3 lightDir;\n"
   "uniform bool specular;\n"
   "uniform float specular_exp;\n"
   "uniform vec3 ambient_color;\n"
   "uniform vec3 diffuse_color;\n"
   "uniform vec3 specular_color;\n\n"

   "void main()\n"
   "{\n"
   "   vec3 V = normalize(vpos);\n"
   "   vec3 N = texture2D(sNormal, tex).rgb * 2.0 - 1.0;\n"
   "   N = normalize(N.x * tangent + N.y * binormal + N.z * normal);\n"
   "   vec3 diffuse = texture2D(sDiffuse, tex).rgb;\n"
   "   float NdotL = clamp(dot(N, lightDir), 0.0, 1.0);\n"
   "   vec3 color = diffuse * diffuse_color * NdotL;\n"
   "   if(specular)\n"
   "   {\n"
   "      vec3 gloss = texture2D(sGloss, tex).rgb;\n"
   "      vec3 R = reflect(V, N);\n"
   "      float RdotL = clamp(dot(R, lightDir), 0.0, 1.0);\n"
   "      color += gloss * specular_color * pow(RdotL, specular_exp);\n"
   "   }\n"
   "   gl_FragColor.rgb = ambient_color * diffuse + color;\n"
   "}\n";

static const char *parallax_frag_source =
   "varying vec2 tex;\n"
   "varying vec3 vpos;\n"
   "varying vec3 normal;\n"
   "varying vec3 tangent;\n"
   "varying vec3 binormal;\n"

   "uniform sampler2D sNormal;\n"
   "uniform sampler2D sDiffuse;\n"
   "uniform sampler2D sGloss;\n\n"

   "uniform vec3 lightDir;\n"
   "uniform bool specular;\n"
   "uniform float specular_exp;\n"
   "uniform vec3 ambient_color;\n"
   "uniform vec3 diffuse_color;\n"
   "uniform vec3 specular_color;\n\n"

   "void main()\n"
   "{\n"
   "   mat3 TBN = mat3(tangent, binormal, normal);\n"
   "   vec3 V = normalize(vpos);\n"
   "   vec3 V_ts = V * TBN;\n"
   "   float height = texture2D(sNormal, tex).a;\n"
   "   float offset = height * 0.025 - 0.0125;\n"
   "   vec2 tc = tex + offset * V_ts.xy;\n"
   "   height += texture2D(sNormal, tc).a;\n"
   "   offset = 0.025 * (height - 1.0);\n"
   "   tc = tex + offset * V_ts.xy;\n"
   "   vec3 N = texture2D(sNormal, tc).rgb * 2.0 - 1.0;\n"
   "   N = normalize(N.x * tangent + N.y * binormal + N.z * normal);\n"
   "   vec3 diffuse = texture2D(sDiffuse, tc).rgb;\n"
   "   float NdotL = clamp(dot(N, lightDir), 0.0, 1.0);\n"
   "   vec3 color = diffuse * diffuse_color * NdotL;\n"
   "   if(specular)\n"
   "   {\n"
   "      vec3 gloss = texture2D(sGloss, tc).rgb;\n"
   "      vec3 R = reflect(V, N);\n"
   "      float RdotL = clamp(dot(R, lightDir), 0.0, 1.0);\n"
   "      color += gloss * specular_color * pow(RdotL, specular_exp);\n"
   "   }\n"
   "   gl_FragColor.rgb = ambient_color * diffuse + color;\n"
   "}\n";

static const char *pom_frag_source =
   "varying vec2 tex;\n"
   "varying vec3 vpos;\n"
   "varying vec3 normal;\n"
   "varying vec3 tangent;\n"
   "varying vec3 binormal;\n"
   "\n"
   "uniform sampler2D sNormal;\n"
   "uniform sampler2D sDiffuse;\n"
   "uniform sampler2D sGloss;\n"
   "\n"
   "uniform vec3 lightDir;\n"
   "uniform bool specular;\n"
   "uniform vec3 ambient_color;\n"
   "uniform vec3 diffuse_color;\n"
   "uniform vec3 specular_color;\n"
   "uniform float specular_exp;\n"
   "uniform vec2 planes;\n"
   "uniform float depth_factor;\n"
   "\n"
   "void ray_intersect(sampler2D reliefMap, inout vec4 p, inout vec3 v)\n"
   "{\n"
   "   const int search_steps = 20;\n"
   "\n"
   "   v /= float(search_steps);\n"
   "\n"
   "   vec4 pp = p;\n"
   "   for(int i = 0; i < search_steps - 1; ++i)\n"
   "   {\n"
   "      p.w = texture2D(reliefMap, p.xy).w;\n"
   "      if(p.w > p.z)\n"
   "      {\n"
   "         pp = p;\n"
   "         p.xyz += v;\n"
   "      }\n"
   "   }\n"
   "\n"
   "   float f = (pp.w - pp.z) / (p.z - pp.z - p.w + pp.w);\n"
   "   p = mix(pp, p, f);\n"
   "}\n"
   "\n"
   "void ray_intersect_ATI(sampler2D reliefMap, inout vec4 p, inout vec3 v)"
   "{\n"
   "   float h0 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 1.000).a;\n"
   "   float h1 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.875).a;\n"
   "   float h2 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.750).a;\n"
   "   float h3 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.625).a;\n"
   "   float h4 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.500).a;\n"
   "   float h5 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.375).a;\n"
   "   float h6 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.250).a;\n"
   "   float h7 = 1.0 - texture2D(reliefMap, p.xy + v.xy * 0.125).a;\n"
   "\n"
   "   float x, y, xh, yh;\n"
   "   if     (h7 > 0.875) { x = 0.937; y = 0.938; xh = h7; yh = h7; }\n"
   "   else if(h6 > 0.750) { x = 0.750; y = 0.875; xh = h6; yh = h7; }\n"
   "   else if(h5 > 0.625) { x = 0.625; y = 0.750; xh = h5; yh = h6; }\n"
   "   else if(h4 > 0.500) { x = 0.500; y = 0.625; xh = h4; yh = h5; }\n"
   "   else if(h3 > 0.375) { x = 0.375; y = 0.500; xh = h3; yh = h4; }\n"
   "   else if(h2 > 0.250) { x = 0.250; y = 0.375; xh = h2; yh = h3; }\n"
   "   else if(h1 > 0.125) { x = 0.125; y = 0.250; xh = h1; yh = h2; }\n"
   "   else                { x = 0.000; y = 0.125; xh = h0; yh = h1; }\n"
   "\n"
   "   float parallax = (x * (y - yh) - y * (x - xh)) / ((y - yh) - (x - xh));\n"
   "   p.xyz += v * (1.0 - parallax);\n"
   "}\n"
   "\n"
   "void main()\n"
   "{\n"
   "\n"
   "   vec3 V = normalize(vpos);\n"
   "   float a = dot(normal, -V);\n"
   "   vec3 v = vec3(dot(V, tangent), dot(V, binormal), a);\n"
   "   vec3 scale = vec3(1.0, 1.0, depth_factor);\n"
   "   v *= scale.z / (scale * v.z);\n"
   "   vec4 p = vec4(tex, vec2(0.0, 1.0));\n"
   "#ifdef ATI\n"
   "   ray_intersect_ATI(sNormal, p, v);\n"
   "#else\n"
   "   ray_intersect(sNormal, p, v);\n"
   "#endif\n"
   "\n"
   "   vec2 uv = p.xy;\n"
   "   vec3 N = texture2D(sNormal, uv).xyz * 2.0 - 1.0;\n"
   "   vec3 diffuse = texture2D(sDiffuse, uv).rgb;\n"
   "\n"
   "   N.z = sqrt(1.0 - dot(N.xy, N.xy));\n"
   "   N = normalize(N.x * tangent + N.y * binormal + N.z * normal);\n"
   "\n"
   "   float NdotL = clamp(dot(N, lightDir), 0.0, 1.0);\n"
   "\n"
   "   vec3 color = diffuse * diffuse_color * NdotL;\n"
   "\n"
   "   if(specular)\n"
   "   {\n"
   "      vec3 gloss = texture2D(sGloss, uv).rgb;\n"
   "      vec3 R = reflect(V, N);\n"
   "      float RdotL = clamp(dot(R, lightDir), 0.0, 1.0);\n"
   "      color += gloss * specular_color * pow(RdotL, specular_exp);\n"
   "   }\n"
   "\n"
   "   gl_FragColor.rgb = ambient_color * diffuse + color;\n"
   "}\n";

static const char *relief_frag_source =
   "varying vec2 tex;\n"
   "varying vec3 vpos;\n"
   "varying vec3 normal;\n"
   "varying vec3 tangent;\n"
   "varying vec3 binormal;\n"
   "\n"
   "uniform sampler2D sNormal;\n"
   "uniform sampler2D sDiffuse;\n"
   "uniform sampler2D sGloss;\n"
   "\n"
   "uniform vec3 lightDir;\n"
   "uniform bool specular;\n"
   "uniform vec3 ambient_color;\n"
   "uniform vec3 diffuse_color;\n"
   "uniform vec3 specular_color;\n"
   "uniform float specular_exp;\n"
   "uniform vec2 planes;\n"
   "uniform float depth_factor;\n"
   "\n"
   "float ray_intersect(sampler2D reliefMap, vec2 dp, vec2 ds)\n"
   "{\n"
   "   const int linear_search_steps = 20;\n"
   "\n"
   "   float size = 1.0 / float(linear_search_steps);\n"
   "   float depth = 0.0;\n"
   "   float best_depth = 1.0;\n"
   "\n"
   "   for(int i = 0; i < linear_search_steps - 1; ++i)\n"
   "   {\n"
   "      depth += size;\n"
   "      float t = texture2D(reliefMap, dp + ds * depth).a;\n"
   "      if(best_depth > 0.996)\n"
   "         if(depth >= t)\n"
   "            best_depth = depth;\n"
   "   }\n"
   "   depth = best_depth;\n"
   "\n"
   "   const int binary_search_steps = 5;\n"
   "\n"
   "   for(int i = 0; i < binary_search_steps; ++i)\n"
   "   {\n"
   "      size *= 0.5;\n"
   "      float t = texture2D(reliefMap, dp + ds * depth).a;\n"
   "      if(depth >= t)\n"
   "      {\n"
   "         best_depth = depth;\n"
   "         depth -= 2.0 * size;\n"
   "      }\n"
   "      depth += size;\n"
   "   }\n"
   "\n"
   "   return(best_depth);\n"
   "}\n"
   "\n"
   "void main()\n"
   "{\n"
   "\n"
   "   vec3 V = normalize(vpos);\n"
   "   float a = dot(normal, -V);\n"
   "   vec2 s = vec2(dot(V, tangent), dot(V, binormal));\n"
   "   s *= depth_factor / a;\n"
   "   vec2 ds = s;\n"
   "   vec2 dp = tex;\n"
   "   float d = ray_intersect(sNormal, dp, ds);\n"
   "\n"
   "   vec2 uv = dp + ds * d;\n"
   "   vec3 N = texture2D(sNormal, uv).xyz * 2.0 - 1.0;\n"
   "   vec3 diffuse = texture2D(sDiffuse, uv).rgb;\n"
   "\n"
   "   N.z = sqrt(1.0 - dot(N.xy, N.xy));\n"
   "   N = normalize(N.x * tangent + N.y * binormal + N.z * normal);\n"
   "\n"
   "   float NdotL = clamp(dot(N, lightDir), 0.0, 1.0);\n"
   "\n"
   "   vec3 color = diffuse * diffuse_color * NdotL;\n"
   "\n"
   "   if(specular)\n"
   "   {\n"
   "      vec3 gloss = texture2D(sGloss, uv).rgb;\n"
   "      vec3 R = reflect(V, N);\n"
   "      float RdotL = clamp(dot(R, lightDir), 0.0, 1.0);\n"
   "      color += gloss * specular_color * pow(RdotL, specular_exp);\n"
   "   }\n"
   "\n"
   "   gl_FragColor.rgb = ambient_color * diffuse + color;\n"
   "}\n";

static int bumpmapping = BUMPMAP_NORMAL;
static int specular = 0;
static int rotate_type = ROTATE_OBJECT;
static int object_type = OBJECT_QUAD;

static vec3 ambient_color = {0.2f, 0.2f, 0.2f};
static vec3 diffuse_color = {1, 1, 1};
static vec3 specular_color = {1, 1, 1};
static float specular_exp = 32.0f;
static vec3 uvscale = {1, 1};

static const float depth_factor = 0.05f;

static int mx;
static int my;
static vec3 object_rot;
static vec3 light_rot;
static vec3 scene_rot;
static float zoom;

#define M(r,c) m[(c << 2) + r]
#define T(r,c) t[(c << 2) + r]

static void mat_invert(matrix m)
{
   float invdet;
   matrix t;

   invdet = (float)1.0 / (M(0, 0) * (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1)) -
                          M(0, 1) * (M(1, 0) * M(2, 2) - M(1, 2) * M(2, 0)) +
                          M(0, 2) * (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0)));

   T(0,0) =  invdet * (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1));
   T(0,1) = -invdet * (M(0, 1) * M(2, 2) - M(0, 2) * M(2, 1));
   T(0,2) =  invdet * (M(0, 1) * M(1, 2) - M(0, 2) * M(1, 1));
   T(0,3) = 0;

   T(1,0) = -invdet * (M(1, 0) * M(2, 2) - M(1, 2) * M(2, 0));
   T(1,1) =  invdet * (M(0, 0) * M(2, 2) - M(0, 2) * M(2, 0));
   T(1,2) = -invdet * (M(0, 0) * M(1, 2) - M(0, 2) * M(1, 0));
   T(1,3) = 0;

   T(2,0) =  invdet * (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0));
   T(2,1) = -invdet * (M(0, 0) * M(2, 1) - M(0, 1) * M(2, 0));
   T(2,2) =  invdet * (M(0, 0) * M(1, 1) - M(0, 1) * M(1, 0));
   T(2,3) = 0;

   T(3,0) = -(M(3, 0) * T(0, 0) + M(3, 1) * T(1, 0) + M(3, 2) * T(2, 0));
   T(3,1) = -(M(3, 0) * T(0, 1) + M(3, 1) * T(1, 1) + M(3, 2) * T(2, 1));
   T(3,2) = -(M(3, 0) * T(0, 2) + M(3, 1) * T(1, 2) + M(3, 2) * T(2, 2));
   T(3,3) = 1;

   memcpy(m, t, 16 * sizeof(float));
}

static void mat_transpose(matrix m)
{
   matrix t;
   t[0 ] = m[0 ]; t[1 ] = m[4 ]; t[2 ] = m[8 ]; t[3 ] = m[12];
   t[4 ] = m[1 ]; t[5 ] = m[5 ]; t[6 ] = m[9 ]; t[7 ] = m[13];
   t[8 ] = m[2 ]; t[9 ] = m[6 ]; t[10] = m[10]; t[11] = m[14];
   t[12] = m[3 ]; t[13] = m[7 ]; t[14] = m[11]; t[15] = m[15];
   memcpy(m, t, sizeof(matrix));
}

static void mat_mult_vec(vec3 v, matrix m)
{
   vec3 t;
   t[0] = M(0, 0) * v[0] + M(0, 1) * v[1] + M(0, 2) * v[2];
   t[1] = M(1, 0) * v[0] + M(1, 1) * v[1] + M(1, 2) * v[2];
   t[2] = M(2, 0) * v[0] + M(2, 1) * v[1] + M(2, 2) * v[2];

   v[0] = t[0];
   v[1] = t[1];
   v[2] = t[2];
}

static inline void vec3_set(vec3 v, float x, float y, float z)
{
   v[0] = x;
   v[1] = y;
   v[2] = z;
}

static inline void vec3_copy(vec3 r, vec3 v)
{
   r[0] = v[0];
   r[1] = v[1];
   r[2] = v[2];
}

static void vec4_normalize(vec4 r, vec4 v)
{
   float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
   if(len != 0)
   {
      float ilen = 1.0f / len;
      r[0] = v[0] * ilen;
      r[1] = v[1] * ilen;
      r[2] = v[2] * ilen;
      r[3] = v[3] * ilen;
   }
   else
      r[0] = r[1] = r[2] = r[3] = 0;
}

static void vec3_normalize(vec3 r, vec3 v)
{
   float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
   if(len != 0.0f)
   {
      float ilen = 1.0f / len;
      r[0] = v[0] * ilen;
      r[1] = v[1] * ilen;
      r[2] = v[2] * ilen;
   }
   else
      r[0] = r[1] = r[2] = 0;
}

static inline void quat_ident(vec4 q)
{
   q[0] = q[1] = q[2] = 0;
   q[3] = 1;
}

static void quat_mul(vec4 r, vec4 a, vec4 b)
{
   r[0] = a[0] * b[3] + b[0] * a[3] + a[1] * b[2] - a[2] * b[1];
   r[1] = a[1] * b[3] + b[1] * a[3] + a[2] * b[0] - a[0] * b[2];
   r[2] = a[2] * b[3] + b[2] * a[3] + a[0] * b[1] - a[1] * b[0];
   r[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

static void quat_rotate(vec4 q, float a, float x, float y, float z)
{
   float hs, len, ilen;

   len = sqrtf(x * x + y * y + z * z);
   if(len == 0) return;
   ilen = 1.0f / len;
   x *= ilen;
   y *= ilen;
   z *= ilen;

   a = (a * (M_PI / 180.0f)) * 0.5f;

   hs = sinf(a);
   q[0] = x * hs;
   q[1] = y * hs;
   q[2] = z * hs;
   q[3] = cosf(a);
}

static void quat_get_direction(vec3 v, vec4 q)
{
   v[0] = 2.0f * (q[0] * q[2] - q[3] * q[1]);
   v[1] = 2.0f * (q[1] * q[2] + q[3] * q[0]);
   v[2] = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
}

#undef M
#undef T

static void init(GtkWidget *widget, gpointer data)
{
   int i, err;
   unsigned char white[16] = {0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff};
   GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
   GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
   GtkWidget *menu;
   GList *curr;

   if(!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
      return;

   err = glewInit();
   if(err != GLEW_OK)
   {
      g_message("%s", (char *)glewGetErrorString(err));
      _gl_error = 1;
   }

   glClearColor(0, 0, 0.4f, 0);
   glDepthFunc(GL_LEQUAL);
   glEnable(GL_DEPTH_TEST);

   glLineWidth(3);

   _gl_error = 0;

   if(!GLEW_ARB_multitexture)
   {
      g_message("GL_ARB_multitexture is required for the 3D preview");
      _gl_error = 1;
   }

   if(!GLEW_ARB_texture_env_combine)
   {
      g_message("GL_ARB_texture_env_combine is required for the 3D preview");
      _gl_error = 1;
   }

   if(!GLEW_ARB_texture_env_dot3)
   {
      g_message("GL_ARB_texture_env_dot3 is required for the 3D preview");
      _gl_error = 1;
   }

   if(_gl_error) return;

   glGenTextures(1, &diffuse_tex);
   glGenTextures(1, &gloss_tex);
   glGenTextures(1, &normal_tex);
   glGenTextures(1, &white_tex);

   glGetIntegerv(GL_MAX_TEXTURE_UNITS, &num_mtus);

   glActiveTexture(GL_TEXTURE0);
   glEnable(GL_TEXTURE_2D);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
   glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
   glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
   glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
   glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
   glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

   glActiveTexture(GL_TEXTURE1);
   glEnable(GL_TEXTURE_2D);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
   glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
   glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
   glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
   glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
   glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

   glBindTexture(GL_TEXTURE_2D, white_tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 4, 4, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, white);

   if(num_mtus > 2)
   {
      glActiveTexture(GL_TEXTURE2);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, white_tex);
   }

   has_glsl = GLEW_ARB_shader_objects && GLEW_ARB_vertex_shader &&
      GLEW_ARB_fragment_shader;
   has_npot = GLEW_ARB_texture_non_power_of_two;
   has_generate_mipmap = GLEW_SGIS_generate_mipmap;
   has_aniso = GLEW_EXT_texture_filter_anisotropic;

   if(has_glsl)
   {
      GLhandleARB prog, vert_shader, frag_shader;
      int res, len, loc;
      const char *sources[2];
      char *info;

      /* Get max # of instructions and indirections supported by the hardware.
       * Used to determine if parallax occlusion and relief mapping should be
       * enabled and if the "ATI" version of parallax occlusion mapping should
       * be used.
       */
      if(GLEW_ARB_fragment_program)
      {
         glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 1);
         glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                           GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB,
                           &max_instructions);
         glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                           GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB,
                           &max_indirections);
         glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
      }

      vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
      glShaderSourceARB(vert_shader, 1, &vert_source, 0);
      glCompileShaderARB(vert_shader);
      glGetObjectParameterivARB(vert_shader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
      if(!res)
      {
         glGetObjectParameterivARB(vert_shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
         info = g_malloc(len + 1);
         glGetInfoLogARB(vert_shader, len, 0, info);
         g_message("Vertex shader failed to compile:\n%s\n", info);
         g_free(info);
      }

      prog = glCreateProgramObjectARB();
      glAttachObjectARB(prog, vert_shader);

      frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
      glShaderSourceARB(frag_shader, 1, &normal_frag_source, 0);
      glCompileShaderARB(frag_shader);
      glGetObjectParameterivARB(frag_shader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
      if(res)
         glAttachObjectARB(prog, frag_shader);
      else
      {
         glGetObjectParameterivARB(frag_shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
         info = g_malloc(len + 1);
         glGetInfoLogARB(frag_shader, len, 0, info);
         g_message("Normal mapping fragment shader failed to compile:\n%s\n",
                   info);
         g_free(info);
         glDeleteObjectARB(prog);
         prog = 0;
      }
      glDeleteObjectARB(frag_shader);

      if(prog)
      {
         glLinkProgramARB(prog);
         glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &res);

         if(!res)
         {
            glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
            info = g_malloc(len + 1);
            glGetInfoLogARB(prog, len, 0, info);
            g_message("Normal mapping program failed to link:\n%s\n", info);
            g_free(info);
            glDeleteObjectARB(prog);
            prog = 0;
         }
      }

      programs[BUMPMAP_NORMAL] = prog;

      prog = glCreateProgramObjectARB();
      glAttachObjectARB(prog, vert_shader);

      frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
      glShaderSourceARB(frag_shader, 1, &parallax_frag_source, 0);
      glCompileShaderARB(frag_shader);
      glGetObjectParameterivARB(frag_shader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
      if(res)
         glAttachObjectARB(prog, frag_shader);
      else
      {
         glGetObjectParameterivARB(frag_shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
         info = g_malloc(len + 1);
         glGetInfoLogARB(frag_shader, len, 0, info);
         g_message("Parallax mapping fragment shader failed to compile:\n%s\n",
                   info);
         g_free(info);
         glDeleteObjectARB(prog);
         prog = 0;
      }
      glDeleteObjectARB(frag_shader);

      if(prog)
      {
         glLinkProgramARB(prog);
         glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &res);

         if(!res)
         {
            glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
            info = g_malloc(len + 1);
            glGetInfoLogARB(prog, len, 0, info);
            g_message("Parallax mapping program failed to link:\n%s\n", info);
            g_free(info);
            glDeleteObjectARB(prog);
            prog = 0;
         }
      }

      programs[BUMPMAP_PARALLAX] = prog;

      if(max_instructions >= 200)
      {
         prog = glCreateProgramObjectARB();
         glAttachObjectARB(prog, vert_shader);

         if(max_indirections < 100)
            sources[0] = "#define ATI 1\n";
         else
            sources[0] = "";

         sources[1] = pom_frag_source;

         frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
         glShaderSourceARB(frag_shader, 2, sources, 0);
         glCompileShaderARB(frag_shader);
         glGetObjectParameterivARB(frag_shader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
         if(res)
            glAttachObjectARB(prog, frag_shader);
         else
         {
            glGetObjectParameterivARB(frag_shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
            info = g_malloc(len + 1);
            glGetInfoLogARB(frag_shader, len, 0, info);
            g_message("Parallax Occlusion mapping fragment shader failed to compile:\n%s\n",
                      info);
            g_free(info);
            glDeleteObjectARB(prog);
            prog = 0;
         }
         glDeleteObjectARB(frag_shader);

         if(prog)
         {
            glLinkProgramARB(prog);
            glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &res);

            if(!res)
            {
               glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
               info = g_malloc(len + 1);
               glGetInfoLogARB(prog, len, 0, info);
               g_message("Parallax Occlusion mapping program failed to link:\n%s\n",
                         info);
               g_free(info);
               glDeleteObjectARB(prog);
               prog = 0;
            }
         }

         programs[BUMPMAP_POM] = prog;
      }
      else
         programs[BUMPMAP_POM] = 0;

      if(max_instructions >= 200 && max_indirections >= 100)
      {
         prog = glCreateProgramObjectARB();
         glAttachObjectARB(prog, vert_shader);

         frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
         glShaderSourceARB(frag_shader, 1, &relief_frag_source, 0);
         glCompileShaderARB(frag_shader);
         glGetObjectParameterivARB(frag_shader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
         if(res)
            glAttachObjectARB(prog, frag_shader);
         else
         {
            glGetObjectParameterivARB(frag_shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
            info = g_malloc(len + 1);
            glGetInfoLogARB(frag_shader, len, 0, info);
            g_message("Relief mapping fragment shader failed to compile:\n%s\n",
                      info);
            g_free(info);
            glDeleteObjectARB(prog);
            prog = 0;
         }
         glDeleteObjectARB(frag_shader);

         if(prog)
         {
            glLinkProgramARB(prog);
            glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &res);

            if(!res)
            {
               glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
               info = g_malloc(len + 1);
               glGetInfoLogARB(prog, len, 0, info);
               g_message("Relief mapping program failed to link:\n%s\n", info);
               g_free(info);
               glDeleteObjectARB(prog);
               prog = 0;
            }
         }

         programs[BUMPMAP_RELIEF] = prog;
      }
      else
         programs[BUMPMAP_RELIEF] = 0;

      glDeleteObjectARB(vert_shader);

      if(programs[BUMPMAP_NORMAL])
      {
         glUseProgramObjectARB(programs[BUMPMAP_NORMAL]);
         loc = glGetUniformLocationARB(programs[BUMPMAP_NORMAL], "sNormal");
         glUniform1iARB(loc, 0);
         loc = glGetUniformLocationARB(programs[BUMPMAP_NORMAL], "sDiffuse");
         glUniform1iARB(loc, 1);
         loc = glGetUniformLocationARB(programs[BUMPMAP_NORMAL], "sGloss");
         glUniform1iARB(loc, 2);
      }

      if(programs[BUMPMAP_PARALLAX])
      {
         glUseProgramObjectARB(programs[BUMPMAP_PARALLAX]);
         loc = glGetUniformLocationARB(programs[BUMPMAP_PARALLAX], "sNormal");
         glUniform1iARB(loc, 0);
         loc = glGetUniformLocationARB(programs[BUMPMAP_PARALLAX], "sDiffuse");
         glUniform1iARB(loc, 1);
         loc = glGetUniformLocationARB(programs[BUMPMAP_PARALLAX], "sGloss");
         glUniform1iARB(loc, 2);
      }

      if(programs[BUMPMAP_POM])
      {
         glUseProgramObjectARB(programs[BUMPMAP_POM]);
         loc = glGetUniformLocationARB(programs[BUMPMAP_POM], "sNormal");
         glUniform1iARB(loc, 0);
         loc = glGetUniformLocationARB(programs[BUMPMAP_POM], "sDiffuse");
         glUniform1iARB(loc, 1);
         loc = glGetUniformLocationARB(programs[BUMPMAP_POM], "sGloss");
         glUniform1iARB(loc, 2);
         loc = glGetUniformLocationARB(programs[BUMPMAP_POM], "depth_factor");
         glUniform1fARB(loc, depth_factor);
      }

      if(programs[BUMPMAP_RELIEF])
      {
         glUseProgramObjectARB(programs[BUMPMAP_RELIEF]);
         loc = glGetUniformLocationARB(programs[BUMPMAP_RELIEF], "sNormal");
         glUniform1iARB(loc, 0);
         loc = glGetUniformLocationARB(programs[BUMPMAP_RELIEF], "sDiffuse");
         glUniform1iARB(loc, 1);
         loc = glGetUniformLocationARB(programs[BUMPMAP_RELIEF], "sGloss");
         glUniform1iARB(loc, 2);
         loc = glGetUniformLocationARB(programs[BUMPMAP_RELIEF], "depth_factor");
         glUniform1fARB(loc, depth_factor);
      }

      glUseProgramObjectARB(0);

      for(i = 0; i < OBJECT_MAX; ++i)
      {
         glGenBuffersARB(1, &object_info[i].vbo);
         glBindBufferARB(GL_ARRAY_BUFFER_ARB, object_info[i].vbo);
         glBufferDataARB(GL_ARRAY_BUFFER_ARB,
                         object_info[i].num_verts * 16 * sizeof(float),
                         object_info[i].verts, GL_STATIC_DRAW_ARB);
      }

      glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

      menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(bumpmapping_opt));
      curr = gtk_container_get_children(GTK_CONTAINER(menu));
      for(i = 0; i < BUMPMAP_MAX && curr; ++i)
      {
         if(programs[i] == 0)
            gtk_widget_set_sensitive(GTK_WIDGET(curr->data), 0);
         curr = curr->next;
      }
   }
   else
   {
      gtk_widget_set_sensitive(gloss_opt, 0);
      gtk_widget_set_sensitive(bumpmapping_opt, 0);
      gtk_widget_set_sensitive(specular_check, 0);
      gtk_widget_set_sensitive(specular_exp_range, 0);
      gtk_widget_set_sensitive(ambient_color_btn, 0);
      gtk_widget_set_sensitive(diffuse_color_btn, 0);
      gtk_widget_set_sensitive(specular_color_btn, 0);
   }

   object_rot[0] = object_rot[1] = object_rot[2] = 0;
   light_rot[0] = light_rot[1] = light_rot[2] = 0;
   scene_rot[0] = scene_rot[1] = scene_rot[2] = 0;
   zoom = 2;

   gdk_gl_drawable_gl_end(gldrawable);
}

static void draw_object(int obj, vec3 l, matrix m)
{
   const int vsize = 16 * sizeof(float);
   int i;
   vec3 c, t, b, n;
   vec2 uv;
   float *verts;
   unsigned short *indices;

   if(obj < 0 || obj >= OBJECT_MAX) return;

   if(has_glsl)
   {
      glBindBufferARB(GL_ARRAY_BUFFER_ARB, object_info[obj].vbo);

#define OFFSET(x) ((void*)((x) * sizeof(float)))

      glVertexPointer(4, GL_FLOAT, vsize, OFFSET(0));
      glNormalPointer(GL_FLOAT, vsize, OFFSET(12));
      glClientActiveTexture(GL_TEXTURE4);
      glTexCoordPointer(3, GL_FLOAT, vsize, OFFSET(9));
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glClientActiveTexture(GL_TEXTURE3);
      glTexCoordPointer(3, GL_FLOAT, vsize, OFFSET(6));
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glClientActiveTexture(GL_TEXTURE0);
      glTexCoordPointer(2, GL_FLOAT, vsize, OFFSET(4));
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_NORMAL_ARRAY);

#undef OFFSET

      glDrawElements(GL_TRIANGLES, object_info[obj].num_indices,
                     GL_UNSIGNED_SHORT, object_info[obj].indices);

      glDisableClientState(GL_VERTEX_ARRAY);
      glDisableClientState(GL_NORMAL_ARRAY);
      glClientActiveTexture(GL_TEXTURE4);
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glClientActiveTexture(GL_TEXTURE3);
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glClientActiveTexture(GL_TEXTURE0);
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);

      glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   }
   else
   {
      verts = object_info[obj].verts;
      indices = object_info[obj].indices;

      glBegin(GL_TRIANGLES);
      for(i = 0; i < object_info[obj].num_indices; ++i)
      {
         vec3_copy(t, &verts[16 * indices[i] +  6]);
         vec3_copy(b, &verts[16 * indices[i] +  9]);
         vec3_copy(n, &verts[16 * indices[i] + 12]);
         mat_mult_vec(t, m);
         mat_mult_vec(b, m);
         mat_mult_vec(n, m);
         c[0] = (l[0] * t[0] + l[1] * t[1] + l[2] * t[2]);
         c[1] = (l[0] * b[0] + l[1] * b[1] + l[2] * b[2]);
         c[2] = (l[0] * n[0] + l[1] * n[1] + l[2] * n[2]);
         vec3_normalize(c, c);
         c[0] = c[0] * 0.5f + 0.5f;
         c[1] = c[1] * 0.5f + 0.5f;
         c[2] = c[2] * 0.5f + 0.5f;

         uv[0] = verts[16 * indices[i] + 4] * uvscale[0];
         uv[1] = verts[16 * indices[i] + 5] * uvscale[1];

         glColor3fv(c);
         glNormal3fv(&verts[16 * indices[i] + 12]);
         glMultiTexCoord2fv(GL_TEXTURE0, uv);
         glMultiTexCoord2fv(GL_TEXTURE1, uv);
         if(num_mtus > 2)
            glMultiTexCoord2fv(GL_TEXTURE2, uv);
         glVertex3fv(&verts[16 * indices[i]]);
      }
      glEnd();
   }
}

static gint expose(GtkWidget *widget, GdkEventExpose *event)
{
   matrix m;
   vec3 l;
   vec4 qx, qy, qz, qt, qrot;
   int loc;
   GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
   GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
   GLhandleARB prog = 0;

   if(event->count > 0) return(1);

   if(!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
      return(1);

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   if(_gl_error)
   {
      gdk_gl_drawable_swap_buffers(gldrawable);
      gdk_gl_drawable_gl_end(gldrawable);
      return(1);
   }

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glRotatef(scene_rot[0], 1, 0, 0);
   glRotatef(scene_rot[1], 0, 1, 0);
   glRotatef(scene_rot[2], 0, 0, 1);
   glTranslatef(0, 0, -zoom);
   glRotatef(object_rot[0], 1, 0, 0);
   glRotatef(object_rot[1], 0, 1, 0);
   glRotatef(object_rot[2], 0, 0, 1);

   glGetFloatv(GL_MODELVIEW_MATRIX, m);
   mat_invert(m);
   mat_transpose(m);

   quat_ident(qx);
   quat_ident(qy);
   quat_ident(qz);
   quat_rotate(qx, -light_rot[0], 1, 0, 0);
   quat_rotate(qy, -light_rot[1], 0, 1, 0);
   quat_rotate(qz, -light_rot[2], 0, 0, 1);
   quat_mul(qt, qx, qy);
   quat_mul(qrot, qt, qz);
   vec4_normalize(qrot, qrot);
   quat_get_direction(l, qrot);

   if(has_glsl)
   {
      prog = programs[bumpmapping];
      glUseProgramObjectARB(prog);
      loc = glGetUniformLocationARB(prog, "specular");
      glUniform1iARB(loc, specular);
      loc = glGetUniformLocationARB(prog, "ambient_color");
      glUniform3fvARB(loc, 1, ambient_color);
      loc = glGetUniformLocationARB(prog, "diffuse_color");
      glUniform3fvARB(loc, 1, diffuse_color);
      loc = glGetUniformLocationARB(prog, "specular_color");
      glUniform3fvARB(loc, 1, specular_color);
      loc = glGetUniformLocationARB(prog, "specular_exp");
      glUniform1fARB(loc, specular_exp);
      loc = glGetUniformLocationARB(prog, "lightDir");
      glUniform3fvARB(loc, 1, l);
      loc = glGetUniformLocationARB(prog, "uvscale");
      glUniform2fvARB(loc, 1, uvscale);
   }

   draw_object(object_type, l, m);

   if(has_glsl)
      glUseProgramObjectARB(0);

   gdk_gl_drawable_swap_buffers(gldrawable);
   gdk_gl_drawable_gl_end(gldrawable);

   return(1);
}

static gint configure(GtkWidget *widget, GdkEventConfigure *event)
{
   GdkGLContext *glcontext;
   GdkGLDrawable *gldrawable;
   int w, h;

   g_return_val_if_fail(widget && event, FALSE);

   glcontext = gtk_widget_get_gl_context(widget);
   gldrawable = gtk_widget_get_gl_drawable(widget);

   if(!gdk_gl_drawable_gl_begin(gldrawable,glcontext))
      return(1);

   w = widget->allocation.width;
   h = widget->allocation.height;

   glViewport(0, 0, w, h);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(60, (float)w / (float)h, 0.1f, 100);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gdk_gl_drawable_gl_end(gldrawable);

   return(1);
}

static gint button_press(GtkWidget *widget, GdkEventButton *event)
{
   mx = event->x;
   my = event->y;
   return(1);
}

static gint motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
   int x, y;
   float dx, dy;
   float *rot;
   GdkModifierType state;

   if(event->is_hint)
   {
#ifndef WIN32
      gdk_window_get_pointer(event->window, &x, &y, &state);
#endif
   }
   else
   {
      x = event->x;
      y = event->y;
      state = event->state;
   }

   dx = -0.25f * (float)(mx - x);
   dy = -0.25f * (float)(my - y);

   rot = object_rot;
   if(rotate_type == ROTATE_LIGHT)
      rot = light_rot;
   else if(rotate_type == ROTATE_SCENE)
      rot = scene_rot;

   if(state & GDK_BUTTON1_MASK)
   {
      rot[1] += cosf(rot[0] / 180.0f * M_PI) * dx;
      rot[2] -= sinf(rot[0] / 180.0f * M_PI) * dx;
      rot[0] += dy;
   }
   else if(state & GDK_BUTTON3_MASK)
   {
      zoom += (-dy * 0.2f);
   }

   mx = x;
   my = y;

   gtk_widget_queue_draw(widget);

   return(1);
}

static void window_destroy(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(glarea);
   _active = 0;
}

static void get_nearest_pot(int w, int h, int *w_pot, int *h_pot)
{
   int n, next_pot, prev_pot, d1, d2;

   if(!IS_POT(w))
   {
      next_pot = 1;
      for(n = 1; n <= 12; ++n)
      {
         prev_pot = next_pot;
         next_pot = 1 << n;
         if(next_pot >= w) break;
      }

      if(next_pot < w)
         *w_pot = next_pot;
      else
      {
         d1 = w - prev_pot;
         d2 = next_pot - w;
         if(d1 < d2)
            *w_pot = prev_pot;
         else
            *w_pot = next_pot;
      }
   }
   else
      *w_pot = w;

   if(!IS_POT(h))
   {
      next_pot = 1;
      for(n = 1; n <= 12; ++n)
      {
         prev_pot = next_pot;
         next_pot = 1 << n;
         if(next_pot >= h) break;
      }

      if(next_pot < h)
         *h_pot = next_pot;
      else
      {
         d1 = h - prev_pot;
         d2 = next_pot - h;
         if(d1 < d2)
            *h_pot = prev_pot;
         else
            *h_pot = next_pot;
      }
   }
   else
      *h_pot = h;
}

static void diffusemap_callback(gint32 id, gpointer data)
{
   GimpDrawable *drawable;
   int w, h, bpp, mipw, miph, n;
   int w_pot, h_pot;
   unsigned char *pixels, *tmp, *mip;
   GimpPixelRgn src_rgn;
   GLenum type = 0;

   if(_gl_error) return;

   if(id == normalmap_drawable_id)
   {
      if(white_tex != 0)
      {
         glActiveTexture(GL_TEXTURE1);
         glBindTexture(GL_TEXTURE_2D, white_tex);
      }
      gtk_widget_queue_draw(glarea);
      return;
   }

   drawable = gimp_drawable_get(id);

   w = drawable->width;
   h = drawable->height;
   bpp = drawable->bpp;

   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_RGB;             break;
      case 4: type = GL_RGBA;            break;
   }

   pixels = g_malloc(w * h * bpp);
   gimp_pixel_rgn_init(&src_rgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_get_rect(&src_rgn, pixels, 0, 0, w, h);

   if(!has_npot && !(IS_POT(w) && IS_POT(h)))
   {
      get_nearest_pot(w, h, &w_pot, &h_pot);
      tmp = g_malloc(h_pot * w_pot * bpp);
      scale_pixels(tmp, w_pot, h_pot, pixels, w, h, bpp);
      g_free(pixels);
      pixels = tmp;
      w = w_pot;
      h = h_pot;
   }

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, diffuse_tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   if(has_aniso)
      glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
   if(has_generate_mipmap)
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, type, w, h, 0,
                type, GL_UNSIGNED_BYTE, pixels);

   if(!has_generate_mipmap)
   {
      mipw = w;
      miph = h;
      n = 0;
      while((mipw != 1) && (miph != 1))
      {
         if(mipw > 1) mipw >>= 1;
         if(miph > 1) miph >>= 1;
         ++n;
         mip = g_malloc(mipw * miph * bpp);
         scale_pixels(mip, mipw, miph, pixels, w, h, bpp);
         glTexImage2D(GL_TEXTURE_2D, n, bpp, mipw, miph, 0,
                      (bpp == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                      mip);
         g_free(mip);
      }
   }

   g_free(pixels);

   gimp_drawable_detach(drawable);

   gtk_widget_queue_draw(glarea);
}

static void glossmap_callback(gint32 id, gpointer data)
{
   GimpDrawable *drawable;
   int w, h, bpp, mipw, miph, n;
   int w_pot, h_pot;
   unsigned char *pixels, *tmp, *mip;
   GimpPixelRgn src_rgn;
   GLenum type = 0;

   if(_gl_error) return;
   if(num_mtus < 3) return;

   if(id == normalmap_drawable_id)
   {
      if(white_tex != 0)
      {
         glActiveTexture(GL_TEXTURE2);
         glBindTexture(GL_TEXTURE_2D, white_tex);
      }
      gtk_widget_queue_draw(glarea);
      return;
   }

   drawable = gimp_drawable_get(id);

   w = drawable->width;
   h = drawable->height;
   bpp = drawable->bpp;

   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_RGB;             break;
      case 4: type = GL_RGBA;            break;
   }

   pixels = g_malloc(w * h * bpp);
   gimp_pixel_rgn_init(&src_rgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_get_rect(&src_rgn, pixels, 0, 0, w, h);

   if(!has_npot && !(IS_POT(w) && IS_POT(h)))
   {
      get_nearest_pot(w, h, &w_pot, &h_pot);
      tmp = g_malloc(h_pot * w_pot * bpp);
      scale_pixels(tmp, w_pot, h_pot, pixels, w, h, bpp);
      g_free(pixels);
      pixels = tmp;
      w = w_pot;
      h = h_pot;
   }

   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, gloss_tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   if(has_aniso)
      glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
   if(has_generate_mipmap)
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, type, w, h, 0,
                type, GL_UNSIGNED_BYTE, pixels);

   if(!has_generate_mipmap)
   {
      mipw = w;
      miph = h;
      n = 0;
      while((mipw != 1) && (miph != 1))
      {
         if(mipw > 1) mipw >>= 1;
         if(miph > 1) miph >>= 1;
         ++n;
         mip = g_malloc(mipw * miph * bpp);
         scale_pixels(mip, mipw, miph, pixels, w, h, bpp);
         glTexImage2D(GL_TEXTURE_2D, n, bpp, mipw, miph, 0,
                      (bpp == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                      mip);
         g_free(mip);
      }
   }

   g_free(pixels);

   gimp_drawable_detach(drawable);

   gtk_widget_queue_draw(glarea);
}

static void object_selected(GtkWidget *widget, gpointer data)
{
   object_type = (int)((size_t)data);
   gtk_widget_queue_draw(glarea);
}

static void bumpmapping_clicked(GtkWidget *widget, gpointer data)
{
   bumpmapping = (int)((size_t)data);
   gtk_widget_queue_draw(glarea);
}

static void toggle_fullscreen(GtkWidget *widget, gpointer data)
{
   fullscreen = !fullscreen;
   if(fullscreen)
   {
      gtk_window_fullscreen(GTK_WINDOW(window));
      gtk_widget_hide(controls_table);
   }
   else
   {
      gtk_window_unfullscreen(GTK_WINDOW(window));
      gtk_widget_show(controls_table);
   }
}

static void toggle_clicked(GtkWidget *widget, gpointer data)
{
   *((int*)data) = !(*((int*)data));
   gtk_widget_queue_draw(glarea);
}

static void specular_exp_changed(GtkWidget *widget, gpointer data)
{
   specular_exp = gtk_range_get_value(GTK_RANGE(widget));
   gtk_widget_queue_draw(glarea);
}

static void color_changed(GtkWidget *widget, gpointer data)
{
   float *c = (float*)data;
   GimpRGB color;

   gimp_color_button_get_color(GIMP_COLOR_BUTTON(widget), &color);
   c[0] = color.r;
   c[1] = color.g;
   c[2] = color.b;

   gtk_widget_queue_draw(glarea);
}

static void rotate_type_toggled(GtkWidget *widget, gpointer data)
{
   if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)))
      rotate_type = (int)((size_t)data);
}

static void uvscale_changed(GtkWidget *widget, gpointer data)
{
   int n = (int)((size_t)data);
   float v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
   GtkWidget *btn = g_object_get_data(G_OBJECT(widget), "chain");

   uvscale[n] = v;
   if(gimp_chain_button_get_active(GIMP_CHAIN_BUTTON(btn)))
   {
      if(n == 0)
      {
         uvscale[1] = v;
         gtk_spin_button_set_value(GTK_SPIN_BUTTON(uvscale_spin2), v);
      }
      else
      {
         uvscale[0] = v;
         gtk_spin_button_set_value(GTK_SPIN_BUTTON(uvscale_spin1), v);
      }
   }

   gtk_widget_queue_draw(glarea);
}

static void reset_view_clicked(GtkWidget *widget, gpointer data)
{
   GimpRGB c;

   object_rot[0] = object_rot[1] = object_rot[2] = 0;
   light_rot[0] = light_rot[1] = light_rot[2] = 0;
   scene_rot[0] = scene_rot[1] = scene_rot[2] = 0;
   zoom = 2;

   specular_exp = 32.0f;
   ambient_color[0] = ambient_color[1] = ambient_color[2] = 0.2f;
   diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0f;
   specular_color[0] = specular_color[1] = specular_color[2] = 1.0f;
   uvscale[0] = uvscale[1] = 1;

   gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(rotate_obj_btn), 1);
   gtk_option_menu_set_history(GTK_OPTION_MENU(object_opt), 0);
   gtk_option_menu_set_history(GTK_OPTION_MENU(bumpmapping_opt), 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(specular_check), 0);
   gtk_range_set_value(GTK_RANGE(specular_exp_range), specular_exp);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(uvscale_spin1), uvscale[0]);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(uvscale_spin2), uvscale[1]);

   gimp_rgb_set(&c, ambient_color[0], ambient_color[1], ambient_color[2]);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(ambient_color_btn), &c);
   gimp_rgb_set(&c, diffuse_color[0], diffuse_color[1], diffuse_color[2]);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(diffuse_color_btn), &c);
   gimp_rgb_set(&c, specular_color[0], specular_color[1], specular_color[2]);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(specular_color_btn), &c);

   bumpmapping = 0;
   specular = 0;
   rotate_type = ROTATE_OBJECT;
   object_type = OBJECT_QUAD;

   gtk_widget_queue_draw(glarea);
}

void show_3D_preview(GimpDrawable *drawable)
{
   int i;
   GtkWidget *vbox;
   GtkWidget *table, *table2;
   GtkWidget *opt;
   GtkWidget *menu;
   GtkWidget *menuitem;
   GtkWidget *check;
   GtkWidget *btn;
   GtkWidget *hscale;
   GtkWidget *label;
   GtkObject *adj;
   GtkWidget *spin;
   GtkWidget *toolbar;
   GtkToolItem *toolbtn;
   GtkTooltips *tooltips;
   GtkWidget *icon;
   GdkPixbuf *pixbuf;
   GSList *group = 0;
   GdkGLConfig *glconfig;
   GimpRGB color;
   const char *object_strings[OBJECT_MAX] =
   {
      "Quad", "Cube", "Sphere", "Torus", "Teapot"
   };
   const char *bumpmap_strings[BUMPMAP_MAX] =
   {
      "Normal", "Parallax", "Parallax Occlusion", "Relief"
   };

   bumpmapping = 0;
   specular = 0;
   specular_exp = 32.0f;
   ambient_color[0] = ambient_color[1] = ambient_color[2] = 0.2f;
   diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0f;
   specular_color[0] = specular_color[1] = specular_color[2] = 1.0f;
   uvscale[0] = uvscale[1] = 1;

   if(_active) return;

   normalmap_drawable_id = drawable->drawable_id;

   glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGBA |
                                        GDK_GL_MODE_DEPTH |
                                        GDK_GL_MODE_DOUBLE);
   if(glconfig == 0)
   {
      g_message("Could not initialize OpenGL context!");
      return;
   }

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(window), "Normalmap - 3D Preview");
   gtk_container_set_resize_mode(GTK_CONTAINER(window), GTK_RESIZE_QUEUE);
   gtk_container_set_reallocate_redraws(GTK_CONTAINER(window), TRUE);
   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(window_destroy), 0);

   vbox = gtk_vbox_new(0, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);
   gtk_widget_show(vbox);

   tooltips = gtk_tooltips_new();

   toolbar = gtk_toolbar_new();
   gtk_widget_show(toolbar);
   gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
   gtk_box_pack_start(GTK_BOX(vbox), toolbar, 0, 0, 0);

   group = NULL;

   toolbtn = gtk_radio_tool_button_new(group);
   rotate_obj_btn = (GtkWidget*)toolbtn;
   gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolbtn), "");
   pixbuf = gdk_pixbuf_new_from_xpm_data(object_xpm);
   icon = gtk_image_new_from_pixbuf(pixbuf);
   gtk_widget_show(icon);
   gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toolbtn), icon);
   gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(toolbtn), tooltips, "Rotate object", 0);
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));
   gtk_signal_connect(GTK_OBJECT(toolbtn), "toggled",
                      GTK_SIGNAL_FUNC(rotate_type_toggled),
                      (gpointer)ROTATE_OBJECT);
   group = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(toolbtn));

   toolbtn = gtk_radio_tool_button_new(group);
   gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolbtn), "");
   pixbuf = gdk_pixbuf_new_from_xpm_data(light_xpm);
   icon = gtk_image_new_from_pixbuf(pixbuf);
   gtk_widget_show(icon);
   gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toolbtn), icon);
   gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(toolbtn), tooltips, "Rotate light", 0);
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));
   gtk_signal_connect(GTK_OBJECT(toolbtn), "toggled",
                      GTK_SIGNAL_FUNC(rotate_type_toggled),
                      (gpointer)ROTATE_LIGHT);
   group = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(toolbtn));

   toolbtn = gtk_radio_tool_button_new(group);
   gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolbtn), "");
   pixbuf = gdk_pixbuf_new_from_xpm_data(scene_xpm);
   icon = gtk_image_new_from_pixbuf(pixbuf);
   gtk_widget_show(icon);
   gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toolbtn), icon);
   gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(toolbtn), tooltips, "Rotate scene", 0);
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));
   gtk_signal_connect(GTK_OBJECT(toolbtn), "toggled",
                      GTK_SIGNAL_FUNC(rotate_type_toggled),
                      (gpointer)ROTATE_SCENE);
   group = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(toolbtn));

   toolbtn = gtk_separator_tool_item_new();
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));

   toolbtn = gtk_toggle_tool_button_new();
   gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolbtn), "");
   pixbuf = gdk_pixbuf_new_from_xpm_data(full_xpm);
   icon = gtk_image_new_from_pixbuf(pixbuf);
   gtk_widget_show(icon);
   gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toolbtn), icon);
   gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(toolbtn), tooltips, "Toggle fullscreen", 0);
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));
   gtk_signal_connect(GTK_OBJECT(toolbtn), "clicked",
                      GTK_SIGNAL_FUNC(toggle_fullscreen), 0);

   toolbtn = gtk_separator_tool_item_new();
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));

   toolbtn = gtk_tool_item_new();
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));
   label = gtk_label_new("Object type: ");
   gtk_widget_show(label);
   gtk_container_add(GTK_CONTAINER(toolbtn), label);

   toolbtn = gtk_tool_item_new();
   gtk_widget_show(GTK_WIDGET(toolbtn));
   gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(toolbtn));

   opt = gtk_option_menu_new();
   object_opt = opt;
   gtk_widget_show(opt);
   gtk_container_add(GTK_CONTAINER(toolbtn), opt);

   menu = gtk_menu_new();

   for(i = 0; i < OBJECT_MAX; ++i)
   {
      menuitem = gtk_menu_item_new_with_label(object_strings[i]);
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         GTK_SIGNAL_FUNC(object_selected),
                         (gpointer)((size_t)i));
      gtk_widget_show(menuitem);
      gtk_menu_append(GTK_MENU(menu), menuitem);
   }

	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);

   glarea = gtk_drawing_area_new();
   gtk_widget_set_usize(glarea, 500, 300);
   gtk_widget_set_gl_capability(glarea, glconfig, 0, 1, GDK_GL_RGBA_TYPE);
   gtk_widget_set_events(glarea, GDK_EXPOSURE_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_POINTER_MOTION_MASK);
   gtk_signal_connect(GTK_OBJECT(glarea), "realize",
                      GTK_SIGNAL_FUNC(init), 0);
   gtk_signal_connect(GTK_OBJECT(glarea), "expose_event",
                      GTK_SIGNAL_FUNC(expose), 0);
   gtk_signal_connect(GTK_OBJECT(glarea), "motion_notify_event",
                      GTK_SIGNAL_FUNC(motion_notify), 0);
   gtk_signal_connect(GTK_OBJECT(glarea), "button_press_event",
                      GTK_SIGNAL_FUNC(button_press), 0);
   gtk_signal_connect(GTK_OBJECT(glarea), "configure_event",
                      GTK_SIGNAL_FUNC(configure), 0);

   gtk_box_pack_start(GTK_BOX(vbox), glarea, 1, 1, 0);

   table = gtk_table_new(11, 2, 0);
   controls_table = table;
   gtk_container_set_border_width(GTK_CONTAINER(table), 5);
   gtk_table_set_col_spacings(GTK_TABLE(table), 5);
   gtk_table_set_row_spacings(GTK_TABLE(table), 5);
   gtk_box_pack_start(GTK_BOX(vbox), table, 0, 0, 0);
   gtk_widget_show(table);

   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   menu = gimp_drawable_menu_new(0, diffusemap_callback, 0, normalmap_drawable_id);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 0, "Diffuse map:", 0, 0.5,
                             opt, 1, 0);

   opt = gtk_option_menu_new();
   gloss_opt = opt;
   gtk_widget_show(opt);
   menu = gimp_drawable_menu_new(0, glossmap_callback, 0, normalmap_drawable_id);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 1, "Gloss map:", 0, 0.5,
                             opt, 1, 0);

   opt = gtk_option_menu_new();
   bumpmapping_opt = opt;
   gtk_widget_show(opt);

   gimp_table_attach_aligned(GTK_TABLE(table), 0, 2,
                             "Bump mapping:", 0, 0.5,
                             opt, 1, 0);

   menu = gtk_menu_new();

   for(i = 0; i < BUMPMAP_MAX; ++i)
   {
      menuitem = gtk_menu_item_new_with_label(bumpmap_strings[i]);
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         GTK_SIGNAL_FUNC(bumpmapping_clicked),
                         (gpointer)((size_t)i));
      gtk_widget_show(menuitem);
      gtk_menu_append(GTK_MENU(menu), menuitem);
   }

	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);

   check = gtk_check_button_new_with_label("Specular lighting");
   specular_check = check;
   gtk_widget_show(check);
   gtk_table_attach(GTK_TABLE(table), check, 1, 2, 3, 4,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &specular);

   specular_exp_range = hscale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(32, 0, 256, 1, 8, 0)));
   gtk_widget_show(hscale);
   gtk_scale_set_value_pos(GTK_SCALE(hscale), GTK_POS_RIGHT);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 4, "Specular exponent:", 0, 0.5,
                             hscale, 1, 0);
   gtk_signal_connect(GTK_OBJECT(hscale), "value_changed",
                      GTK_SIGNAL_FUNC(specular_exp_changed), 0);


   gimp_rgb_set(&color, ambient_color[0], ambient_color[1], ambient_color[2]);
   ambient_color_btn = btn = gimp_color_button_new("Ambient color", 30, 15, &color, GIMP_COLOR_AREA_FLAT);
   gtk_widget_show(btn);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(btn), &color);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 5, "Ambient color:", 0, 0.5,
                             btn, 1, 0);
   gtk_signal_connect(GTK_OBJECT(btn), "color_changed",
                      GTK_SIGNAL_FUNC(color_changed), (gpointer)ambient_color);

   gimp_rgb_set(&color, diffuse_color[0], diffuse_color[1], diffuse_color[2]);
   diffuse_color_btn = btn = gimp_color_button_new("Diffuse color", 30, 15, &color, GIMP_COLOR_AREA_FLAT);
   gtk_widget_show(btn);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(btn), &color);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 6, "Diffuse color:", 0, 0.5,
                             btn, 1, 0);
   gtk_signal_connect(GTK_OBJECT(btn), "color_changed",
                      GTK_SIGNAL_FUNC(color_changed), (gpointer)diffuse_color);

   gimp_rgb_set(&color, specular_color[0], specular_color[1], specular_color[2]);
   specular_color_btn = btn = gimp_color_button_new("Specular color", 30, 15, &color, GIMP_COLOR_AREA_FLAT);
   gtk_widget_show(btn);
   gimp_color_button_set_color(GIMP_COLOR_BUTTON(btn), &color);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 7, "Specular color:", 0, 0.5,
                             btn, 1, 0);
   gtk_signal_connect(GTK_OBJECT(btn), "color_changed",
                      GTK_SIGNAL_FUNC(color_changed), (gpointer)specular_color);

   table2 = gtk_table_new(2, 2, 0);
   gtk_widget_show(table2);
   gtk_table_set_col_spacings(GTK_TABLE(table2), 5);
   gtk_table_set_row_spacings(GTK_TABLE(table2), 5);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 8, "UV scale:", 0, 0.5,
                             table2, 1, 0);

   adj = gtk_adjustment_new(1, 0, 1000, 1, 10, 10);
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 4);
   uvscale_spin1 = spin;
   gtk_widget_show(spin);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(uvscale_changed), (gpointer)0);
   gtk_table_attach(GTK_TABLE(table2), spin, 0, 1, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);

   adj = gtk_adjustment_new(1, 0, 1000, 1, 10, 10);
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 4);
   uvscale_spin2 = spin;
   gtk_widget_show(spin);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(uvscale_changed), (gpointer)1);
   gtk_table_attach(GTK_TABLE(table2), spin, 0, 1, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);

   btn = gimp_chain_button_new(GIMP_CHAIN_RIGHT);
   gtk_widget_show(btn);
   gimp_chain_button_set_active(GIMP_CHAIN_BUTTON(btn), 1);
   gtk_table_attach(GTK_TABLE(table2), btn, 1, 2, 0, 2,
                    (GtkAttachOptions)(0),
                    (GtkAttachOptions)(0), 0, 0);

   g_object_set_data(G_OBJECT(uvscale_spin1), "chain", btn);
   g_object_set_data(G_OBJECT(uvscale_spin2), "chain", btn);

   btn = gtk_button_new_with_label("Reset view");
   gtk_widget_show(btn);
   gtk_table_attach(GTK_TABLE(table), btn, 0, 2, 10, 11,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_signal_connect(GTK_OBJECT(btn), "clicked",
                      GTK_SIGNAL_FUNC(reset_view_clicked), 0);

   gtk_widget_show(glarea);
   gtk_widget_show(window);

   _active = 1;
}

void destroy_3D_preview(void)
{
   if(!_active) return;
   gtk_widget_destroy(window);
   _active = 0;
}

void update_3D_preview(unsigned int w, unsigned int h, int bpp,
                       unsigned char *image)
{
   int w_pot, h_pot, mipw, miph, n;
   unsigned char *pixels = image;
   unsigned char *mip;

   if(!_active) return;
   if(_gl_error) return;

   if(!has_npot && !(IS_POT(w) && IS_POT(h)))
   {
      get_nearest_pot(w, h, &w_pot, &h_pot);
      pixels = g_malloc(h_pot * w_pot * bpp);
      scale_pixels(pixels, w_pot, h_pot, image, w, h, bpp);
      w = w_pot;
      h = h_pot;
   }

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, normal_tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   if(has_aniso)
      glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
   if(has_generate_mipmap)
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, bpp, w, h, 0,
                (bpp == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

   if(!has_generate_mipmap)
   {
      mipw = w;
      miph = h;
      n = 0;
      while((mipw != 1) && (miph != 1))
      {
         if(mipw > 1) mipw >>= 1;
         if(miph > 1) miph >>= 1;
         ++n;
         mip = g_malloc(mipw * miph * bpp);
         scale_pixels(mip, mipw, miph, pixels, w, h, bpp);
         glTexImage2D(GL_TEXTURE_2D, n, bpp, mipw, miph, 0,
                      (bpp == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                      mip);
         g_free(mip);
      }
   }

   if(pixels != image)
      g_free(pixels);

   gtk_widget_queue_draw(glarea);
}

int is_3D_preview_active(void)
{
   return(_active);
}
