#ifndef SEPARATE_H
#define SEPARATE_H

#include "lcms.h"

#define CMYKPROFILE "plug_in_separate_save/cmyk-profile"

#ifdef ENABLE_COLOR_MANAGEMENT
typedef GimpColorRenderingIntent SeparateRenderingIntent;
#else
typedef gint SeparateRenderingIntent;
#endif

#ifdef SEPARATE_SEPARATE
enum separate_function { SEP_NONE, SEP_DUOTONE, SEP_SEPARATE, SEP_FULL, SEP_LIGHT, SEP_PROOF, SEP_SAVE, SEP_LOAD };
#else
enum separate_function { SEP_NONE, SEP_DUOTONE, SEP_FULL, SEP_LIGHT, SEP_PROOF, SEP_SAVE, SEP_LOAD };
#endif

struct SeparateSettings
{
  gboolean preserveblack;
  gboolean overprintblack;
  gboolean profile;
  SeparateRenderingIntent intent;
  gboolean bpc;
#ifdef SEPARATE_SEPARATE
  gboolean composite;
#endif
};

struct ProofSettings
{
  SeparateRenderingIntent mode;
  gboolean profile;
};

struct SaveSettings
{
  gint8 embedprofile;
};

struct SeparateContext
{
  /* Settings */
  gchar *displayfilename;
  gchar *rgbfilename;
  gchar *cmykfilename;
  gchar *prooffilename;
  gchar *filename;
  struct SeparateSettings ss;
  struct ProofSettings ps;
  struct SaveSettings sas;

  /* Dialog private */
  GtkWidget *dialog;
  GtkWidget *rgbfileselector;
  GtkWidget *cmykfileselector;
  GtkWidget *filenamefileselector;
  gboolean dialogresult;
#ifdef SEPARATE_SEPARATE
  gboolean integrated;
#endif

  /* Core related */
  gint32 imageID;
  cmsHTRANSFORM hTransform;
  guchar *cmyktemp;
  guchar *destptr[4];
  int srcbpp;
  int bpp[4];
};

#endif
