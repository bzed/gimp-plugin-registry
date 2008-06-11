/**********************************************************************
  icc_colorspace.h
  Copyright(C) 2007 Y.Yamakawa
**********************************************************************/

#ifndef ICC_COLORSPACE_H
#define ICC_COLORSPACE_H

#include "lcms.h"

#ifdef ENABLE_COLOR_MANAGEMENT
typedef GimpColorRenderingIntent ICCRenderingIntent;
#else
typedef gint ICCRenderingIntent;
#endif

typedef enum {
  ICC_COLORSPACE_NONE,
  ICC_COLORSPACE_ASSIGN,
  ICC_COLORSPACE_CONVERT
}  IccColorspaceFunction;

typedef enum {
  ICC_COLORSPACE_ASSIGN_NONE,
  ICC_COLORSPACE_ASSIGN_WORKSPACE,
  ICC_COLORSPACE_ASSIGN_PROFILE
}  IccColorspaceAssignMode;

typedef struct _AssignSettings
{
  IccColorspaceAssignMode mode;
} AssignSettings;

typedef struct _ConvertSettings
{
  gboolean use_workspace;
  ICCRenderingIntent intent;
  gboolean bpc;
  gboolean flatten;
} ConvertSettings;

typedef struct _IccColorspaceContext
{
  /* Settings */
  AssignSettings as;
  ConvertSettings cs;

  GimpRunMode run_mode;
  IccColorspaceFunction func;

  gchar *filename;
  //gchar *src_filename; // Source / devicelink profile

  //gint nAbstractProfiles; // <= 253
  //gchar **abstract_filenames; // Abstract profiles

  /* Core related */
  gint32 imageID;

  gchar *profile;
  gsize profileSize;

  gchar *workspaceProfile;
  gsize workspaceProfileSize;

  cmsHPROFILE hProfile;
  cmsHPROFILE hWorkspaceProfile;
  cmsHTRANSFORM hTransform;

  /* Progress bar */
  gdouble percentage;
  gdouble step;
} IccColorspaceContext;

#endif
