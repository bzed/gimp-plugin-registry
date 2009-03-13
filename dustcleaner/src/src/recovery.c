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
 * recovery.c 
 * Author: Frank.Tao
 * Date: 2006.11.5
 * Version: 0.1.01
 */
 

#include "dust.h"


int recoveryImage( IplImage *img, CvSeq *blockSeq, TPara *para )
{
	int i;
	for (i=0; i<blockSeq->total; i++) {
		DustBlock* dust = (DustBlock*)cvGetSeqElem( blockSeq, i );
		CvMat* m_offset = dust->colorOffset;
		CvMat ma, *m_tmp;
		CvRect rect;
				
		// get subimage containing dust
		rect = cvRect( dust->offsetX, 
								dust->offsetY, 
								dust->colorOffset->cols,
								dust->colorOffset->rows );			
		cvGetSubRect( img, &ma, rect );								
		
		if ( para->recoveryStrength > 0 ) {
			double scale = para->recoveryStrength/10.0 + 1;
			//printf("%f,%d", scale,para->recoveryStrength);
			m_tmp = cvCreateMat( m_offset->rows, m_offset->cols, CV_8UC3 );
			cvSet( m_tmp, cvScalarAll(1), NULL );
			cvMul( m_offset, m_tmp, m_offset, scale );
			cvReleaseMat(&m_tmp);
		}
		
		if ( para->kernelSize > 1 ) {			
			int s = para->kernelSize % 2 == 0 ? para->kernelSize + 1 : para->kernelSize;
			m_tmp = cvCloneMat(m_offset);
			cvSmooth( m_tmp, m_offset, para->kernelType,
               s, s, 1, 1 );			
			cvReleaseMat(&m_tmp);
		} 
		mapping( &ma, m_offset, m_offset, para->compensationMap, 1 );			
		cvAdd( &ma, m_offset, &ma, NULL );	
	}
    return i;
}

