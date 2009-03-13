#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"

#include "separate.h"
#include "platform.h"
#include "util.h"

#include <tiffio.h>

#define STRIPHEIGHT 64

static
int separate_writetiffdata(TIFF* out,gint32 imageid,gint32 width,gint32 height)
{
	int result=TRUE;
	int32 BufSizeOut = TIFFStripSize(out);
	unsigned char *BufferOut;
	int i;
	int StripCount = (height+(STRIPHEIGHT-1))/STRIPHEIGHT;
	int32 sw=width;
	int32 sl=STRIPHEIGHT;

	GimpDrawable *drw[4];
	GimpPixelRgn pixrgn[4];
  gchar *chanbuf[4];

	drw[0]=separate_find_channel(imageid,sep_C);
	drw[1]=separate_find_channel(imageid,sep_M);
	drw[2]=separate_find_channel(imageid,sep_Y);
	drw[3]=separate_find_channel(imageid,sep_K);

	for(i=0;i<4;++i)
	{
		if(!(chanbuf[i]=malloc(sw*sl)))
		  result=FALSE;
		if(drw[i])
			gimp_pixel_rgn_init(&pixrgn[i],drw[i],0,0,width,height,FALSE,FALSE);
	}
	if(!(result))
		return(FALSE);

	BufferOut = (unsigned char *) _TIFFmalloc(BufSizeOut);
	if (!BufferOut)
		return(FALSE);

	gimp_progress_init (_("Saving..."));

	for (i = 0; i < StripCount; i++)
	{
		int j;
		unsigned char *src[4]={NULL,NULL,NULL,NULL};
		unsigned char *dest=BufferOut;
		int x,y;
		gimp_progress_update(((double) i) / ((double) StripCount));

		for(j=0;j<4;++j)
		{
			if(drw[j])
			{
				int left,top,wd,ht;
				left=0;
				top=i*STRIPHEIGHT;
				wd=width;
				ht=(top+STRIPHEIGHT > height) ? height-top : STRIPHEIGHT;
				src[j]=chanbuf[j];
				gimp_pixel_rgn_get_rect(&pixrgn[j],src[j],left,top,wd,ht);
			}
		}
		for(y=0;y<sl;++y)
		{
			for(x=0;x<sw;++x)
			{
				if(src[0])
					*dest++=*src[0]++;
				else
					*dest++=0;
				if(src[1])
					*dest++=*src[1]++;
				else
					*dest++=0;
				if(src[2])
					*dest++=*src[2]++;
				else
					*dest++=0;
				if(src[3])
					*dest++=*src[3]++;
				else
					*dest++=0;
			}
		}
		TIFFWriteEncodedStrip(out, i, BufferOut, BufSizeOut);
	}

	_TIFFfree(BufferOut);
	return 1;
}


void separate_save( GimpDrawable *drawable, struct SeparateContext *sc )
{
  gint32 imageid = sc->imageID;//gimp_drawable_get_image( drawable->drawable_id );
  gchar *filename = gimp_image_get_filename( imageid );
  gint32 width,height;
  gdouble xres,yres;
  TIFF *out;

  gimp_image_get_resolution( imageid, &xres, &yres );
  width=gimp_image_width(imageid);
  height=gimp_image_height(imageid);

#ifdef G_OS_WIN32
  gchar *_filename; // win32 filename encoding(not UTF8)
  _filename = g_win32_locale_filename_from_utf8( filename );
  out = TIFFOpen( _filename != NULL ? _filename : filename, "w" );
  g_free( _filename );
#else
  out = TIFFOpen( filename, "w" );
#endif
  g_free( filename );

  if( out ) {
#ifdef ENABLE_COLOR_MANAGEMENT
    gsize length;
    gchar *buf = NULL;
    GimpParasite *parasite = NULL;
    cmsHPROFILE hProfile = NULL;

    if( sc->sas.embedprofile == 3 ) {
      parasite = gimp_image_parasite_find( imageid, CMYKPROFILE );

      if( parasite ) {
        length = gimp_parasite_data_size( parasite );
        buf = gimp_parasite_data( parasite );
      }
    } else {
      gchar *profilefilename;
      switch( sc->sas.embedprofile ) {
      case 1:
        profilefilename = sc->cmykfilename;
        break;
      case 2:
        profilefilename = sc->prooffilename;
        break;
      default:
        profilefilename = "";
      }
      g_file_get_contents( profilefilename, &buf, &length, NULL );
    }
    if( buf ) {
      if( ( hProfile = cmsOpenProfileFromMem( buf, length ) ) &&
          cmsGetColorSpace( hProfile ) == icSigCmykData ) { /* profile is OK? */
        /* Profile is embedded, and cannot be used independently */
        buf[47] |= 2;

        TIFFSetField( out, TIFFTAG_ICCPROFILE, length, buf );
      }

      if( hProfile )
        cmsCloseProfile( hProfile );

      if( parasite )
        gimp_parasite_free( parasite );
      else
        g_free( buf );
    }
#endif

    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_INKSET, INKSET_CMYK);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(out, TIFFTAG_XRESOLUTION, xres);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, yres);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, STRIPHEIGHT);
    separate_writetiffdata(out,imageid,width,height);

    TIFFWriteDirectory(out);
    TIFFClose(out);
  }
}
