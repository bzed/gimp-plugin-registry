	DustCleaner is a GIMP plugin to detect and remove the dust spots 
	in digital image.
	Copyright (C) 2006-2007  Frank Tao<solotim.cn@gmail.com>
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
	02110-1301, USA        	
 * plugin main 
 * Author: Frank.Tao<solotim.cn@gmail.com>
 * Date: 2007.04.12
 * Version: 0.1
#include <libgimp/gimp.h>
#include <cv.h>
#include <gtk/gtk.h>
#include "dustcleaner_gimp_plugin.h"
static TPara para = {
			NULL,			//uchar **ptr;
			{},				//double compensationMap[256];
    			{8, 8},			//CvSize dustSize;    
    			60,				//int sensitivity; // 1-100
    			8,				//int scanStep;
    			120,			//int noiseReductionCoreLenght;
    			1,				//int noiseReductionCorePower;
    			1,				//int hasContourMorphologyEx;
    			0,				//int kernelSize;
    			CV_GAUSSIAN,	//int kernelType;    			
			0,				//int isSave;
    			DUST_DETECTION,	//int op;
    			0,				//int margin;
    			50,				//int segmentationSensitivity;
    			0				//int recoveryStrength;
GimpPlugInInfo PLUG_IN_INFO =
  NULL,
  NULL,
  query,
  run
MAIN()
static void
query (void)
  static GimpParamDef args[] =
    {
      GIMP_PDB_INT32,
      "run-mode",
      "Run mode"
    },
    {
      GIMP_PDB_IMAGE,
      "image",
      "Input image"
    },
    {
      GIMP_PDB_DRAWABLE,
      "drawable",
      "Input drawable"
    }
  };
  gimp_install_procedure (
    "plug-in-dustcleaner",
    "DustCleaner beta",
    "Remove the dust spot of image",
    "Frank Tao<solotim.cn@gmail.com>",
    "Copyright Frank Tao",
    "2007",
    "_DustCleaner (beta)",
    "RGB*, GRAY*",
    GIMP_PLUGIN,
    G_N_ELEMENTS (args), 0,
    args, NULL);
  gimp_plugin_menu_register ("plug-in-dustcleaner",
                             "<Image>/Filters/Misc");
static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
  static GimpParam  values[1];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  GimpRunMode       run_mode;
  GimpDrawable     *drawable;
  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals  = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  /* Getting run_mode - we won't display a dialog if
   * we are in NONINTERACTIVE mode
   */
  run_mode = param[0].data.d_int32;
  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[2].data.d_drawable);
  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /* Get options last values if needed */
      gimp_get_data ("plug-in-dustcleaner-para", &para);
      /* Display the dialog */
      if (! show_dustcleaner_dialog (drawable, &para))
        return;
      break;
    case GIMP_RUN_NONINTERACTIVE:
      if (nparams != 4)
        status = GIMP_PDB_CALLING_ERROR;
      if (status == GIMP_PDB_SUCCESS)
        para.sensitivity = param[3].data.d_int32;  //============???
      break;
    case GIMP_RUN_WITH_LAST_VALS:
      /*  Get options last values if needed  */
      gimp_get_data ("plug-in-dustcleaner-para", &para);
      break;
    default:
      break;
    }
  /*  
   */
  process_it (drawable, NULL);
  /*   g_print ("blur() took %g seconds.\n", g_timer_elapsed (timer));
   *   g_timer_destroy (timer);
   */
  gimp_displays_flush ();
  gimp_drawable_detach (drawable);
  /*  Finally, set options in the core  */
  if (run_mode == GIMP_RUN_INTERACTIVE)
    gimp_set_data ("plug-in-dustcleaner-para", &para, sizeof (TPara));
    
  return;
void process_it (GimpDrawable *drawable, GimpPreview  *preview)
	gint         i, j, k, channels;
	gint         x1, y1, x2, y2;
	gint         width, height;
	GimpPixelRgn rgn_in, rgn_out;
	guchar      *rect;
	CvMat		mat;
	if (! preview)
		gimp_progress_init ("Dust Cleaner...");
  /* Gets upper left and lower right coordinates,
   * and layers number in the image */
  if (preview)
    {
      gimp_preview_get_position (preview, &x1, &y1);
      gimp_preview_get_size (preview, &width, &height);
      x2 = x1 + width;
      y2 = y1 + height;
    }
  else
    {
	gimp_drawable_mask_bounds (drawable->drawable_id,
	                        &x1, &y1,
	                        &x2, &y2);
      width = x2 - x1;
      height = y2 - y1;
    }   
	channels = gimp_drawable_bpp (drawable->drawable_id);
	/* Region for reading */
	gimp_pixel_rgn_init (&rgn_in,
	                drawable,
	                x1, y1,
	                x2 - x1, y2 - y1,
	                FALSE, FALSE);
	/* Region for writting */
	gimp_pixel_rgn_init (&rgn_out,
	                drawable,
	                x1, y1,
	                x2 - x1, y2 - y1,
	                preview == NULL, TRUE);
	/* Initialise enough memory for rect*/
	rect = g_new (guchar, channels * width * height);
	/* Put raw data from rgn_in to rect */
	gimp_pixel_rgn_get_rect (&rgn_in,
	                                        rect,
	                                        x1, y1,
							   width,
							   height);
	/*****************************************************************/
	/* 				Process Here						    */	
	/*****************************************************************/
	/* Create OpenCV matrix object from raw data */
	mat = cvMat( height, width, CV_8UC3, rect );
	findDust(&mat);	
	/* Set modified raw data back to writable region */
	gimp_pixel_rgn_set_rect (&rgn_out,
	                                        rect,
	                                        x1, y1,
	                                        width,
	                                        height);		
	/* Free memory */
	g_free (rect); 
  /*  Update the modified region */
  if (preview)
    {
      gimp_drawable_preview_draw_region (GIMP_DRAWABLE_PREVIEW (preview),
                                         &rgn_out);
    }
  else
    {
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id,
                            x1, y1,
                            width, height);
    }
	return;
static int findDust(CvMat* mat)
	IplImage *header, *img, *grayImg, *img_bak;	
	header = cvCreateImageHeader(cvGetSize(mat),IPL_DEPTH_8U,3);
	// Create the output image    
	img =  cvGetImage(mat, header);
	// create gray scale image
	grayImg = cvCreateImage(cvGetSize(mat), IPL_DEPTH_8U, 1); 
	cvCvtColor(img, grayImg, CV_BGR2GRAY); 
	printf("TPara:\ndustSize.width=%d\t,", para.dustSize.width);
	printf("dustSize.height=%d,\n", para.dustSize.height);
	printf("sensitivity=%d,\t", para.sensitivity);
	printf("scanStep=%d,\n", para.scanStep);
	printf("noiseReductionCoreLenght=%d,\t", para.noiseReductionCoreLenght);
	printf("noiseReductionCorePower=%d,\n", para.noiseReductionCorePower);
	printf("hasContourMorphologyEx=%d,\t", para.noiseReductionCorePower);
	printf("op=%d,\n", para.op);
	printf("kernelSize=%d,\t", para.kernelSize);
	printf("kernelType=%d,\n", para.kernelType);
	printf("margin=%d,\n", para.margin);
	printf("segmentationSensitivity=%d,\n", para.segmentationSensitivity);
	printf("recoveryStrength=%d,\n", para.recoveryStrength);
	CvSeq *frgSeq, *blockSeq;
	// initialize sequence of dust fragment	
	CvMemStorage* frgStg = cvCreateMemStorage(0);
	frgSeq = cvCreateSeq(  CV_SEQ_ELTYPE_GENERIC, sizeof(CvSeq), sizeof(DustFragment), frgStg );			
	CvMemStorage* blkStg = cvCreateMemStorage(0);
	blockSeq = cvCreateSeq(  CV_SEQ_ELTYPE_GENERIC, sizeof(CvSeq), sizeof(DustBlock), blkStg );	
	// initialization
	initialSearch( grayImg, frgSeq, blockSeq, &para );
	// start dust searching ============
	searchDust(grayImg, frgSeq, &para);		    
	cvtFragToBlock( frgSeq, blockSeq, &para);	
	// end dust searching ============
	switch (para.op) {
		case DUST_DETECTION:		
			img_bak = cvCloneImage( img );
			// mark image
			markDust( img_bak, img, blockSeq, &para );
			cvReleaseImage(&img_bak);				
		break;
		case DUST_RECOVERY_INSTANTLY:
			// don't mark image
			markDust( img, NULL, blockSeq, &para );
			// record dust mask
			recordDust( img, blockSeq, &para );
			// recovery image
			recoveryImage( img, blockSeq, &para );
			// free mem!!!! @todo
		break;
		case DUST_RECOVERY_BY_MASK:
		break;
		default:
		break;
    
	// free mem
	int i;
	for (i=0; i<blockSeq->total; i++) {
		DustBlock* next = (DustBlock*)cvGetSeqElem( blockSeq, i );
		freeDustBlock(next, 0);		
printf( "DEBUG
103\n");
	cvReleaseMemStorage( &frgStg );  
	cvReleaseMemStorage( &blkStg );  	
	cvReleaseImageHeader(&header);
	cvReleaseImage(&grayImg);	
	return 1;
