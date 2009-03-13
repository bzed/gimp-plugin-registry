/* GIMP Dustcleaner
 * Copyright (C) 2006-2008  Frank Tao<solotim.cn@gmail.com> (the "Author").
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the Author of the
 * Software shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the Author.
 */

#include "config.h"

#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "dust.h"
#include "dustcleaner_gimp_plugin.h"


static int findDust(CvMat* mat, TPara para);

void process_it (GimpDrawable *drawable, GimpPreview  *preview, TPara *para)
{
	gint         channels;
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
	findDust(&mat, *para);	

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
}

static int findDust(CvMat* mat, TPara para)
{	
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
	}

    
	// free mem
	int i;
	for (i=0; i<blockSeq->total; i++) {
		DustBlock* next = (DustBlock*)cvGetSeqElem( blockSeq, i );
		freeDustBlock(next, 0);		
	}
printf( "DEBUG＝＝103\n");

	cvReleaseMemStorage( &frgStg );  
	cvReleaseMemStorage( &blkStg );  	
	cvReleaseImageHeader(&header);
	cvReleaseImage(&grayImg);	

	return 1;
}

