/*-----------------------------------------------------------------------------

    File        : greycstoration4integration.cpp

    Description : Example of used of the GREYCstoration_4integration plug-in.

		  (see http://www.greyc.ensicaen.fr/~dtschump/greycstoration/)

   THIS VERSION IS FOR DEVELOPERS ONLY. IT SHOWS AN EXAMPLE OF HOW THE
   INTEGRATION OF THE GREYCSTORATION ALGORITHM CAN BE DONE IN
   THIRD PARTIES SOFTWARES. IF YOU ARE A "USER" OF GREYCSTORATION,
   PLEASE RATHER LOOK AT THE FILE 'greycstoration.cpp' WHICH IS THE
   SOURCE OF THE COMPLETE COMMAND LINE GREYCSTORATION TOOL.
   THE EXAMPLE FOCUS ON THE DENOISING ALGORITHM. FOR INPAINTING AND
   IMAGE RESIZING, PLEASE LOOK AT THE COMPLETE GREYCSTORATION SOURCE CODE
   (FILE 'greycstoration.cpp')

    Copyright  : David Tschumperle - http://www.greyc.ensicaen.fr/~dtschump/

  This software is governed by the CeCILL  license under French law and
  abiding by the rules of distribution of free software.  You can  use,
  modify and/ or redistribute the software under the terms of the CeCILL
  license as circulated by CEA, CNRS and INRIA at the following URL
  "http://www.cecill.info".

  As a counterpart to the access to the source code and  rights to copy,
  modify and redistribute granted by the license, users are provided only
  with a limited warranty  and the software's author,  the holder of the
  economic rights,  and the successive licensors  have only  limited
  liability.

  In this respect, the user's attention is drawn to the risks associated
  with loading,  using,  modifying and/or developing or reproducing the
  software by the user in light of its specific status of free software,
  that may mean  that it is complicated to manipulate,  and  that  also
  therefore means  that it is reserved for developers  and  experienced
  professionals having in-depth computer knowledge. Users are therefore
  encouraged to load and test the software's suitability as regards their
  requirements in conditions enabling the security of their systems and/or
  data to be ensured and,  more generally, to use and operate it in the
  same conditions as regards security.

  The fact that you are presently reading this means that you have had
  knowledge of the CeCILL license and that you accept its terms.

  ------------------------------------------------------------------------------*/

// Include the CImg Library, with the GREYCstoration plugin included
#define cimg_plugin "plugins/greycstoration.h"
#if cimg_OS!=2
#include <pthread.h>
#endif
#include "../CImg.h"
using namespace cimg_library;

// The undef below is necessary when using a non-standard compiler.
#ifdef cimg_use_visualcpp6
#define std
#endif

// Main procedure
//----------------
int main(int argc,char **argv) {

  // Read algorithm parameters from the command line
  const char *file_i         = cimg_option("-i","img/milla.ppm","Input file");
  const float amplitude      = cimg_option("-dt",40.0f,"Regularization strength for one iteration (>=0)");
  const unsigned int nb_iter = cimg_option("-iter",3,"Number of regularization iterations (>0)");
  const float sharpness      = cimg_option("-p",0.8f,"Contour preservation for regularization (>=0)");
  const float anisotropy     = cimg_option("-a",0.8f,"Regularization anisotropy (0<=a<=1)");
  const float alpha          = cimg_option("-alpha",0.6f,"Noise scale(>=0)");
  const float sigma          = cimg_option("-sigma",1.1f,"Geometry regularity (>=0)");
  const bool fast_approx     = cimg_option("-fast",true,"Use fast approximation for regularization (0 or 1)");
  const float gauss_prec     = cimg_option("-prec",2.0f,"Precision of the gaussian function for regularization (>0)");
  const float dl = cimg_option("-dl",0.8f,"Spatial integration step for regularization (0<=dl<=1)");
  const float da = cimg_option("-da",30.0f,"Angular integration step for regulatization (0<=da<=90)");
  const unsigned int interp = cimg_option("-interp",0,"Interpolation type (0=Nearest-neighbor, 1=Linear, 2=Runge-Kutta)");
  const unsigned int tile = cimg_option("-tile",0,"Use tiled mode (reduce memory usage");
  const unsigned int btile = cimg_option("-btile",4,"Size of tile overlapping regions");

  // Load input image (replace 'unsigned char' by 'unsigned short' to be able to process 16-bits/pixels).
  CImg<unsigned char> img(file_i);

  // Create display window
  CImgDisplay disp(img,"GREYCstoration");

  // Begin iteration loop
  //---------------------
  for (unsigned int iter=0; iter<nb_iter; iter++) {

    // This function will start a thread running one iteration of the GREYCstoration filter.
    // It returns immediately, so you can do what you want after (update a progress bar for instance).
    img.greycstoration_run(amplitude,sharpness,anisotropy,alpha,sigma,dl,da,gauss_prec,interp,fast_approx,tile,btile);

    // Here, we print the overall progress percentage.
    do {
      // pr_iteration is the progress percentage for the current iteration
      const float pr_iteration = img.greycstoration_progress();

      // This simply computes the global progression indice (including all iterations)
      const unsigned int pr_global = (unsigned int)((iter*100 + pr_iteration)/nb_iter);

      // Display progress on window title and console.
      std::fprintf(stderr,"\rProgress : %u%%\t",pr_global);
      disp.set_title("GREYCstoration (%u%%)",pr_global);

      // Wait a little bit
      cimg::wait(100);

      // If the display window is closed, stop the algorithm
      if (disp.is_closed) img.greycstoration_stop();

    } while (img.greycstoration_is_running());

    img.display(disp);
  }
  std::fprintf(stderr,"\rDone !                         \n\n");

  disp.close();
  img.display("GREYCstoration - Result");

  return 0;
}
