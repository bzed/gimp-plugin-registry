/**********************************************************************
  icc_colorspace.h
  Copyright(C) 2007-2010 Y.Yamakawa
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
  ICC_COLORSPACE_CONVERT,
  ICC_COLORSPACE_ABSTRACT
}  IccColorspaceFunction;

typedef enum {
  ICC_COLORSPACE_ASSIGN_NONE,
  ICC_COLORSPACE_ASSIGN_WORKSPACE,
  ICC_COLORSPACE_ASSIGN_PROFILE
}  IccColorspaceAssignMode;

typedef enum {
  ICC_COLORSPACE_TARGET_SELECTION,
  ICC_COLORSPACE_TARGET_LAYERS,
  ICC_COLORSPACE_TARGET_FLATTEN_IMAGE
}  IccColorspaceConvertTarget;

typedef struct _AssignSettings
{
  IccColorspaceAssignMode mode;
} AssignSettings;

typedef struct _ConvertSettings
{
  gboolean use_workspace;
  ICCRenderingIntent intent;
  IccColorspaceConvertTarget target;
  gboolean bpc;
  /*gboolean flatten;*/
} ConvertSettings;

typedef struct _AbstractSettings
{
  IccColorspaceConvertTarget target;
} AbstractSettings;

typedef struct _IccColorspaceContext
{
  /* Settings */
  AssignSettings as;
  ConvertSettings cs;
  AbstractSettings bs;

  GimpRunMode run_mode;
  IccColorspaceFunction func;

  gchar *filename; /* 変換先・割り当てプロファイルのパス */
  gchar *filenames[16]; /* 間に挟むプロファイルのパス */
  gint n_filenames;

  /* Core related */
  gint32 imageID;
  GimpImageBaseType type;
  IccColorspaceConvertTarget target;

  gchar *profile; /* 変換先・割り当てプロファイルの実体 */
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
