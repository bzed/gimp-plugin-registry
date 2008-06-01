/* separate+ 0.5 - image processing plug-in for the Gimp
 *
 * Copyright (C) 2002-2004 Alastair Robinson (blackfive@fakenhamweb.co.uk),
 * Based on code by Andrew Kieschnick and Peter Kirchgessner
 * Modified by Yoshinori Yamakawa (yam@yellowmagic.info)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "separate.h"
#include "icon.h"

#include "platform.h"
#include "util.h"
#include "tiff.h"

#include "iccbutton.h"

/* Declare local functions.
 */
static void	query( void );
static void	run( const gchar *name,
                 gint nparams,
                 const GimpParam *param,
                 gint *nreturn_vals,
                 GimpParam **return_vals );

#ifdef ENABLE_COLOR_MANAGEMENT
static void      embed_cmyk_profile( gint32 image_id, gchar *filename );
#endif

static void      separate_full( GimpDrawable *drawable, GimpParam *values, struct SeparateContext *sc );
static void      separate_light( GimpDrawable *drawable, GimpParam *values, struct SeparateContext *sc );
static void      separate_proof( GimpDrawable *drawable, GimpParam *values, struct SeparateContext *sc );
static void      separate_duotone( GimpDrawable *drawable, GimpParam *values, struct SeparateContext *sc );

static void      callback_preserve_black_toggled( GtkWidget *toggleButton, gpointer data );

static gint      separate_dialog( struct SeparateContext *sc );
static gint      proof_dialog( struct SeparateContext *sc );
static gint      separate_save_dialog( struct SeparateContext *sc );

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()

static void
query (void)
{
  /* Arguments for CMYK Separation routines */

  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
    { GIMP_PDB_STRING, "input_profile", "Input ICC profile" },
    { GIMP_PDB_STRING, "output_profile", "Output ICC profile" },
    { GIMP_PDB_INT8, "preserve_black", "Preserve pure black (TRUE,FALSE)" },
    { GIMP_PDB_INT8, "overprint_black", "Overprint pure black (TRUE,FALSE)" },
    { GIMP_PDB_INT32, "rendering_intent", "Rendering intent (0-3)" },
    { GIMP_PDB_INT8, "use_bpc", "Use BPC algorithm (TRUE,FALSE)" },
    { GIMP_PDB_INT8, "option", "Use embedded source profile if possible (TRUE,FALSE)" }
#ifdef SEPARATE_SEPARATE
    ,{ GIMP_PDB_INT8, "pseudo_composite", "Make CMYK pseudo-composite (TRUE,FALSE)" }
#endif
  };
  static gint nargs = sizeof (args) / sizeof (args[0]);
  static GimpParamDef rargs[] =
  {
    //{ GIMP_PDB_STATUS , "status", "Success or failure"},
    { GIMP_PDB_IMAGE, "new_image", "Separated image" }
  };
  static gint nrargs = sizeof (rargs) / sizeof (rargs[0]);

  /* Arguments for Proofing routines */

  static GimpParamDef proofargs[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
    { GIMP_PDB_STRING, "display_profile", "Monitor(or workspace) profile" },
    { GIMP_PDB_STRING, "proofing_profile", "Proofing profile" },
    { GIMP_PDB_INT32, "mode", "0:Normal, 1:Black ink simulation, 2:Media white simulation" }
#ifdef ENABLE_COLOR_MANAGEMENT
    ,{ GIMP_PDB_INT8, "option", "Use attached proofing profile if possible (TRUE,FALSE)" }
#endif
  };
  static gint nproofargs = sizeof (proofargs) / sizeof (proofargs[0]);
  static GimpParamDef proofrargs[] =
  {
    //{ GIMP_PDB_STATUS , "status", "Success or failure"},
    { GIMP_PDB_IMAGE, "new_image", "Proof image" }
  };
  static gint nproofrargs = sizeof (proofrargs) / sizeof (proofrargs[0]);

  /* Arguments for CMYK TIFF saver */

  static GimpParamDef saveargs[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
    { GIMP_PDB_STRING, "filename", "Filename" }
#ifdef ENABLE_COLOR_MANAGEMENT
    ,{ GIMP_PDB_INT8, "embed_profile", "0:None, 1:CMYK profile, 2:Print simulation profile, 3:Own profile" }
#endif
  };
  static gint nsaveargs = sizeof(saveargs) / sizeof (saveargs[0]);
  /*static GimpParamDef saverargs[] =
  {
    { GIMP_PDB_STATUS , "status", "Success or failure"},
  };
  static gint nsaverargs = sizeof (saverargs) / sizeof (saverargs[0]);*/

  /* Arguments for DuoTone separation code */
  static GimpParamDef duotoneargs[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
  };
  static gint nduotoneargs = sizeof (duotoneargs) / sizeof (duotoneargs[0]);

#ifdef SEPARATE_SEPARATE
  gimp_install_procedure ("plug_in_separate_separate",
                          N_("Generate CMYK separations"),
                          N_("Separate performs CMYK colour separation of an image, into "
                             "the four layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2008",
                          N_("<Image>/Image/Separate/Separate"),
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs, nrargs,
                          args, rargs);
#endif

  gimp_install_procedure ("plug_in_separate_full",
                          "Separate_full",
                          N_("Separate performs CMYK colour separation of an RGB image, into "
                             "the alpha-channels of four coloured layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2008",
#ifdef SEPARATE_SEPARATE
                          "",
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs - 1, nrargs,
#else
                          "<Image>/Image/Separate/Separate (to Colour)",
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs, nrargs,
#endif
                          args, rargs);

  gimp_install_procedure ("plug_in_separate_light",
                          "Separate_light",
                          N_("Separate performs CMYK colour separation of an RGB image, into "
                             "four greyscale layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2008",
#ifdef SEPARATE_SEPARATE
                          "",
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs - 1, nrargs,
#else
                          "<Image>/Image/Separate/Separate (normal)",
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs, nrargs,
#endif
                          args, rargs);

  gimp_install_procedure ("plug_in_separate_proof",
                          N_( "Softproofing CMYK colour" ),
                          N_( "Separate proofs a CMYK colour separation, by transforming back "
                              "into RGB, with media-white simulation." ),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2008",
                          N_("<Image>/Image/Separate/Proof"),
                          "RGB*,GRAY*",
                          GIMP_PLUGIN,
                          nproofargs, nproofrargs,
                          proofargs, proofrargs);

  gimp_install_procedure ("plug_in_separate_duotone",
                          N_("Generate duotone separations"),
                          "Splits an image into Red and Black plates, mapped into a CMYK image.  "
                          "HACK Alert:  The Red plate occupies the Magenta channel of the CMYK image, "
                          "allowing extraction of spot colour with standard CMYK separation code...",
                          "Alastair Robinson",
                          "Alastair Robinson",
                          "2002",
                          N_("<Image>/Image/Separate/Duotone"),
                          "RGB*",
                          GIMP_PLUGIN,
                          nduotoneargs, nrargs,
                          duotoneargs, rargs);

  gimp_install_procedure ("plug_in_separate_save",
                          N_("Save separated image"),
                          N_("Save separated image in TIFF format."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2008",
                          N_("<Image>/Image/Separate/Save..."),
                          "RGB*,GRAY*",
                          GIMP_PLUGIN,
                          nsaveargs, 0,
                          saveargs, NULL);

#ifdef SEPARATE_SEPARATE
  gimp_plugin_icon_register( "plug_in_separate_separate" ,GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_cmyk );
#else
  gimp_plugin_icon_register( "plug_in_separate_full" ,GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_cmyk );
  gimp_plugin_icon_register( "plug_in_separate_light",GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_cmyk );
#endif
  gimp_plugin_icon_register( "plug_in_separate_duotone",GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_duotone );
#ifdef GIMP_STOCK_DISPLAY_FILTER_PROOF
  gimp_plugin_icon_register( "plug_in_separate_proof" ,GIMP_ICON_TYPE_STOCK_ID, GIMP_STOCK_DISPLAY_FILTER_PROOF );
#endif
  gimp_plugin_icon_register( "plug_in_separate_save" ,GIMP_ICON_TYPE_STOCK_ID, GTK_STOCK_SAVE_AS );

  gimp_plugin_domain_register (GETTEXT_PACKAGE, NULL);
}


static void run ( const gchar *name, gint nparams, const GimpParam *param,
                  gint *nreturn_vals, GimpParam **return_vals)
{
  static GimpParam values[3];
  GimpDrawable *drawable;
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  struct SeparateContext mysc;
  enum separate_function func = SEP_NONE;
  run_mode = param[0].data.d_int32;

#ifdef SEPARATE_SEPARATE
  if( strcmp( name, "plug_in_separate_separate" ) == 0 )
    func = SEP_SEPARATE;
#endif
  if( strcmp( name, "plug_in_separate_full" )== 0 )
    func = SEP_FULL;
  else if( strcmp( name, "plug_in_separate_light" ) == 0 )
    func = SEP_LIGHT;
  else if( strcmp( name, "plug_in_separate_proof" ) == 0 )
    func = SEP_PROOF;
  else if( strcmp( name, "plug_in_separate_save" ) == 0 )
    func = SEP_SAVE;
  else if( strcmp( name, "plug_in_separate_duotone" ) == 0 )
    func = SEP_DUOTONE;

  /* setup for localization */
  INIT_I18N ();

  cmsErrorAction( LCMS_ERROR_IGNORE );

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get( param[2].data.d_drawable );

  values[1].data.d_image = -1;

  separate_init_settings( &mysc, ( func != SEP_SAVE && func != SEP_DUOTONE && run_mode != GIMP_RUN_NONINTERACTIVE ) );
  mysc.imageID = gimp_drawable_get_image( param[2].data.d_drawable );//param[1].data.d_image;


  switch( func ) {
#ifdef SEPARATE_SEPARATE
  case SEP_SEPARATE:
#endif
  case SEP_FULL:
  case SEP_LIGHT:
  case SEP_PROOF:
    switch( run_mode ) {
    case GIMP_RUN_NONINTERACTIVE:
#ifdef SEPARATE_SEPARATE
      if( func == SEP_SEPARATE ) {
        if( nparams != 11 )
          status = GIMP_PDB_CALLING_ERROR;
      } else
#endif
#ifdef ENABLE_COLOR_MANAGEMENT
      if( nparams != ( func == SEP_PROOF ? 7 : 10 ) )
#else
      if( nparams != ( func == SEP_PROOF ? 6 : 10 ) )
#endif
        status = GIMP_PDB_CALLING_ERROR;
      if( status == GIMP_PDB_SUCCESS ) {
        /*	Collect the profile filenames */
        gchar *rgbprofile, *cmykprofile;
        rgbprofile = param[3].data.d_string;
        cmykprofile = param[4].data.d_string;
        if( func == SEP_PROOF ) {
          if( rgbprofile && strlen( rgbprofile ) ) {
            g_free( mysc.displayfilename );
            mysc.displayfilename = g_strdup( rgbprofile );
          }
          if( cmykprofile && strlen( cmykprofile ) ) {
            g_free( mysc.prooffilename );
            mysc.prooffilename = g_strdup( cmykprofile );
          }

          mysc.ps.mode = param[5].data.d_int32 == -1 ? mysc.ps.mode : param[5].data.d_int32;
#ifdef ENABLE_COLOR_MANAGEMENT
          mysc.ps.profile = param[6].data.d_int8;
#endif
        } else {
          if( rgbprofile && strlen( rgbprofile ) ) {
            g_free( mysc.rgbfilename );
            mysc.rgbfilename = g_strdup( rgbprofile );
          }
          if( cmykprofile && strlen( cmykprofile ) ) {
            g_free( mysc.cmykfilename );
            mysc.cmykfilename = g_strdup( cmykprofile );
          }

          mysc.ss.preserveblack = param[5].data.d_int8;
          mysc.ss.overprintblack = param[6].data.d_int8;
          mysc.ss.intent = param[7].data.d_int32 == -1 ? mysc.ss.intent : param[7].data.d_int32;
          mysc.ss.bpc = param[8].data.d_int8;
          mysc.ss.profile = param[9].data.d_int8;
        }
      }
      break;
    case GIMP_RUN_INTERACTIVE:
#ifdef SEPARATE_SEPARATE
      mysc.integrated = ( func == SEP_SEPARATE );
#endif
      if ( !( func == SEP_PROOF ? proof_dialog( &mysc ) : separate_dialog( &mysc ) ) )
        status = GIMP_PDB_EXECUTION_ERROR;
      break;
    case GIMP_RUN_WITH_LAST_VALS:
      break;
    default:
      break;
    }

    if( status == GIMP_PDB_SUCCESS ) {
      /*  Make sure that the drawable is RGB color  */

      switch( func ) {
#ifdef SEPARATE_SEPARATE
      case SEP_SEPARATE:
        if( ( run_mode == GIMP_RUN_NONINTERACTIVE ) ? param[10].data.d_int8 : mysc.ss.composite )
          separate_full(drawable,&values[1],&mysc);
        else
          separate_light(drawable,&values[1],&mysc);
        break;
#endif
      case SEP_FULL:
        separate_full(drawable,&values[1],&mysc);
        break;
      case SEP_LIGHT:
        separate_light(drawable,&values[1],&mysc);
        break;
      case SEP_PROOF:
        separate_proof(drawable,&values[1],&mysc);
        break;
      default:
        gimp_message( _( "Separate: Internal calling error!" ) );
      }
      if( run_mode != GIMP_RUN_NONINTERACTIVE ) {
        gimp_displays_flush();

        if( values[1].data.d_image != -1 )
          separate_store_settings( &mysc, func );
      }
    }
    break;
  case SEP_SAVE:
    if( !( separate_is_CMYK( mysc.imageID ) ) ) {
      gimp_message( _( "This is not a CMYK separated image!" ) );
      status = GIMP_PDB_EXECUTION_ERROR;
    }
    else {
      switch( run_mode ) {
      case GIMP_RUN_NONINTERACTIVE:
#ifdef ENABLE_COLOR_MANAGEMENT
        if( nparams != 5 )
#else
        if( nparams != 4 )
#endif
          status = GIMP_PDB_CALLING_ERROR;
        if( status == GIMP_PDB_SUCCESS ) {
          /* Collect the filenames */
          gchar *filename = param[3].data.d_string;
          gimp_image_set_filename( mysc.imageID, filename );
#ifdef ENABLE_COLOR_MANAGEMENT
          mysc.sas.embedprofile = param[4].data.d_int8;
#endif
        }
        break;
      case GIMP_RUN_INTERACTIVE:
        if ( !separate_save_dialog( &mysc ) )
          status = GIMP_PDB_EXECUTION_ERROR;
        break;
      case GIMP_RUN_WITH_LAST_VALS:
        break;
      default:
        break;
      }
    }
    if( status == GIMP_PDB_SUCCESS ) {
      switch( func ) {
      case SEP_SAVE:
        separate_save( drawable, &mysc );
        break;
      default:
        gimp_message( _( "Separate: Internal calling error!" ) );
      }
    }
    break;
  case SEP_DUOTONE:
    separate_duotone( drawable, &values[1], &mysc );
    break;
  default:
    gimp_message( _("Separate: Internal calling error!" ) );
    break;
  }

  if( func != SEP_DUOTONE ) {
    g_free( mysc.displayfilename );
    g_free( mysc.cmykfilename );
    g_free( mysc.rgbfilename );
    g_free( mysc.prooffilename );
  }

  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  if( func != SEP_SAVE ) {
    *nreturn_vals = 2;
    values[1].type = GIMP_PDB_IMAGE;
    if( values[1].data.d_image != -1 ) {
      if( run_mode != GIMP_RUN_NONINTERACTIVE ) {
        gimp_display_new( values[1].data.d_image );
        gimp_displays_flush();
      }
      gimp_image_undo_enable( values[1].data.d_image );
    }
  } else
    *nreturn_vals = 1;

  gimp_drawable_detach (drawable);
}


#ifdef ENABLE_COLOR_MANAGEMENT
static void embed_cmyk_profile( gint32 image_id, gchar *filename )
{
  gsize length;
  gchar *buf = NULL;
  if( g_file_get_contents( filename, &buf, &length, NULL ) ) {
    GimpParasite *parasite;
    
    /* Profile is embedded, and cannot be used independently */
    buf[47] |= 2;

    parasite = gimp_parasite_new( CMYKPROFILE, GIMP_PARASITE_PERSISTENT, length, buf );
    gimp_image_parasite_attach( image_id, parasite );
    gimp_parasite_free( parasite );

    g_free( buf );
  }
}
#endif

static void separate_core(struct SeparateContext *sc,unsigned char *src,int size)
{
  int i, b1, b2, b3, b4;
  guchar *dp1, *dp2, *dp3, *dp4;
  static guchar richBlack[] = "\0\0\0\0";

  /* keep ink limit */
  if( *( (gint32 *)richBlack ) == 0 ) {
    gdouble ratio;
    cmsDoTransform( sc->hTransform, "\0\0\0\0", richBlack, 1 );
    ratio = ( 255.0 - richBlack[3] ) / ( richBlack[0] + richBlack[1] + richBlack[2] );
    richBlack[0] = CLAMP( richBlack[0] - richBlack[0] * ratio, 0, 255 );
    richBlack[1] = CLAMP( richBlack[1] - richBlack[1] * ratio, 0, 255 );
    richBlack[2] = CLAMP( richBlack[2] - richBlack[2] * ratio, 0, 255 );
  }

  b1 = sc->bpp[0];
  b2 = sc->bpp[1];
  b3 = sc->bpp[2];
  b4 = sc->bpp[3];

  dp1 = sc->destptr[0];
  dp2 = sc->destptr[1];
  dp3 = sc->destptr[2];
  dp4 = sc->destptr[3];

  cmsDoTransform( sc->hTransform, src, sc->cmyktemp, size );

  if( sc->ss.preserveblack ) {
    for( i=0; i < size; ++i ) {
      int r = *src++;
      int g = *src++;
      int b = *src++;
      if( sc->srcbpp == 4 )
        ++src;

      if( ( r|g|b ) != 0 ) {
        dp1[i*b1] = sc->cmyktemp[i*4+3];
        dp2[i*b2] = sc->cmyktemp[i*4+2]; //ly
        dp3[i*b3] = sc->cmyktemp[i*4+1]; //lm
        dp4[i*b4] = sc->cmyktemp[i*4];   //lc
      } else {
        dp1[i*b1] = 255;
        if( !( sc->ss.overprintblack ) ) {
          dp2[i*b2] = 0;
          dp3[i*b3] = 0;
          dp4[i*b4] = 0;
        } else {
          dp2[i*b2] = richBlack[2];
          dp3[i*b3] = richBlack[1];
          dp4[i*b4] = richBlack[0];
        }
      }
    }
  } else {
    for( i=0; i < size; ++i ) {
        dp1[i*b1] = sc->cmyktemp[i*4+3];
        dp2[i*b2] = sc->cmyktemp[i*4+2];
        dp3[i*b3] = sc->cmyktemp[i*4+1];
        dp4[i*b4] = sc->cmyktemp[i*4];
      }
  }
}


static void separate_full( GimpDrawable *drawable, GimpParam *values, struct SeparateContext *sc )
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  guchar *src;
  gint32 rgbimage = sc->imageID;//gimp_drawable_get_image( drawable->drawable_id  );

  cmsHPROFILE hInProfile = NULL, hOutProfile;

  guchar cmykprimaries[]=
  {
      0,   0,   0, 255,
      0,   0, 255,   0,
      0, 255,   0,   0,
    255,   0,   0,   0
  };
  guchar rgbprimaries[]=
  {
      0,   0,   0,
    237, 220,  33,
    236,  38,  99,
     46, 138, 222
  };

  if( sc->ss.intent < 0 || sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1 ) {
    gimp_message( _( "Rendering intent is invalid." ) );
    values[0].data.d_image = -1;
    return;
  }

  if( sc->ss.profile ) {
    GimpParasite *parasite = gimp_image_parasite_find( rgbimage, "icc-profile" );
    if( parasite ) {
      hInProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                          gimp_parasite_data_size( parasite ) );
      gimp_parasite_free( parasite );
    }
  }
  if( hInProfile == NULL )
    hInProfile  = cmsOpenProfileFromFile( sc->rgbfilename, "r" );

  hOutProfile = cmsOpenProfileFromFile( sc->cmykfilename, "r" );

#if 0
  sc->hTransform = cmsCreateTransform( hOutProfile, TYPE_CMYK_8,
                                      hInProfile, TYPE_RGB_8,
                                      INTENT_RELATIVE_COLORIMETRIC,
                                      cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_NOTPRECALC );

  cmsDoTransform( sc->hTransform,
                  cmykprimaries,
                  rgbprimaries,
                  4 );

  cmsDeleteTransform( sc->hTransform );
#endif

  sc->srcbpp = drawable->bpp;

  {
    DWORD dwFlags = 0;
    SeparateRenderingIntent intent;

    if( sc->ss.bpc )
      dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

    if( sc->ss.intent == INTENT_ABSOLUTE_COLORIMETRIC + 1 ) {
      dwFlags |= cmsFLAGS_NOWHITEONWHITEFIXUP;
      cmsSetAdaptationState( 1.0 );
    } else
      cmsSetAdaptationState( 0 );

    intent = sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC ? INTENT_ABSOLUTE_COLORIMETRIC : sc->ss.intent;
    sc->hTransform = cmsCreateTransform( hInProfile, ( sc->srcbpp == 3 ? TYPE_RGB_8 : TYPE_RGBA_8 ),
                                       hOutProfile, TYPE_CMYK_8,
                                       intent, dwFlags );
  }

  if( !( sc->hTransform ) ) {
    gimp_message( _( "Internal error.\nThere might be an error in the specification of the profile." ) );
    if( hInProfile ) cmsCloseProfile( hInProfile );
    if( hOutProfile ) cmsCloseProfile( hOutProfile );
    values[0].data.d_image = -1;
    return;
  }

	width  = drawable->width;
	height = drawable->height;

	{
      gint32 new_image_id, counter;
      gdouble xres, yres;
      GimpDrawable *drawables[4];
      GimpPixelRgn pixrgn[4];
      gint32 layers[4];
      gint32 mask[4];
      gint32 ntiles = 0, tilecounter = 0;

      char *filename = separate_filename_add_suffix( gimp_image_get_filename( gimp_drawable_get_image( drawable->drawable_id ) ), "CMYK" );
      values[0].data.d_image = new_image_id =
        separate_create_planes_CMYK( filename, drawable->width, drawable->height, layers, rgbprimaries );
      g_free( filename );

      gimp_image_get_resolution( rgbimage, &xres, &yres );
      gimp_image_set_resolution( new_image_id, xres, yres );


      for( counter=0; counter < 4; ++counter ) {
        mask[counter] = gimp_layer_create_mask( layers[counter], GIMP_ADD_WHITE_MASK );
        gimp_layer_add_mask( layers[counter], mask[counter] );
        drawables[counter] = gimp_drawable_get( mask[counter] );
      }

      gimp_pixel_rgn_init( &srcPR, drawable, 0, 0, width, height, FALSE, FALSE );
      for( counter=0; counter < 4; ++counter ) {
        gimp_pixel_rgn_init( &pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE );
      }

      sc->cmyktemp = g_new( guchar, 64 * 64 * 4 );

      gimp_progress_init( _( "Separating..." ) );
      ntiles = drawable->ntile_rows * drawable->ntile_cols;
      tileiterator = gimp_pixel_rgns_register( 5, &srcPR, &pixrgn[0], &pixrgn[1], &pixrgn[2], &pixrgn[3] );
      while( tileiterator ) {
        src = srcPR.data;

        for( counter=0; counter < 4; ++counter ) {
          sc->destptr[counter] = pixrgn[counter].data;
          sc->bpp[counter] = pixrgn[counter].bpp;
        }

        separate_core( sc, src, srcPR.w * srcPR.h );

        gimp_progress_update( ( (double)tilecounter ) / ( (double)ntiles ) );

        ++tilecounter;
        tileiterator = gimp_pixel_rgns_process( tileiterator );
      }

      g_free( sc->cmyktemp );
      cmsDeleteTransform( sc->hTransform );
      cmsCloseProfile( hInProfile );
      cmsCloseProfile( hOutProfile );

#ifdef ENABLE_COLOR_MANAGEMENT
      embed_cmyk_profile( new_image_id, sc->cmykfilename );
#endif

      for( counter=0; counter < 4; ++counter ) {
        gimp_drawable_flush( drawables[counter] );
        gimp_drawable_update( drawables[counter]->drawable_id, 0, 0, width, height );
        gimp_drawable_detach( drawables[counter] );
      }

      //gimp_display_new( new_image_id );
      //gimp_displays_flush();
    }
}


static void separate_light(GimpDrawable *drawable,GimpParam *values,struct SeparateContext *sc)
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  gint bytes;
  guchar *src;
  gint ntiles=0, tilecounter=0;
  gint32 rgbimage = sc->imageID;//gimp_drawable_get_image( drawable->drawable_id  );

  cmsHPROFILE hInProfile = NULL, hOutProfile;

  if( sc->ss.intent < 0 || sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1 ) {
    gimp_message( _( "Rendering intent is invalid." ) );
    values[0].data.d_image = -1;
    return;
  }

  if( sc->ss.profile ) {
    GimpParasite *parasite = gimp_image_parasite_find( rgbimage, "icc-profile" );
    if( parasite ) {
      hInProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                          gimp_parasite_data_size( parasite ) );
      gimp_parasite_free( parasite );
    }
  }
  if( hInProfile == NULL )
    hInProfile  = cmsOpenProfileFromFile( sc->rgbfilename, "r" );

  hOutProfile = cmsOpenProfileFromFile( sc->cmykfilename, "r" );

  sc->srcbpp = drawable->bpp;

  {
    DWORD dwFlags = 0;
    SeparateRenderingIntent intent;

    if( sc->ss.bpc )
      dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

    if( sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1 ) {
      dwFlags |= cmsFLAGS_NOWHITEONWHITEFIXUP;
      cmsSetAdaptationState( 1.0 );
    } else
      cmsSetAdaptationState( 0 );

    intent = sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC ? INTENT_ABSOLUTE_COLORIMETRIC : sc->ss.intent;
    sc->hTransform = cmsCreateTransform( hInProfile, ( sc->srcbpp == 3 ? TYPE_RGB_8 : TYPE_RGBA_8 ),
                                       hOutProfile, TYPE_CMYK_8,
                                       intent, dwFlags );
  }

  if( !( sc->hTransform ) ) {
    gimp_message( _( "Internal error.\nThere might be an error in the specification of the profile." ) );
    if( hInProfile ) cmsCloseProfile( hInProfile );
    if( hOutProfile ) cmsCloseProfile( hOutProfile );
    values[0].data.d_image = -1;
    return;
  }

  /* Get the size of the input image. (This will/must be the same
	 as the size of the output image.) */
  width  = drawable->width;
  height = drawable->height;
  bytes  = drawable->bpp;

  /*  initialize the pixel regions  */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE );
  {
    gint32 new_image_id, counter;
    gdouble xres, yres;
    GimpDrawable *drawables[4];
    GimpPixelRgn pixrgn[4];
    gint32 layers[4];

    enum layerid { LAYER_K, LAYER_Y, LAYER_M, LAYER_C };

    char *filename = separate_filename_add_suffix( gimp_image_get_filename( gimp_drawable_get_image( drawable->drawable_id ) ), "CMYK" );
    values[0].data.d_image = new_image_id =
      separate_create_planes_grey( filename, drawable->width,drawable->height, layers );
    g_free( filename );

    gimp_image_get_resolution( rgbimage, &xres, &yres );
    gimp_image_set_resolution( new_image_id, xres, yres );

    gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);
    for( counter=0; counter<4; ++counter ) {
      drawables[counter] = gimp_drawable_get( layers[counter] );
      gimp_pixel_rgn_init( &pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE );
    }

    sc->cmyktemp = g_new( guchar, 64 * 64 * 4 );

    gimp_progress_init( _( "Separating..." ) );
    ntiles = drawable->ntile_cols * drawable->ntile_rows;
    tileiterator = gimp_pixel_rgns_register( 5, &srcPR, &pixrgn[0], &pixrgn[1], &pixrgn[2], &pixrgn[3] );
    while( tileiterator ) {
      src = srcPR.data;

      for( counter=0; counter < 4; ++counter ) {
        sc->destptr[counter] = pixrgn[counter].data;
        sc->bpp[counter] = pixrgn[counter].bpp;
      }

      separate_core( sc, src, srcPR.w * srcPR.h );

      gimp_progress_update( ( (double)tilecounter ) / ( (double)ntiles) );
      ++tilecounter;
      tileiterator = gimp_pixel_rgns_process( tileiterator );
    }

    g_free( sc->cmyktemp );
    cmsDeleteTransform( sc->hTransform );
    cmsCloseProfile( hInProfile );
    cmsCloseProfile( hOutProfile );

#ifdef ENABLE_COLOR_MANAGEMENT
    embed_cmyk_profile( new_image_id, sc->cmykfilename );
#endif

    for( counter=0; counter < 4; ++counter ) {
      gimp_drawable_flush( drawables[counter] );
      gimp_drawable_update( drawables[counter]->drawable_id, 0, 0, width, height );
      gimp_drawable_detach( drawables[counter] );
    }

    //gimp_display_new( new_image_id );
    //gimp_displays_flush();
  }
}


static void separate_proof(GimpDrawable *drawable,GimpParam *values,struct SeparateContext *sc)
{
  gpointer tileiterator;
  gint width, height;
  gint bytes;
  gint ntiles=0, tilecounter=0;
  gint32 cmykimage = sc->imageID;//gimp_drawable_get_image( drawable->drawable_id  );

  GimpDrawable *drawables[5];

  cmsHPROFILE hInProfile = NULL, hOutProfile;
  cmsHTRANSFORM hTransform;
  gint intent ;
  DWORD dwFLAGS;

  values[0].data.d_image = -1; /* error? */

  if( sc->ps.mode < 0 || sc->ps.mode > 2 ) {
    gimp_message( _( "Proofing mode is invalid." ) );
    return;
  }
  
  if( !( separate_is_CMYK( cmykimage ) ) ) {
    gimp_message( _( "Image is not separated..." ) );
    return;
  }

  drawables[0]=separate_find_channel(cmykimage,sep_C);
  drawables[1]=separate_find_channel(cmykimage,sep_M);
  drawables[2]=separate_find_channel(cmykimage,sep_Y);
  drawables[3]=separate_find_channel(cmykimage,sep_K);

  {
    int i, n = 0;
    gchar* channel_names[4] = { _( "C" ), _( "M" ), _( "Y" ), _( "K" ) };
    gchar* missing_channels[7];
    for( i = 0; i < 4; i++ ) {
      if( drawables[i] == 0 )
        missing_channels[n++] = channel_names[i];
    }
    if( n ) {
      missing_channels[n] = NULL;
      missing_channels[5] = g_strjoinv( ", ", missing_channels );
      missing_channels[6] = g_strdup_printf( _( "Couldn't get channel(s) : %s" ), missing_channels[5] );
      gimp_message( missing_channels[6] );
      g_free( missing_channels[5] );
      g_free( missing_channels[6] );
      return;
    }
  }

#ifdef ENABLE_COLOR_MANAGEMENT
  if( sc->ps.profile ) {
    GimpParasite *parasite = gimp_image_parasite_find( cmykimage, CMYKPROFILE );
    if( parasite ) {
      hInProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                          gimp_parasite_data_size( parasite ) );
      gimp_parasite_free( parasite );
    }
  }
  if( !hInProfile )
#endif
    hInProfile = cmsOpenProfileFromFile( sc->prooffilename, "r" );
  hOutProfile  = cmsOpenProfileFromFile(sc->displayfilename, "r" );

  if( !hOutProfile ) {
    gimp_message( _( "Internal error.\nThere might be an error in the specification of the profile." ) );
    if( hInProfile ) cmsCloseProfile( hInProfile );
    //values[0].data.d_image = -1;
    return;
  }
  if( sc->ps.mode == 2 ) { /* Simulate media white */
    cmsCIEXYZ whitePoint;
    cmsCIExyY wp_xyY;
    LPcmsCIExyY D50_xyY = cmsD50_xyY();

    intent = INTENT_ABSOLUTE_COLORIMETRIC;
    dwFLAGS = cmsFLAGS_NOWHITEONWHITEFIXUP;

    cmsTakeMediaWhitePoint( &whitePoint, hOutProfile );
    cmsXYZ2xyY( &wp_xyY, &whitePoint );
    cmsSetAdaptationState( ( pow( fabs( wp_xyY.x - D50_xyY->x ), 2 ) +
                             pow( fabs( wp_xyY.y - D50_xyY->y ), 2 ) > 0.000005 ) ? 1.0 : 0 );
    //cmsSetAdaptationState( ( cmsIsTag( hOutProfile, 0x63686164L/*'chad'*/ ) == FALSE ) ? 1.0 : 0 );
  } else { /* Others */
    intent = sc->ps.mode;
    dwFLAGS = 0;

    cmsSetAdaptationState( 0 );
  }
  hTransform = cmsCreateTransform(hInProfile,  TYPE_CMYK_8,
                                  hOutProfile, TYPE_RGB_8,
                                  intent,
                                  dwFLAGS );
  if( !hTransform ) {
    gimp_message( _( "Internal error.\nThere might be an error in the specification of the profile." ) );
    if( hInProfile ) cmsCloseProfile( hInProfile );
    if( hOutProfile ) cmsCloseProfile( hOutProfile );
    //values[0].data.d_image = -1;
    return;
  }

  /* Get the size of the input image. (This will/must be the same
   *  as the size of the output image.
   */
  width  = drawable->width;
  height = drawable->height;
  bytes  = 1;

  {
    gint32 new_image_id, counter;
    gdouble xres, yres;
    GimpPixelRgn pixrgn[5];
    gint32 layers[1];
    guchar *cmyktemp;

    char *filename = separate_filename_add_suffix( gimp_image_get_filename( cmykimage ), "Proof" ); 
    values[0].data.d_image = new_image_id =
      separate_create_RGB( filename, drawable->width, drawable->height, layers );
    g_free(filename);

    gimp_image_get_resolution( cmykimage, &xres, &yres );
    gimp_image_set_resolution( new_image_id, xres, yres );

    drawables[4]=gimp_drawable_get( layers[0] );

    for( counter = 0; counter < 4; ++counter )
      gimp_pixel_rgn_init( &pixrgn[counter], drawables[counter], 0, 0, width, height, FALSE, FALSE );
    gimp_pixel_rgn_init( &pixrgn[4], drawables[4], 0, 0, width, height, TRUE, FALSE );

    cmyktemp = g_new( guchar, pixrgn[4].w * pixrgn[4].h * 4 );

    gimp_progress_init( _( "Proofing..." ) );
    ntiles=drawables[4]->ntile_cols*drawables[4]->ntile_rows;
    tileiterator=gimp_pixel_rgns_register(5,&pixrgn[4],&pixrgn[0],&pixrgn[1],&pixrgn[2],&pixrgn[3]);

    while(tileiterator)
    {
      long i;
      guchar *ptr[5];

      for(counter=0;counter<5;++counter)
        ptr[counter]=pixrgn[counter].data;

      for(i=0;i<(pixrgn[4].w*pixrgn[4].h);++i)
      {
        cmyktemp[i*4]=(ptr[0])[i*pixrgn[0].bpp];
        cmyktemp[i*4+1]=(ptr[1])[i*pixrgn[1].bpp];
        cmyktemp[i*4+2]=(ptr[2])[i*pixrgn[2].bpp];
        cmyktemp[i*4+3]=(ptr[3])[i*pixrgn[3].bpp];
      }

      cmsDoTransform(hTransform,
                     cmyktemp,
                     ptr[4],
                     pixrgn[4].w*pixrgn[4].h);

      gimp_progress_update (((double) tilecounter) / ((double) ntiles));
      ++tilecounter;
      tileiterator = gimp_pixel_rgns_process (tileiterator);
    }

    g_free( cmyktemp );
    cmsDeleteTransform( hTransform );
    cmsCloseProfile( hInProfile );
    cmsCloseProfile( hOutProfile );

#ifdef ENABLE_COLOR_MANAGEMENT
    {
      /* embed destination profile for correct preview */
      gint num_matches = 0;
      gchar **proc_names;
      gimp_procedural_db_query( "^plug-in-icc-profile-set$",
                                ".*", ".*", ".*", ".*", ".*", ".*",
                                &num_matches,
                                &proc_names );
      if( num_matches ) {
        GimpParam *return_vals;
        gint i, nreturn_vals;

        for( i = 0; i < num_matches; i++ )
          g_free( proc_names[i] );
        g_free( proc_names );

        return_vals = gimp_run_procedure( "plug-in-icc-profile-set",
                                          &nreturn_vals,
                                          GIMP_PDB_INT32,
                                          GIMP_RUN_NONINTERACTIVE,
                                          GIMP_PDB_IMAGE,
                                          new_image_id,
                                          GIMP_PDB_STRING,
                                          sc->displayfilename,
                                          GIMP_PDB_END );

        gimp_destroy_params( return_vals, nreturn_vals );
      } else {
        gsize length;
        gchar *buf = NULL;
        if( g_file_get_contents( sc->displayfilename, &buf, &length, NULL ) ) {
          GimpParasite *parasite;

          /* Profile is embedded, and cannot be used independently */
          buf[47] |= 2;

          parasite= gimp_parasite_new( "icc-profile", 0, length, buf );
          gimp_image_parasite_attach( new_image_id, parasite );
          gimp_parasite_free( parasite );

          g_free( buf );
        }
      }
    }
#endif

    gimp_drawable_flush( drawables[4] );
    gimp_drawable_update( drawables[4]->drawable_id, 0, 0, width, height );
    gimp_drawable_detach( drawables[4] );

    //gimp_display_new( new_image_id );
    //gimp_displays_flush();
  }

}


static void separate_duotone(GimpDrawable *drawable,GimpParam *values,struct SeparateContext *sc)
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  guchar *src;

  width  = drawable->width;
  height = drawable->height;

  {
		gint32 new_image_id, counter;
		GimpDrawable *drawables[2];
		GimpPixelRgn pixrgn[2];
		gint32 layers[2];
		gint32 mask[2];
		gint32 ntiles = 0, tilecounter = 0;

		gchar *filename = separate_filename_add_suffix( gimp_image_get_filename( sc->imageID ),"MK"); 
		values[0].data.d_image = new_image_id =
          separate_create_planes_Duotone( filename, drawable->width, drawable->height, layers);
		g_free( filename );

		for(counter=0;counter<2;++counter)
		{
			mask[counter]=gimp_layer_create_mask(layers[counter],GIMP_ADD_WHITE_MASK);
			gimp_layer_add_mask(layers[counter],mask[counter]);
			drawables[counter]=gimp_drawable_get(mask[counter]);
		}

		gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);
		for(counter=0;counter<2;++counter)
		{
			gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);
		}

		gimp_progress_init (_("Separating..."));
		ntiles=drawable->ntile_rows*drawable->ntile_cols;
		tileiterator=gimp_pixel_rgns_register(3,&srcPR,&pixrgn[0],&pixrgn[1]);
		while(tileiterator)
		{
			long i;
			guchar *destptr[2];
			src=srcPR.data;

			for(counter=0;counter<2;++counter)
				destptr[counter]=pixrgn[counter].data;

			for(i=0;i<(srcPR.w*srcPR.h);++i)
			{
				int r,g,b,t;
				r=src[i*srcPR.bpp];
				g=src[i*srcPR.bpp+1];
				b=src[i*srcPR.bpp+2];
                        t=(g+b)/2;
                        if(r>t)
					g=b=t;
				else
					r=g=(r+g+b)/3;
				(destptr[0])[i*pixrgn[0].bpp]=255-r;
				(destptr[1])[i*pixrgn[1].bpp]=r-g;
			}

			gimp_progress_update(((double) tilecounter) / ((double) ntiles));

			++tilecounter;
			tileiterator = gimp_pixel_rgns_process (tileiterator);
		}

		for(counter=0;counter<2;++counter)
		{
			gimp_drawable_flush (drawables[counter]);
			gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
			gimp_drawable_detach (drawables[counter]);
		}

		//gimp_display_new (new_image_id);
		//gimp_displays_flush();
	}

}


void callback_preserve_black_toggled( GtkWidget *toggleButton, gpointer data )
{
  gtk_widget_set_sensitive( GTK_WIDGET( data ),
                            gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggleButton ) ) );
}


static gint
separate_dialog (struct SeparateContext *sc)
{
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkTable  *table;
  GtkWidget *temp;
  GtkWidget *pureblackselector;
  GtkWidget *overprintselector;
  GtkWidget *intentselector;
  GtkWidget *bpcselector;
  GtkWidget *profileselector;
#ifdef SEPARATE_SEPARATE
  GtkWidget *compositeselector;
#endif
  gboolean   run;

  gimp_ui_init( "separate", FALSE );

  sc->dialogresult  = FALSE;
  dialog = gimp_dialog_new( "Separate", "separate",
                            NULL, 0,
                            gimp_standard_help_func, "gimp-filter-separate",
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

  vbox = gtk_vbox_new( FALSE, 0 );
  gtk_container_set_border_width( GTK_CONTAINER( vbox ), 12 );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), vbox, TRUE, TRUE, 0 );
  gtk_widget_show( vbox );

#ifdef SEPARATE_SEPARATE
  table = GTK_TABLE( gtk_table_new( 2, 9, FALSE ) );
#else
  table = GTK_TABLE( gtk_table_new( 2, 9, FALSE ) );
#endif
  gtk_table_set_col_spacing( table, 0, 8 );
  gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
  gtk_widget_show( GTK_WIDGET( table ) );

  /* Profile file selectors */

  temp = gtk_label_new( _( "Source color space:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, 0, 1, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  sc->rgbfileselector = icc_button_new();
  icc_button_set_max_entries (ICC_BUTTON (sc->rgbfileselector), 10);
  icc_button_set_title( ICC_BUTTON( sc->rgbfileselector ), _( "Choose source profile (RGB)..." ) );
  icc_button_set_filename( ICC_BUTTON( sc->rgbfileselector ), sc->rgbfilename, FALSE );
  icc_button_set_mask( ICC_BUTTON( sc->rgbfileselector ), CLASS_INPUT | CLASS_OUTPUT | CLASS_DISPLAY, COLORSPACE_RGB );

  gtk_table_attach( table, sc->rgbfileselector, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );
  gtk_widget_show( sc->rgbfileselector );

  profileselector = gtk_check_button_new_with_label( _( "Give priority to embedded profile" ) );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( profileselector ), sc->ss.profile );
  gtk_table_attach( table, profileselector, 1, 2, 1, 2, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( profileselector );

  {
    GimpParasite *parasite;
    cmsHPROFILE hProfile;
    gchar *labelStr = NULL;
    GtkWidget *label;
    gint indicator_size, indicator_spacing;

    if( ( parasite = gimp_image_parasite_find( sc->imageID, "icc-profile" ) ) != NULL ) {
      if( ( hProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                            gimp_parasite_data_size( parasite ) ) ) != NULL ) {
        gchar *desc = _icc_button_get_profile_desc( hProfile );
        labelStr = g_strdup_printf( "%s", desc );
        g_free( desc );
        cmsCloseProfile( hProfile );
      }
      gimp_parasite_free( parasite );
    }

    if( labelStr ) {
      label = gtk_label_new( labelStr );
      g_free( labelStr );
    } else
      label = gtk_label_new( _( "( no profiles embedded )" ) );

    gtk_label_set_ellipsize( GTK_LABEL( label ), PANGO_ELLIPSIZE_MIDDLE );
    gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );

    gtk_widget_style_get( GTK_WIDGET( profileselector ),
                          "indicator-size", &indicator_size,
                          "indicator-spacing", &indicator_spacing,
                          NULL );
    temp = gtk_alignment_new( 0, 0.5, 1, 0 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( temp ),
                               0, 0,
                               indicator_size + indicator_spacing * 4, 0 );
    gtk_container_add( GTK_CONTAINER( temp ), label );
  }
  gtk_table_attach( table, temp, 1, 2, 2, 3, GTK_FILL, 0, 0, 0 );
  gtk_widget_show_all( temp );
  gtk_table_set_row_spacing( table, 2, 8 );

  temp = gtk_label_new( _( "Destination color space:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, 3, 4, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  sc->cmykfileselector = icc_button_new();
  icc_button_set_max_entries (ICC_BUTTON (sc->cmykfileselector), 10);
  icc_button_set_title( ICC_BUTTON( sc->cmykfileselector ), _( "Choose output profile (CMYK)..." ) );
  icc_button_set_filename( ICC_BUTTON( sc->cmykfileselector ), sc->cmykfilename, FALSE );
  icc_button_set_mask( ICC_BUTTON( sc->cmykfileselector ), CLASS_OUTPUT, COLORSPACE_CMYK );

  gtk_table_attach( table, sc->cmykfileselector, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0 );
  gtk_widget_show( sc->cmykfileselector );
  gtk_table_set_row_spacing( table, 3, 12 );

  temp=gtk_label_new( _( "Rendering intent:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, 4, 5, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  intentselector = gtk_combo_box_new_text();
  gtk_combo_box_append_text( GTK_COMBO_BOX( intentselector ), _( "Perceptual" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( intentselector ), _( "Relative colorimetric" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( intentselector ), _( "Saturation" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( intentselector ), _( "Absolute colorimetric" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( intentselector ), _( "Absolute colorimetric(2)" ) );
  gtk_combo_box_set_active( GTK_COMBO_BOX( intentselector),
                            sc->ss.intent < 0 ? 0 : ( sc->ss.intent > 4 ? 4 : sc->ss.intent ) );
  gtk_table_attach( table, intentselector, 1, 2, 4, 5, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( intentselector );

  bpcselector = gtk_check_button_new_with_label( _( "Use BPC algorithm" ) );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bpcselector ), sc->ss.bpc );
  gtk_table_attach( table, bpcselector, 1, 2, 5, 6, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( bpcselector );
  gtk_table_set_row_spacing( table, 5, 8 );

  temp=gtk_label_new( _( "Options:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, 6, 7, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  pureblackselector = gtk_check_button_new_with_label( _( "Preserve pure black"  ));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pureblackselector ), sc->ss.preserveblack );
  gtk_table_attach( table, pureblackselector, 1, 2, 6, 7, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( pureblackselector );

  overprintselector = gtk_check_button_new_with_label( _( "Overprint pure black" ) );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( overprintselector ), sc->ss.overprintblack );
  gtk_widget_set_sensitive( GTK_WIDGET( overprintselector ), sc->ss.preserveblack );
  gtk_table_attach( table, overprintselector, 1, 2, 7, 8, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( overprintselector );

  g_signal_connect( G_OBJECT( pureblackselector ), "toggled",
                    G_CALLBACK( callback_preserve_black_toggled ), (gpointer)overprintselector );

#ifdef SEPARATE_SEPARATE
  if( sc->integrated ) {
    compositeselector = gtk_check_button_new_with_label( _( "Make CMYK pseudo-composite"  ));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( compositeselector ), sc->ss.composite );
    gtk_table_attach( table, compositeselector, 1, 2, 8, 9, GTK_FILL, 0, 0, 0 );
    gtk_widget_show( compositeselector );
  }
#endif

  /* Show the widgets */

  run = ( gimp_dialog_run( GIMP_DIALOG( dialog ) ) == GTK_RESPONSE_OK );

  if( run ) {
    /* Update the source and destination profile names... */
    gchar *tmp;

    tmp = icc_button_get_filename( ICC_BUTTON( sc->rgbfileselector ) );
    if( tmp != NULL && strlen( tmp ) ) {
      g_free( sc->rgbfilename );
      sc->rgbfilename = tmp;
    } else
      g_free( tmp );

    tmp = icc_button_get_filename( ICC_BUTTON( sc->cmykfileselector ) );
    if( tmp != NULL && strlen( tmp ) ) {
      g_free( sc->cmykfilename );
      sc->cmykfilename = tmp;
    } else
      g_free( tmp );

    sc->ss.preserveblack = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pureblackselector ) );
    sc->ss.overprintblack = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( overprintselector ) );
    sc->ss.intent = gtk_combo_box_get_active( GTK_COMBO_BOX( intentselector ) );
    sc->ss.bpc = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bpcselector ) );
    sc->ss.profile = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( profileselector ) );
#ifdef SEPARATE_SEPARATE
    if( sc->integrated )
      sc->ss.composite = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( compositeselector ) );
#endif
  }

  gtk_widget_destroy (dialog);

  return run;
}


static gint
proof_dialog (struct SeparateContext *sc)
{
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkTable  *table;
  guint attach = 0;
  GtkWidget *temp;
  GtkWidget *modeselector;
  gboolean   run;
#ifdef ENABLE_COLOR_MANAGEMENT
  GtkWidget *profileselector;
#endif

  gimp_ui_init( "separate", FALSE );

  sc->dialogresult = FALSE;
  dialog = gimp_dialog_new( "Proof", "proof",
                            NULL, 0,
                            gimp_standard_help_func, "gimp-filter-proof",
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL );

  vbox = gtk_vbox_new( FALSE, 0 );
  gtk_container_set_border_width( GTK_CONTAINER( vbox ), 12 );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), vbox, TRUE, TRUE, 0 );
  gtk_widget_show( vbox );

#ifdef ENABLE_COLOR_MANAGEMENT
  table = GTK_TABLE(gtk_table_new( 2, 6, FALSE ) );
#else
  table = GTK_TABLE(gtk_table_new( 2, 4, FALSE ) );
#endif
  gtk_table_set_col_spacing( table, 0, 8 );
  gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
  gtk_widget_show( GTK_WIDGET( table ) );

  /* Profile file selectors */

  temp=gtk_label_new( _( "Monitor / working color space:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  sc->rgbfileselector = icc_button_new();
  icc_button_set_max_entries (ICC_BUTTON (sc->rgbfileselector), 10);
  icc_button_set_title( ICC_BUTTON( sc->rgbfileselector ), _( "Choose RGB profile..." ) );
  icc_button_set_filename( ICC_BUTTON( sc->rgbfileselector ), sc->displayfilename, FALSE );
  icc_button_set_mask( ICC_BUTTON( sc->rgbfileselector ), CLASS_OUTPUT | CLASS_DISPLAY, COLORSPACE_RGB );

  gtk_table_attach( table,sc->rgbfileselector, 1, 2, attach , attach + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0 );
  attach++;
  gtk_widget_show( sc->rgbfileselector );
  gtk_table_set_row_spacing( table, 0, 8 );
  
  temp = gtk_label_new( _( "Separated image's color space:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  sc->cmykfileselector = icc_button_new();
  icc_button_set_max_entries (ICC_BUTTON (sc->cmykfileselector), 10);
  icc_button_set_title( ICC_BUTTON( sc->cmykfileselector ), _( "Choose CMYK profile..." ) );
  icc_button_set_filename( ICC_BUTTON( sc->cmykfileselector ), sc->prooffilename, FALSE );
  icc_button_set_mask( ICC_BUTTON( sc->cmykfileselector ), CLASS_OUTPUT, COLORSPACE_CMYK );

  gtk_table_attach( table, sc->cmykfileselector, 1, 2, attach, attach + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0 );
  gtk_widget_show( sc->cmykfileselector );
  attach++;

#ifdef ENABLE_COLOR_MANAGEMENT
  profileselector = gtk_check_button_new_with_label( _( "Give priority to attached profile" ) );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( profileselector ), sc->ps.profile );
  gtk_table_attach( table, profileselector, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  attach++;
  gtk_widget_show( profileselector );

  {
    GimpParasite *parasite;
    cmsHPROFILE hProfile;
    gchar *labelStr = NULL;
    GtkWidget *label;
    gint indicator_size, indicator_spacing;

    if( ( parasite = gimp_image_parasite_find( sc->imageID, CMYKPROFILE ) ) != NULL ) {
      if( ( hProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                            gimp_parasite_data_size( parasite ) ) ) != NULL ) {
        gchar *desc = _icc_button_get_profile_desc( hProfile );
        labelStr = g_strdup_printf( "%s", desc );
        g_free( desc );
        cmsCloseProfile( hProfile );
      }
      gimp_parasite_free( parasite );
    }

    if( labelStr ) {
      label = gtk_label_new( labelStr );
      g_free( labelStr );
    } else
      label = gtk_label_new( _( "( no profiles attached )" ) );

    gtk_label_set_ellipsize( GTK_LABEL( label ), PANGO_ELLIPSIZE_MIDDLE );
    gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );

    gtk_widget_style_get( GTK_WIDGET( profileselector ),
                          "indicator-size", &indicator_size,
                          "indicator-spacing", &indicator_spacing,
                          NULL );
    temp = gtk_alignment_new( 0, 0.5, 1, 0 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( temp ),
                               0, 0,
                               indicator_size + indicator_spacing * 4, 0 );
    gtk_container_add( GTK_CONTAINER( temp ), label );
  }
  gtk_table_attach( table, temp, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  attach++;
  gtk_widget_show_all( temp );
#endif

  gtk_table_set_row_spacing( table, attach - 1, 12 );

  temp=gtk_label_new( _( "Mode:" ) );
  gtk_misc_set_alignment( GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach( table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( temp );

  modeselector = gtk_combo_box_new_text();
  gtk_combo_box_append_text( GTK_COMBO_BOX( modeselector ), _( "Normal" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( modeselector ), _( "Simulate black ink" ) );
  gtk_combo_box_append_text( GTK_COMBO_BOX( modeselector ), _( "Simulate media white" ) );
  gtk_combo_box_set_active( GTK_COMBO_BOX( modeselector ),
                           sc->ps.mode < 0 ? 0 : ( sc->ps.mode > 2 ? 2 : sc->ps.mode ) );
  gtk_table_attach( table, modeselector, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0 );
  gtk_widget_show( modeselector );

  /* Show the widgets */

  run = ( gimp_dialog_run( GIMP_DIALOG( dialog ) ) == GTK_RESPONSE_OK );

  if( run ) {
    /* Update the source and destination profile names... */
    gchar *tmp;

    tmp = icc_button_get_filename( ICC_BUTTON( sc->rgbfileselector ) );
    if( tmp != NULL && strlen( tmp ) ) {
      g_free( sc->displayfilename );
      sc->displayfilename = tmp;
    } else
      g_free( tmp );

    tmp = icc_button_get_filename( ICC_BUTTON( sc->cmykfileselector ) );
    if( tmp != NULL && strlen( tmp ) ) {
      g_free( sc->prooffilename );
      sc->prooffilename = tmp;
    } else
      g_free( tmp );

    sc->ps.mode = gtk_combo_box_get_active( GTK_COMBO_BOX( modeselector ) );
#ifdef ENABLE_COLOR_MANAGEMENT
    sc->ps.profile = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( profileselector ) );
#endif
  }

  gtk_widget_destroy (dialog);

  return run;
}

static gint
separate_save_dialog( struct SeparateContext *sc )
{
  gchar *filename = gimp_image_get_filename( sc->imageID );
#ifdef G_OS_WIN32
  gchar *dirname = g_path_get_dirname( gimp_filename_to_utf8( filename ) );
  gchar *basename = g_path_get_basename( gimp_filename_to_utf8( filename ) );
#else
  gchar *dirname = g_path_get_dirname( filename );
  gchar *basename = g_path_get_basename( filename );
#endif
#ifdef ENABLE_COLOR_MANAGEMENT
  GtkWidget *hbox, *label, *combo;
#endif

  sc->filename=NULL;
  sc->dialogresult=FALSE;
  gimp_ui_init ("separate", FALSE);

  sc->filenamefileselector = gtk_file_chooser_dialog_new( _("Save separated TIFF..."),
                                                          NULL,
                                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                          NULL );
  gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( sc->filenamefileselector ), dirname );
  gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( sc->filenamefileselector ), basename );
  gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER( sc->filenamefileselector ), TRUE );

  g_free( filename );
  g_free( dirname );
  g_free( basename );

#ifdef ENABLE_COLOR_MANAGEMENT
  {
    GimpParasite *parasite;
    gint lastItemIndex = 2;

    hbox = gtk_hbox_new( FALSE, 8 );
    label = gtk_label_new( _( "Embed color profile:" ) );
    gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );
    combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text( GTK_COMBO_BOX( combo ), _( "None" ) );
    gtk_combo_box_append_text( GTK_COMBO_BOX( combo ), _( "CMYK default profile" ) );
    gtk_combo_box_append_text( GTK_COMBO_BOX( combo ), _( "Print simulation profile" ) );
    if( ( parasite = gimp_image_parasite_find( sc->imageID, CMYKPROFILE ) ) != NULL ) {
      cmsHPROFILE hProfile = cmsOpenProfileFromMem( (gpointer)gimp_parasite_data( parasite ),
                                                    gimp_parasite_data_size( parasite ) );
      if( hProfile ) {
        gchar *desc = _icc_button_get_profile_desc( hProfile );
        gchar *text = g_strdup_printf( _( "Own profile : %s" ), desc );

        gtk_combo_box_append_text( GTK_COMBO_BOX( combo ), text );
        lastItemIndex++;

        g_free( desc );
        g_free( text );
        cmsCloseProfile( hProfile );
      }
      gimp_parasite_free( parasite );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( combo ),
                              ( sc->sas.embedprofile < 0 || sc->sas.embedprofile > lastItemIndex ) ? 0 : sc->sas.embedprofile );
    gtk_box_pack_start( GTK_BOX( hbox ), combo, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( sc->filenamefileselector )->vbox ), hbox, FALSE, FALSE, 0 );
    gtk_widget_show_all( hbox );
  }
#endif

  sc->dialogresult = gtk_dialog_run( GTK_DIALOG( sc->filenamefileselector ) );
  if ( sc->dialogresult == GTK_RESPONSE_ACCEPT ) {
#ifdef ENABLE_COLOR_MANAGEMENT
    sc->sas.embedprofile = gtk_combo_box_get_active( GTK_COMBO_BOX( combo ) );
#endif
    sc->filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( sc->filenamefileselector ) );
#ifdef G_OS_WIN32
    /* GIMP 2.2.x : ANSI
       GIMP 2.3.x : UTF-8 */
    if( GIMP_MAJOR_VERSION == 2 && GIMP_MINOR_VERSION < 3 ) {
      gchar *_filename = g_win32_locale_filename_from_utf8( sc->filename );
      gimp_image_set_filename( sc->imageID, _filename != NULL ? _filename : sc->filename );
      g_free( _filename );
    } else
      gimp_image_set_filename( sc->imageID, sc->filename );
#else
    gimp_image_set_filename( sc->imageID, sc->filename );
#endif
    g_free( sc->filename );
    sc->filename = NULL;
    sc->dialogresult = TRUE;
  } else
    sc->dialogresult = FALSE;

  gtk_widget_destroy( sc->filenamefileselector );

  return sc->dialogresult;
}
