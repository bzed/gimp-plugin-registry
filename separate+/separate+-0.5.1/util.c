
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libgimp/gimp.h>

#include "platform.h"
#include "separate.h"
#include "util.h"

static char *separate_channelnames[]={"C","M","Y","K"};

static gboolean check_layer_name(char *layer_name,gint *mask)
{
  if((((*mask)&16) == 0 ) && (strcmp(layer_name,_( "Background" ) )==0))
	{
		*mask|=16;
		return(TRUE);
	}
  if((((*mask)&1) == 0 ) && (strcmp(layer_name,"C")==0))
	{
		*mask|=1;
		return(TRUE);
	}
  if((((*mask)&2) == 0 ) && (strcmp(layer_name,"M")==0))
	{
		*mask|=2;
		return(TRUE);
	}
  if((((*mask)&4) == 0 ) && (strcmp(layer_name,"Y")==0))
	{
		*mask|=4;
		return(TRUE);
	}
  if((((*mask)&8) == 0 ) && (strcmp(layer_name,"K")==0))
	{
		*mask|=8;
		return(TRUE);
	}
	return(FALSE);
}


GimpDrawable *separate_find_channel(gint32 image_id,enum separate_channel channel)
{
	GimpDrawable *result=NULL;
	gint *layers,layercount;
	gint i;

	if((channel<0) || (channel>3))
		return(NULL);

	layers=gimp_image_get_layers(image_id,&layercount);
	for(i=0;i<layercount;++i)
	{
		char *layer_name=gimp_drawable_get_name(layers[i]);
		if(strcmp(layer_name,separate_channelnames[channel])==0)
		{
			result=gimp_drawable_get(layers[i]);
			if(gimp_drawable_is_rgb(result->drawable_id))
				result=gimp_drawable_get(gimp_layer_get_mask(layers[i]));
			return(result);
		}
	}
	return(result);
}


gboolean separate_is_CMYK(gint32 image_id)
{
	gint *layers,layercount;
	gint i;
	gint mask=0;

	layers=gimp_image_get_layers(image_id,&layercount);

	if(layercount>5)
		return(FALSE);
	for(i=0;i<layercount;++i)
	{
		char *layer_name=gimp_drawable_get_name(layers[i]);
		if(check_layer_name(layer_name,&mask)==FALSE)
			return(FALSE);
	}
	if((mask==0)||(mask==16))
		return(FALSE);
	return(TRUE);
}


void separate_init_settings( struct SeparateContext *sc, gboolean get_last_values )
{
  memset( sc, '\0', sizeof( struct SeparateContext ) );
#ifdef ENABLE_COLOR_MANAGEMENT
  {
    GimpColorConfig *config;

    if( ( config = gimp_get_color_configuration() ) ) {
      g_object_get( config,
                    "display-profile", &( sc->displayfilename ),
                    "rgb-profile", &( sc->rgbfilename ),
                    "cmyk-profile", &( sc->cmykfilename ),
                    "printer-profile", &( sc->prooffilename ),
                    "display-rendering-intent", &( sc->ps.mode ),
                    "simulation-rendering-intent", &( sc->ss.intent ),
                    NULL );
      if( sc->ps.mode >= 2 )
        sc->ps.mode--;
    }
    g_object_unref( G_OBJECT( config ) );
  }
#endif

  if( !( sc->displayfilename ) )
    sc->displayfilename = DEFAULT_RGB_PROFILE;
  if( !( sc->rgbfilename ) )
    sc->rgbfilename = DEFAULT_RGB_PROFILE;
  if( !( sc->cmykfilename ) )
    sc->cmykfilename = DEFAULT_CMYK_PROFILE;
  if( !( sc->prooffilename ) )
    sc->prooffilename = DEFAULT_CMYK_PROFILE;

  if( get_last_values ) {
    gint size;
    if( size = gimp_get_data_size( "separate_displayprofile" ) ) {
      g_free( sc->displayfilename );
      sc->displayfilename = g_new( gchar, size );
      gimp_get_data( "separate_displayprofile", sc->displayfilename );
    }
    if( size = gimp_get_data_size( "separate_rgbprofile" ) ) {
      g_free( sc->rgbfilename );
      sc->rgbfilename = g_new( gchar, size );
      gimp_get_data( "separate_rgbprofile", sc->rgbfilename );
    }
    if( size = gimp_get_data_size( "separate_cmykprofile" ) ) {
      g_free( sc->cmykfilename );
      sc->cmykfilename = g_new( gchar, size );
      gimp_get_data( "separate_cmykprofile", sc->cmykfilename );
    }
    if( size = gimp_get_data_size( "separate_proofprofile" ) ) {
      g_free( sc->prooffilename );
      sc->prooffilename = g_new( gchar, size );
      gimp_get_data( "separate_proofprofile", sc->prooffilename );
    }
    gimp_get_data( "separate_settings", &( sc->ss ) );
    gimp_get_data( "separate_proofsettings", &( sc->ps ) );
  }
}

void separate_store_settings( struct SeparateContext *sc, enum separate_function func ) {
  switch( func ) {
#ifdef SEPARATE_SEPARATE
  case SEP_SEPARATE:
#endif
  case SEP_FULL:
  case SEP_LIGHT:
    if( sc->rgbfilename )
      gimp_set_data( "separate_rgbprofile", sc->rgbfilename, strlen( sc->rgbfilename ) + 1 );
    if( sc->cmykfilename )
      gimp_set_data( "separate_cmykprofile", sc->cmykfilename, strlen( sc->cmykfilename ) + 1 );
    gimp_set_data( "separate_settings", &( sc->ss ), sizeof( struct SeparateSettings ) );
    break;
  case SEP_PROOF:
    if( sc->displayfilename )
      gimp_set_data( "separate_displayprofile", sc->displayfilename, strlen( sc->displayfilename ) + 1 );
    if( sc->prooffilename )
      gimp_set_data( "separate_proofprofile", sc->prooffilename, strlen( sc->prooffilename ) + 1 );
    gimp_set_data( "separate_proofsettings", &( sc->ps ), sizeof( struct ProofSettings ) );
    break;
  default:
    break;
  } 
}

/* Create a normal RGB image for proof...*/
gint32 separate_create_RGB(gchar *filename,
	guint width, guint height, gint32 *layers)
{
  gint32 image_id;

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable( image_id );
  gimp_image_set_filename (image_id, filename);

	layers[0] = gimp_layer_new (image_id, _( "Background" ), width, height,
			      GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
	gimp_image_add_layer (image_id, layers[0], -1);
  return (image_id);
}


/* Create an image with four greyscale layers, to be used as CMYK channels...*/
gint32 separate_create_planes_grey(gchar *filename,
	guint width, guint height, gint32 *layers)
{
  gint32 image_id;

  image_id = gimp_image_new (width, height, GIMP_GRAY);
  gimp_image_undo_disable( image_id );
  gimp_image_set_filename (image_id, filename);

	layers[0] = gimp_layer_new (image_id, "K", width, height,
			      GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
	gimp_image_add_layer (image_id, layers[0], -1);
  layers[1] = gimp_layer_new (image_id, "Y", width, height,
			      GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);
  layers[2] = gimp_layer_new (image_id, "M", width, height,
				    GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[2], -1);
  layers[3] = gimp_layer_new (image_id, "C", width, height,
			      GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[3], -1);

  return (image_id);
}


/* Create an image with four colour layers with masks, to be used as CMYK channels...*/
gint32 separate_create_planes_CMYK(gchar *filename,
	guint width, guint height, gint32 *layers,guchar *primaries)
{
  gint32 image_id;
  gint32 background_id;
  gint32 ntiles,tilecounter;
  gint counter;
  gpointer tileiterator;
  GimpPixelRgn pixrgn[5];
  GimpDrawable *drawables[5];
  guchar rs[4]={};
  guchar gs[4]={};
  guchar bs[4]={};
  guchar as[]={255,255,255,255};

  for( counter=0; counter < 4; ++counter ) {
    rs[counter]=*primaries++;
    gs[counter]=*primaries++;
    bs[counter]=*primaries++;
  }

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable( image_id );
  gimp_image_set_filename (image_id, filename);

  background_id = gimp_layer_new (image_id, _( "Background" ), width, height,
                                  GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, background_id, -1);
  layers[0] = gimp_layer_new (image_id, "K", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[0], -1);
  layers[1] = gimp_layer_new (image_id, "Y", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);
  layers[2] = gimp_layer_new (image_id, "M", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[2], -1);
  layers[3] = gimp_layer_new (image_id, "C", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[3], -1);

  for(counter=0; counter < 4; ++counter ) {
    drawables[counter]=gimp_drawable_get(layers[counter]);
    gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);
  }
  drawables[4]=gimp_drawable_get(background_id);
  gimp_pixel_rgn_init (&pixrgn[4], drawables[4], 0, 0, width, height, TRUE, FALSE);

  gimp_progress_init (_( "Creating CMYK layers..." ) );

  ntiles=drawables[0]->ntile_rows*drawables[0]->ntile_cols;
  tilecounter=0;
  tileiterator=gimp_pixel_rgns_register(5,&pixrgn[0],&pixrgn[1],&pixrgn[2],&pixrgn[3],&pixrgn[4]);

  while( tileiterator ) {
    long i;
    guchar *destptr[5];
    for(counter=0;counter<5;++counter)
      destptr[counter]=pixrgn[counter].data;
    for( i=0; i < (pixrgn[0].w*pixrgn[0].h); ++i ) {
      for( counter=0; counter < 4; ++counter ) {
        (destptr[counter])[i*pixrgn[counter].bpp]=rs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+1]=gs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+2]=bs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+3]=as[counter];
      }
      (destptr[4])[i*pixrgn[4].bpp]=255;
      (destptr[4])[i*pixrgn[4].bpp+1]=255;
      (destptr[4])[i*pixrgn[4].bpp+2]=255;
    }
    ++tilecounter;
    gimp_progress_update (((double) tilecounter) / ((double) ntiles));
    tileiterator = gimp_pixel_rgns_process (tileiterator);
  }

  for( counter=0; counter < 5; ++counter ) {
    gimp_drawable_flush (drawables[counter]);
    gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
    gimp_drawable_detach (drawables[counter]);
  }

  return (image_id);
}


/* Create an image with two colour layers with masks, to be used as MK duotone channels...*/
gint32 separate_create_planes_Duotone(gchar *filename,
	guint width, guint height, gint32 *layers)
{
  gint32 image_id;
  gint32 background_id;
  gint32 ntiles,tilecounter;
  gint counter;
  gpointer tileiterator;
  GimpPixelRgn pixrgn[3];
  GimpDrawable *drawables[3];

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable( image_id );
  gimp_image_set_filename (image_id, filename);

  background_id = gimp_layer_new (image_id, _( "Background" ), width, height,
                                  GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, background_id, -1);

  layers[0] = gimp_layer_new (image_id, "K", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[0], -1);

  layers[1] = gimp_layer_new (image_id, "M", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);

  for( counter=0; counter < 2; ++counter ) {
    drawables[counter]=gimp_drawable_get(layers[counter]);
    gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);
  }
  drawables[2]=gimp_drawable_get(background_id);
  gimp_pixel_rgn_init (&pixrgn[2], drawables[2], 0, 0, width, height, TRUE, FALSE);

  gimp_progress_init ( _( "Creating CMYK layers...") );

  ntiles=drawables[0]->ntile_rows*drawables[0]->ntile_cols;
  tilecounter=0;
  tileiterator=gimp_pixel_rgns_register(3,&pixrgn[0],&pixrgn[1],&pixrgn[2]);

  while( tileiterator ) {
    long i;
    guchar rs[]={0,255};
    guchar gs[]={0,0};
    guchar bs[]={0,0};
    guchar as[]={255,255};
    guchar *destptr[3];
    for( counter=0; counter < 3; ++counter )
      destptr[counter]=pixrgn[counter].data;
    for( i=0; i < ( pixrgn[0].w * pixrgn[0].h ); ++i ) {
      for( counter=0; counter < 2; ++counter ) {
        (destptr[counter])[i*pixrgn[counter].bpp]=rs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+1]=gs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+2]=bs[counter];
        (destptr[counter])[i*pixrgn[counter].bpp+3]=as[counter];
      }
      (destptr[2])[i*pixrgn[2].bpp]=255;
      (destptr[2])[i*pixrgn[2].bpp+1]=255;
      (destptr[2])[i*pixrgn[2].bpp+2]=255;
    }
    ++tilecounter;
    gimp_progress_update (((double) tilecounter) / ((double) ntiles));
    tileiterator = gimp_pixel_rgns_process (tileiterator);
  }

  for( counter=0; counter < 3; ++counter ) {
    gimp_drawable_flush (drawables[counter]);
    gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
    gimp_drawable_detach (drawables[counter]);
  }

  return (image_id);
}


char *separate_build_filename(char *root,char *suffix)
{
	/* Build a filename like <imagename>-<channel>.<extension> */
	char *filename;
	char *extension;
	root = g_strdup(root);
        extension = root + strlen (root) - 1;
	while (extension >= root)
	{
		if (*extension == '.') break;
		extension--;
	}
	if (extension >= root)
	{
		*(extension++) = '\0';
		filename = g_strdup_printf ("%s-%s.%s", root, suffix, extension);
	}
	else
		filename = g_strdup_printf ("%s-%s", root, suffix);
	g_free(root);

	return(filename);
}


char *separate_filename_add_suffix(char *root,char *suffix)
{
	/* Build a filename like <imagename>-<channel>.<extension> */
	char *filename;
	char *extension;

	if(root==NULL)
		return(g_strdup_printf(_( "Untitled-%s.tif" ),suffix));

	root=g_strdup(root);
	extension = root + strlen (root) - 1;

	while (extension >= root)
	{
		if (*extension == '.') break;
		extension--;
	}
	if (extension >= root)
	{
		*(extension++) = '\0';
	}
	filename = g_strdup_printf ("%s-%s.tif", root, suffix);
	g_free(root);

	return(filename);
}


char *separate_filename_change_extension(char *root,char *newext)
{
	/* Change <imagename>.<extension> to <imagename>.<tif> */
	char *filename;
	char *extension;
	root=g_strdup(root);
	extension = root + strlen (root) - 1;
	while (extension >= root)
	{
		if (*extension == '.') break;
		extension--;
	}
	if (extension >= root)
	{
		*extension++ = 0;
		filename = g_strdup_printf ("%s.%s", root, newext);
	}
	else
		filename = g_strdup(root);
	g_free(root);

	return(filename);
}

