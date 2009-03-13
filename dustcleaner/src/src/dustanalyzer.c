/*
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
*/
/*
 * dustanalyzer.c 
 * Author: Frank.Tao
 * Date: 2006.10.16
 * Version: 0.1.01
 */

#include <math.h>
#include "dust.h"

const int margin = 8;

int findDustArea( const IplImage *image, DustBlock* dust, TPara *para )
{
	int  j, threshold; 
	double min_val = 0, max_val = 0;
	CvRect rect;
	CvMat ma, *ma_gray, *ma_equl, *ma_bin,  *ma_bin_tmp, *ma_bin_final;
	//CvMat *ma_0, *ma_1, *ma_2, *mb, *mc, *md;	
	CvPoint seed;	
	

	// get subimage containing dust
	rect = cvRect( dust->col0, 
					dust->row0, 
					dust->col1 - dust->col0 + 1,
					dust->row1 - dust->row0 + 1 );			
	cvGetSubRect( image, &ma, rect );		
	
	// get gray image of the subimage
	ma_gray = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvCvtColor(&ma, ma_gray, CV_BGR2GRAY); 	
	
	// get the darkest point of ma_gray, which is deemed as seed	
	cvMinMaxLoc( ma_gray, &min_val, &max_val, &seed, NULL, NULL );
	cvReleaseMat( &ma_gray );
	
	// enlarge dust rectangle
	int crop_top = intMax( 0, dust->row0 - margin );
	int crop_left = intMax( 0, dust->col0 - margin );
	int crop_bottom = intMin( image->height-1, dust->row1 + margin );
	int crop_right = intMin( image->width-1, dust->col1 + margin );	
		
	// get subimage containing dust again for enlarged subimage
	rect = cvRect( crop_left, 
					crop_top, 
					crop_right - crop_left + 1,
					crop_bottom - crop_top + 1 );			
	cvGetSubRect( image, &ma, rect );	
	seed.x += dust->col0 - crop_left;    // change coordinate
	seed.y += dust->row0 - crop_top;	  // change coordinate
	
	// get gray image of the subimage
	ma_gray = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvCvtColor(&ma, ma_gray, CV_BGR2GRAY); 	
	
	// get equalize-histogramed image from ma_gray
	ma_equl = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvEqualizeHist( ma_gray, ma_equl );
	
	// calculate the gray threshold for segmentation	
	LinkedFragVector* element = dust->childFrag;
	double avg = 0;
	int point_count = 0;
	while( element != NULL ) {
		DustFragment* frg = element->dustFragment;
		for ( j=frg->row; j<frg->row + frg->step; j++ ) {
			int row = j - crop_top;
			int col1 = frg->left - crop_left;
			int col2 = frg->right - crop_left;
			avg += cvGetReal2D( ma_equl, row, col1 );
			avg += cvGetReal2D( ma_equl, row, col2 );
			point_count += 2;
		}	
		element = element->next;	
	}
	avg = avg/point_count;	
	threshold = (int)avg - ( avg - min_val )*(255-max_val + min_val)/255;
	threshold += para->segmentationSensitivity -50; 	// right expression mapped to (-50,50)
	threshold = intMin(255, intMax(0, threshold));
	//printf("@%d\n",threshold);
	
	// segmentation
	ma_bin = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvThreshold( ma_equl,  ma_bin, threshold, 255, CV_THRESH_BINARY );	
	
	// floodfill from seed
	ma_bin_tmp = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvCopy(ma_bin, ma_bin_tmp, NULL);	
	cvFloodFill( ma_bin_tmp, seed, cvRealScalar(255), cvRealScalar(0), cvRealScalar(0),	NULL, 4, NULL );
	
	// get dust area mask	
	cvSub( ma_bin_tmp, ma_bin,  ma_bin_tmp, NULL );
	
	// Morphological Open 	
	if ( para->hasContourMorphologyEx ) {		
		ma_bin_final = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
		IplConvKernel* kernel = cvCreateStructuringElementEx( 3, 3, 
																	1, 1,
																	CV_SHAPE_RECT, 
																	NULL );		
		cvMorphologyEx( ma_bin_tmp, ma_bin_final, NULL, kernel, CV_MOP_CLOSE, 1 );	
		cvReleaseStructuringElement( &kernel );
	} else {
		ma_bin_final = cvCloneMat(ma_bin_tmp);
	}
	
	// margin offset
	cvReleaseMat(&ma_bin_tmp);
	ma_bin_tmp = cvCloneMat(ma_bin_final);
	if ( para->margin > 0 ) {		
		cvDilate( ma_bin_tmp, ma_bin_final, NULL, para->margin );				
	} else if ( para->margin < 0 ) {		
		cvErode( ma_bin_tmp, ma_bin_final, NULL, -1 * para->margin );	
	} else {
	}

	// write data to DustBlock
	dust->colorOffset = ma_bin_final;
	dust->offsetX = crop_left;
	dust->offsetY = crop_top;
	
	
	// show test
	/*cvNamedWindow("dd", 1);
	cvShowImage("dd", ma_bin_tmp);
	cvWaitKey(0);
	cvNamedWindow("test", 1);	
	cvShowImage("test", dust->colorOffset);
	cvWaitKey(0);*/

	/*
	ma_0 = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	ma_1 = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	ma_2 = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );		
	cvSplit( &ma, ma_0, ma_1, ma_2, 0 );

	printf("%d,%d,%d\n",ma.rows, ma.cols,ma_0->type);					
	
	*/	
	
	// free memory
	cvReleaseMat(&ma_gray);
	cvReleaseMat(&ma_equl);
	cvReleaseMat(&ma_bin);
	cvReleaseMat(&ma_bin_tmp);
	
	// ma_bin_final has been deliverred to dust->colorOffset, should not be release here
	//cvReleaseMat(&ma_bin_final);  ! can't !
	return 0;
}



int calcuThresholdAndSeed( CvArr* mat, DustBlock* dustBlk, CvPoint* seed )
{
	int i = 100;    // @TODO WRONG
	double thr = 0.0;	
	LinkedFragVector* element = dustBlk->childFrag;
	int darkness = 255;

	while( element != NULL ) {
		DustFragment fg = *(element->dustFragment);		
		int row = fg.row - dustBlk->row0 + fg.step/2;
		int col1 = fg.left - dustBlk->col0;
		int col2 = fg.right - dustBlk->col0;
		row = intMin( intMax( 0, row ), dustBlk->row1 - dustBlk->row0 );
		col1 = intMin( intMax( 0, col1 ), dustBlk->col1 - dustBlk->col0 );
		col2 = intMin( intMax( 0, col2 ), dustBlk->col1 - dustBlk->col0 );
		thr += cvGetReal2D( mat, row, col1 );
		thr += cvGetReal2D( mat, row, col2 );
		
		double k = cvGetReal2D( mat, row, col1 + (fg.right - fg.left)/2 );
		if ( k < darkness ) {
			darkness = k;
			seed->x = col1 + (fg.right - fg.left)/2;
			seed->y = row;
		}
		element = element->next;
	}
	return (int)thr/(2*i);
}

/* Calculate the the 3-channels offset between dust area and it's around
 * parameter:  
 * IplImage *image: source image
 * DustBlock *dust: dust block object
 * TPara *para: configuration parameters
 * return: 
 * CvMat*: the pointer refer to color offset matrix, which is a member of DustBlock
 */
CvMat* distillColorOffset( const IplImage *image, DustBlock* dust, TPara *para)
{
	//int i, j;	
	CvMat ma, *reverse_mask, *m_tmp, *m_offset;
	CvRect rect;

	// get subimage containing dust
	rect = cvRect( dust->offsetX, 
					dust->offsetY, 
					dust->colorOffset->cols,
					dust->colorOffset->rows );			
	cvGetSubRect( image, &ma, rect );		
	
	// calculate reverse mask from colorOffset
	reverse_mask = cvCreateMat( ma.rows, ma.cols, CV_8UC1 );
	cvConvertScaleAbs( dust->colorOffset, reverse_mask, -1, 255 );	

	// calculate average value of no-dust area
	CvScalar avg = cvAvg( &ma, reverse_mask );
	//printf("kkkk   %f,%f,%f,%f\n",avg.val[0],avg.val[1],avg.val[2],avg.val[3]);
	
	// calculate color offset of dust area
	m_tmp = cvCloneMat( &ma );
	cvSet( m_tmp, avg, dust->colorOffset );	
	m_offset = cvCreateMat( ma.rows, ma.cols, CV_8UC3 );
	cvSub( m_tmp, &ma, m_tmp, NULL );
	
	// smooth the offset    (neccessary?)
	cvSmooth( m_tmp, m_offset, CV_GAUSSIAN, 3, 1, 0, 0 );
	
	// mapping offset	
	mapping( &ma, m_offset, m_offset, para->compensationMap, 0 );
	
	/*cvAdd( &ma, m_offset, m_tmp, NULL );
	cvNamedWindow("dd", 1);
	cvShowImage("dd", m_tmp);
	cvWaitKey(0);*/
	
	// reassign dust->colorOffset
	cvReleaseMat( &dust->colorOffset );
	dust->colorOffset = m_offset;
	
	//cvSaveImage( "marked.jpg", markedImage );
	//printf( "--Marked image has been saved in current path, namely ./marked.jpg, please check it.\n");	
		
	cvReleaseMat( &reverse_mask );
	cvReleaseMat( &m_tmp );
	// m_offset should not be released, since it's assigned to dust->colorOffset
	return dust->colorOffset;
}



int mapping( const CvMat* src, const CvMat* offset, CvMat* dst, double* map, int isReverse ) 
{
	int i, j;
	/*CvMat *mt1, *mt2, *mt3, *mo1, *mo2, *mo3;
	mt1 = cvCreateMat( src->rows, src->cols, CV_8UC1 );
	mt2 = cvCreateMat( src->rows, src->cols, CV_8UC1 );
	mt3 = cvCreateMat( src->rows, src->cols, CV_8UC1 );
	mo1 = cvCreateMat( src->rows, src->cols, CV_8UC1 );
	mo2 = cvCreateMat( src->rows, src->cols, CV_8UC1 );
	mo3 = cvCreateMat( src->rows, src->cols, CV_8UC1 );*/
		
	
	//cvSplit( src, mt1, mt2, mt3, NULL );
	//cvSplit( offset, mo1, mo2, mo3, NULL );

	for ( i=0; i<src->rows; i++ ) {
		for ( j=0; j<src->cols; j++ ) {
			CvScalar s_val = cvGet2D( src, i, j );			
			CvScalar o_val = cvGet2D( offset, i, j );
			CvScalar d_val = cvScalarAll( 0 );
			int ch = 0;
			while ( ch<3 ) {
				int index = (int)s_val.val[ch];
				if (isReverse) {
					d_val.val[ch] = o_val.val[ch] * map[index]/map[255];
				} else {
					d_val.val[ch] = o_val.val[ch] * map[255]/map[index];	
				}				
				ch++;
			}		
			cvSet2D( dst, i, j, d_val );
		}
	}	
	return 1;
}

