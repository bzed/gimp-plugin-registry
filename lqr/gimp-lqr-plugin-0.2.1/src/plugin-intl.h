/* plugin-intl.h
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

#ifndef __PLUGIN_INTL_H__
#define __PLUGIN_INTL_H__


#ifndef GETTEXT_PACKAGE
#error "config.h must be included prior to plugin-intl.h"
#endif

#include <libintl.h>

#define _(String) gettext (String)

#ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#else
#    define N_(String) (String)
#endif


typedef enum _LqrGradFunc LqrGradFunc;

/**** gradient functions for energy evluation ****/
enum _LqrGradFunc
{
  LQR_GF_NORM,                         /* gradient norm : sqrt(x^2 + y^2)            */
  LQR_GF_NORM_BIAS,                    /* gradient biased norm : sqrt(x^2 + 0.1 y^2) */
  LQR_GF_SUMABS,                       /* sum of absulte values : |x| + |y|          */
  LQR_GF_XABS,                         /* x absolute value : |x|                     */
  LQR_GF_YABS,                         /* y absolute value : |y|                     */
  LQR_GF_NULL			       /* 0 */
};



#endif /* __PLUGIN_INTL_H__ */
