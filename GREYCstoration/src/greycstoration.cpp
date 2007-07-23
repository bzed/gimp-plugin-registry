/*-----------------------------------------------------------------------------

    File        : greycstoration.cpp

    Description : GREYCstoration - A tool to denoise, inpaint and resize images.

    The GREYCstoration algorithm is an implementation of diffusion tensor-directed
    diffusion PDE's for image regularization and interpolation, published in

    "Fast Anisotropic Smoothing of Multi-Valued Images
    using Curvature-Preserving PDE's"
    (D. Tschumperlé)
    International Journal of Computer Vision, May 2006.
    (see also http://www.greyc.ensicaen.fr/~dtschump/greycstoration)

    "Vector-Valued Image Regularization with PDE's : A Common Framework
    for Different Applications"
    (D. Tschumperlé, R. Deriche).
    IEEE Transactions on Pattern Analysis and Machine Intelligence,
    Vol 27, No 4, pp 506-517, April 2005.

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

#define greycstoration_version 2.52
#if defined(sun)         || defined(__sun)      || defined(linux)       || defined(__linux) \
 || defined(__linux__)   || defined(__CYGWIN__) || defined(BSD)         || defined(__FreeBSD__) \
 || defined(__OPENBSD__) || defined(__MACOSX__) || defined(__APPLE__)   || defined(sgi) \
 || defined(__sgi)
#include <pthread.h>
#endif
#define cimg_debug 1
#define cimg_plugin "plugins/greycstoration.h"
#include "../CImg.h"
using namespace cimg_library;
// The undef below is necessary when using a non-standard compiler.
#ifdef cimg_use_visualcpp6
#define std
#endif

//-----------
// get_geom() : read geometry from a string (for instance '320x256' or '200%x200%').
//-----------
void get_geom(const char *geom, int &geom_w, int &geom_h) {
  char tmp[16];
  std::sscanf(geom,"%d%7[^0-9]%d%7[^0-9]",&geom_w,tmp,&geom_h,tmp+1);
  if (tmp[0]=='%') geom_w=-geom_w;
  if (tmp[1]=='%') geom_h=-geom_h;
}

//--------------------------
// GREYCstoration main code
//--------------------------
template<typename T> void greycstoration(int argc, char **argv, T pixel_type) {
  pixel_type = 0;


  cimg_usage("Fast Anisotropic Smoothing of Multi-valued Images with Curvature-Preserving PDE's");
  if (!cimg_option("-quiet",false,0))
    std::fprintf(stderr,
                 "---------------------------------------------------------------------------------------------\n"
                 " GREYCstoration v%g :\n"
                 " ------------------------------\n"
                 " The GREYCstoration algorithm is the result of a research work done in the\n"
                 " IMAGE group of the GREYC Lab (CNRS, UMR 6072)/France, by David Tschumperle\n"
                 " ( http://www.greyc.ensicaen.fr/EquipeImage/ and http://www.greyc.ensicaen.fr/~dtschump/ ).\n"
                 " It can be used to denoise, inpaint or resize 2D color images.\n"
                 " This algorithm is intended to help people for processing image data and for comparison\n"
                 " purposes. This is an open source software, distributed within the CImg Library package\n"
                 " (http://cimg.sourceforge.net), and submitted to the CeCiLL License (view file LICENSE.txt).\n"
                 " If you are interested to distribute this algorithm in a closed-source product,\n"
                 " you are invited to contact David Tschumperle (mail available on his web page).\n"
                 " Please also cite the paper below when using results of the GREYCstoration\n"
                 " algorithm in your own publications :\n\n"
                 "    \"Fast Anisotropic Smoothing of Multi-Valued Images using Curvature-Preserving PDE's.\"\n"
                 "    ( D. Tschumperle )\n"
                 "    International Journal of Computer Vision, IJCV(68), No. 1, June 2006, pp. 65-82.\n"
                 " You can also take a look at the official GREYCstoration web page :\n"
                 "    http://www.greyc.ensicaen.fr/~dtschump/greycstoration/\n"
                 "---------------------------------------------------------------------------------------------\n\n",
                 greycstoration_version);
  else std::fprintf(stderr,"** GREYCstoration v%g\n",greycstoration_version);

  // Read command line parameters
  //------------------------------
  cimg_help("  + Input/Output parameters\n    -----------------------");
  const char *restore      = cimg_option("-restore",  (const char*)0,"Restore the image specified after '-restore'");
  const char *inpaint      = cimg_option("-inpaint",  (const char*)0,"Inpaint the image specified after '-inpaint'");
  const char *resize       = cimg_option("-resize",   (const char*)0,"Resize the image specified after '-resize'");
  const char *refer        = cimg_option("-reference",(const char*)0,"Reference image to compare with");
  unsigned char bits       = cimg_option("-bits",                  8,"Define image pixel type (8='uchar', 16='ushort', 32='float')");
  bits = 0;
  const unsigned int save  = cimg_option("-save",                  0,"Iteration saving step (>=0)");
  const bool visu          = cimg_option("-visu",cimg_display_type?true:false,"Enable/Disable visualization windows (0 or 1)");
  const char *file_o       = cimg_option("-o",        (const char*)0,"Output image");
  const bool append_result = cimg_option("-append",            false,"Append original image to output file");
  unsigned int jpg_quality = 0;
  const char *ext = cimg::filename_split(file_o);
  if (!cimg::strcasecmp(ext,"jpg") || !cimg::strcasecmp(ext,"jpeg"))
    jpg_quality = cimg_option("-quality",100,"JPEG output quality");

  cimg_help("\n  + Global algorithm parameters\n    ---------------------------");
  const float amplitude      = cimg_option("-dt",restore?40.0f:(inpaint?20.0f:20.0f),"Regularization strength per iteration (>=0)");
  unsigned int nb_iter       = cimg_option("-iter",       restore?1:(inpaint?1000:3),"Number of iterations (>0)");
  const float sharpness      = cimg_option("-p",    restore?0.9f:(inpaint?0.3f:0.2f),"Contour preservation (>=0)");
  const float anisotropy     = cimg_option("-a",   restore?0.15f:(inpaint?1.0f:0.9f),"Smoothing anisotropy (0<=a<=1)");
  const float alpha          = cimg_option("-alpha",restore?0.6f:(inpaint?0.8f:0.1f),"Noise scale (>=0)");
  const float sigma          = cimg_option("-sigma",restore?1.1f:(inpaint?2.0f:1.5f),"Geometry regularity (>=0)");
  const bool fast_approx     = cimg_option("-fast",                             true,"Enable fast approximation (true or false)");
  const float gauss_prec     = cimg_option("-prec",                             2.0f,"Computation precision (>0)");
  const float dl             = cimg_option("-dl",                               0.8f,"Spatial integration step (0<=dl<=1)");
  const float da             = cimg_option("-da",                              30.0f,"Angular integration step (0<=da<=90)");
  const unsigned int interp  = cimg_option("-interp",                              0,"Interpolation type (0=Nearest-neighbor, 1=Linear, 2=Runge-Kutta)");
  const unsigned int tile    = cimg_option("-tile",                              512,"Activate tiled mode and set tile size (>=0)");
  const unsigned int btile   = cimg_option("-btile",                               4,"Size of tile borders (>=0)");
  const unsigned int threads = cimg_option("-threads",                             2,"Number of threads used (tile mode only)");
  const float sdt            = cimg_option("-sdt", restore?0.0f:(inpaint?0.0f:10.0f),"Activate sharpening filter, set sharpening strength (>=0)");
  const float sp             = cimg_option("-sp",   restore?0.5f:(inpaint?0.5f:3.0f),"Sharpening edge threshold (>=0)");

  if (argc==1 ||
      (!restore && !inpaint && !resize) ||
      (restore && inpaint) ||
      (restore && resize) ||
      (inpaint && resize)) {
    cimg::dialog(cimg::basename(argv[0]),
		 "You must specify (only) one of the '-restore', '-inpaint', or '-resize' options.\n"
		 "(try option '-h', '-h -restore','-h -inpaint' or '-h -resize' to get options relative to specific actions\n");
    std::exit(0);
  }

  // Init variables
  //----------------
  CImg<T> img0, img, imgr;
  CImg<unsigned char> mask;
  CImgDisplay *disp = 0;

  // Specific parameters for image denoising
  //----------------------------------------
  if (restore) {
    cimg_help("\n  + Restoration mode parameters\n    ---------------------------");
    const float ng = cimg_option("-ng",0.0f,"Add synthetic gaussian noise before denoising (>=0)");
    const float nu = cimg_option("-nu",0.0f,"Add synthetic uniform noise before denoising (>=0)");
    const float ns = cimg_option("-ns",0.0f,"Add synthetic Salt&Pepper noise before denoising (>=0)");

    std::fprintf(stderr,"- Image Restoration mode\n");
    if (!cimg::strcmp("-restore",restore)) {
      cimg::dialog(cimg::basename(argv[0]),"You must specify a valid input image filename after the '-restore' flag.");
      std::exit(0);
    }
    std::fprintf(stderr,"- Load input image '%s'...",cimg::basename(restore));
    img.load(restore);
    const CImgStats stats(img,false);
    std::fprintf(stderr,"\r- Input image : '%s' (size %ux%u, value range [%g,%g])\n",
		 cimg::basename(restore),img.width,img.height,stats.min,stats.max);
    if (ng>0 || nu>0 || ns>0) {
      img0 = img;
      if (ng>0) img.noise(ng,0);
      if (nu>0) img.noise(nu,1);
      if (ns>0) img.noise(ns,2);
      const CImgStats stats(img,false);
      std::fprintf(stderr,"\r- Noisy image : value range [%g,%g], PSNR Noisy / Original : %g\n",
		   stats.min,stats.max,img.PSNR(img0));
    }
  }

  // Specific parameters for image inpainting
  //-----------------------------------------
  if (inpaint) {
    cimg_help("\n  + Inpainting mode parameters\n    --------------------------");
    const char *file_m        = cimg_option("-m",(const char*)0,"Input inpainting mask");
    const unsigned int dilate = cimg_option("-dilate",0,"Inpainting mask dilatation (>=0)");
    const unsigned int init   = cimg_option("-init",4,"Inpainting init (0=black, 1=white, 2=noise, 3=unchanged, 4=smart)");

    std::fprintf(stderr,"- Image Inpainting mode\n");
    if (!cimg::strcmp("-inpaint",inpaint)) {
      cimg::dialog(cimg::basename(argv[0]),"You must specify a valid input image filename after the '-inpaint' flag.");
      std::exit(0);
    }
    std::fprintf(stderr,"- Load input image '%s'...",cimg::basename(inpaint));
    img.load(inpaint);
    const CImgStats stats(img,false);
    std::fprintf(stderr,"\r- Input image : '%s' (size %ux%u, value range [%g,%g])\n",
		 cimg::basename(inpaint),img.width,img.height,stats.min,stats.max);
    if (!file_m) {
      cimg::dialog(cimg::basename(argv[0]),"You need to specify a valid inpainting mask filename after the '-m' flag.");
      std::exit(0);
    }
    if (cimg::strncasecmp("block",file_m,5)) {
      std::fprintf(stderr,"- Load inpainting mask '%s'...",cimg::basename(file_m));
      mask.load(file_m);
      std::fprintf(stderr,"\r- Inpainting mask : '%s' (size %ux%u)\n",cimg::basename(file_m),mask.width,mask.height);
    }
    else {
      int l=16; std::sscanf(file_m,"block%d",&l);
      mask.assign(img.dimx()/l,img.dimy()/l);
      cimg_forXY(mask,x,y) mask(x,y)=(x+y)%2;
      img0 = img;
    }
    mask.resize(img.dimx(),img.dimy(),1,1);
    if (dilate) mask.dilate(dilate);
    switch (init) {
    case 0 : { cimg_forXYV(img,x,y,k) if (mask(x,y)) img(x,y,k) = 0; } break;
    case 1 : { cimg_forXYV(img,x,y,k) if (mask(x,y)) img(x,y,k) = 255; } break;
    case 2 : { cimg_forXYV(img,x,y,k) if (mask(x,y)) img(x,y,k) = (T)(255*cimg::rand()); } break;
    case 3 : break;
    default: {
      typedef unsigned char uchar;
      CImg<unsigned char> tmask(mask), ntmask(tmask);
      CImg_3x3(M,uchar);
      CImg_3x3(I,T);
      while (CImgStats(ntmask,false).max>0) {
	cimg_for3x3(tmask,x,y,0,0,M) if (Mcc && (!Mpc || !Mnc || !Mcp || !Mcn)) {
	  const float
            ccp = Mcp?0.0f:1.0f, cpc = Mpc?0.0f:1.0f,
	    cnc = Mnc?0.0f:1.0f, ccn = Mcn?0.0f:1.0f,
            csum = ccp + cpc + cnc + ccn;
	  cimg_forV(img,k) {
	    cimg_get3x3(img,x,y,0,k,I);
	    img(x,y,k) = (T)((ccp*Icp + cpc*Ipc + cnc*Inc + ccn*Icn)/csum);
	  }
	  ntmask(x,y) = 0;
	}
	tmask = ntmask;
      }
    } break;
    }
  }

  // Specific parameters for image resizing
  //----------------------------------------
  if (resize) {
    cimg_help("\n  + Resizing mode parameters\n    ------------------------");
    const char *geom0       = cimg_option("-g0",(const char*)0,"Input image geometry");
    const char *geom        = cimg_option("-g",(const char*)0,"Output image geometry");
    const bool anchor       = cimg_option("-anchor",true,"Anchor original pixels");
    const unsigned int init = cimg_option("-init",5,"Initial estimate (1=block, 3=linear, 5=bicubic)");

    std::fprintf(stderr,"- Image Resizing mode\n");
    if (!geom && !geom0) {
      cimg::dialog(cimg::basename(argv[0]),"You need to specify an output geometry or an input geometry (option -g or -g0)");
      std::exit(0);
    }
    if (!cimg::strcmp("-resize",resize)) {
      cimg::dialog(cimg::basename(argv[0]),"You must specify a valid input image filename after the '-resize' flag.");
      std::exit(0);
    }
    std::fprintf(stderr,"- Load input image '%s'...",cimg::basename(resize));
    img.load(resize);
    const CImgStats stats(img,false);
    std::fprintf(stderr,"\r- Input image : '%s' (size %ux%u, value range [%g,%g])\n",
		 cimg::basename(resize),img.width,img.height,stats.min,stats.max);
    int w,h;
    if (geom0) {
      int w0,h0;
      get_geom(geom0,w0,h0);
      w0 = w0>0?w0:-w0*img.dimx()/100;
      h0 = h0>0?h0:-h0*img.dimy()/100;
      std::fprintf(stderr,"- Reducing geometry to %dx%d using %s interpolation.\n",w0,h0,
		   init==1?"bloc":(init==3?"linear":"bicubic"));
      img0.assign(img);
      w = img.dimx();
      h = img.dimy();
      img.resize(w0,h0,-100,-100,5);
    }
    if (geom) {
      get_geom(geom,w,h);
      w = w>0?w:-w*img.dimx()/100;
      h = h>0?h:-h*img.dimy()/100;
    }
    mask.assign(img.dimx(),img.dimy(),1,1,255);
    if (!anchor) mask.resize(w,h,1,1,1); else mask = !mask.resize(w,h,1,1,4);
    img.resize(w,h,1,-100,init);
    if (!img0.is_empty()) std::fprintf(stderr,"\r- PSNR Original / Thumbnail : %g\n",img.PSNR(img0));
  }

  // Load reference image if any mentioned
  //--------------------------------------
  if (refer) {
    std::fprintf(stderr,"- Load reference image '%s'...",cimg::basename(refer));
    imgr.load(refer);
    const CImgStats stats(imgr,false);
    std::fprintf(stderr,"\r- Reference image : '%s' (size %ux%u, value range [%g,%g])",
		 cimg::basename(refer),imgr.width,imgr.height,stats.min,stats.max);
    if (!img0.is_empty()) {
      imgr.resize(img0);
      std::fprintf(stderr,", PSNR Reference / Original : %g dB\n",imgr.PSNR(img0));
    } else {
      imgr.resize(img);
      std::fprintf(stderr,"\n");
    }
  }

  // Init images and display
  //-------------------------
  CImg<T> dest(img);
  if (!visu && !append_result) img.assign();
  if (visu) {
    const int sx = 2*CImgDisplay::screen_dimx()/3, sy = 2*CImgDisplay::screen_dimy()/3;
    int nwidth=dest.dimx(), nheight=dest.dimy();
    if (nwidth>sx) { nheight = nheight*sx/nwidth; nwidth = sx; }
    if (nheight>sy) { nwidth = nwidth*sy/nheight; nheight = sy; }
    disp = new CImgDisplay(img.get_resize(nwidth,nheight),"GREYCstoration");
    dest.display(*disp);
  }
  const float gfact = (sizeof(T)==2)?1.0f/256:1.0f;

  //---------------------------------
  // Begin GREYCstoration iterations
  //---------------------------------
  bool stop_all = false;
  for (unsigned int iter=0; iter<nb_iter && !stop_all; iter++) {
    bool stop_iteration = false;

    // Run one iteration of the GREYCstoration filter
    //------------------------------------------------
    if (restore)
      dest.greycstoration_run(amplitude,sharpness,anisotropy,alpha,sigma,gfact,dl,da,gauss_prec,interp,fast_approx,tile,btile,threads);
    if (inpaint || resize)
      dest.greycstoration_run(mask,amplitude,sharpness,anisotropy,alpha,sigma,gfact,dl,da,gauss_prec,interp,fast_approx,tile,btile,threads);
    do {
      const unsigned int progress = (unsigned int)dest.greycstoration_progress();
      std::fprintf(stderr,"\r- Processing : Iteration %u/%u (%u%%)\t\t",1+iter,nb_iter,progress);
      if (disp) {
        if (disp->is_resized) disp->resize();
        disp->set_title("Processing : Iteration %u/%u (%u%%)",1+iter,nb_iter,progress);
        if (disp->is_closed || disp->key==cimg::keyQ || disp->key==cimg::keyESC) {
          dest.greycstoration_stop();
          stop_iteration = true;
          iter = nb_iter-1;
        }
      }
      cimg::wait(200);
    } while (dest.greycstoration_is_running());
    if (!stop_iteration && sdt>0) dest.sharpen(sdt,sp,alpha/3,sigma/3);
    dest.cut(cimg::type<T>::min(),cimg::type<T>::max());

    // Prepare for next iteration
    //---------------------------
    if (disp && visu) dest.display(*disp);
    if (file_o && save && !(iter%save)) dest.save(file_o,iter);

    // Display result and allows user interaction if needed.
    //-------------------------------------------------------
    if (iter==nb_iter-1) {
      std::fprintf(stderr,"\r- Processing : Done !                          \n");
      if (!img0.is_empty()) std::fprintf(stderr,"- PSNR Restored / Original : %g dB\n",dest.PSNR(img0));
      if (disp) {
	static bool first_time = true;
	if (first_time) {
	  first_time = false;
	  std::fprintf(stderr,
		       "- GREYCstoration interface :\n"
		       " > You can now zoom to a particular rectangular region,\n"
                       "   or press one of the following key on the display window :\n"
		       "   SPACE : Swap views.\n"
		       "   S     : Save a snapshot of the current image.\n"
		       "   I     : Run another iteration.\n"
		       "   Q     : Quit GREYCstoration.\n");
	}

	CImgList<T> visu;
	visu.insert_shared(img0).insert_shared(img).insert_shared(dest).insert_shared(imgr);
	const char *titles[4] = { "original", "noisy", "restored", "reference"};
	unsigned int visupos = 2;
	CImgDisplay *dispz = 0;
	CImg<T> zoom;
	int s[6], snb=0;
	bool stop_interact = false;
	while (!stop_interact) {
          disp->show().set_title("GREYCstoration (%s)",titles[visupos]);
	  visu(visupos).feature_selection(s,2,*disp);
          if (disp->is_closed) stop_interact = true;
	  switch (disp->key) {
	  case cimg::keySPACE: do { visupos = (visupos+1)%visu.size; } while (visu(visupos).is_empty()); break;
	  case cimg::keyBACKSPACE: do { visupos = (visupos-1+visu.size)%visu.size; } while (visu(visupos).is_empty()); break;
	  case cimg::keyQ: stop_interact = stop_all = true; break;
	  case cimg::keyI:
	    stop_interact = true;
	    std::fprintf(stderr,"- Perform iteration %u...\n",++nb_iter);
	    dest.display(*disp);
	    break;
	  case cimg::keyS:
	    if (!snb) {
              if (!append_result) dest.save(file_o?file_o:"GREYCstoration.bmp");
              else CImgList<T>(img,dest).get_append('x').save(file_o?file_o:"GREYCstoration.bmp");
            }
	    if (!zoom.is_empty()) zoom.save(file_o?file_o:"GREYCstoration.bmp",snb);
	    std::fprintf(stderr,"- Snapshot %u : '%s' saved\n",snb++,file_o?file_o:"GREYCstoration.bmp");
	    break;
	  }
	  disp->key = 0;
	  if (disp->is_resized) disp->resize().display(visu(visupos));
	  if (dispz && dispz->is_resized) dispz->resize().display(zoom);
	  if (dispz && dispz->is_closed) { delete dispz; dispz = 0; }

	  if (s[0]>=0 && s[1]>=0 && s[3]>=0 && s[4]>=0) {
	    const int x0 = s[0], y0 = s[1], x1 = s[3], y1 = s[4];
	    if (cimg::abs(x0-x1)>4 && cimg::abs(y0-y1)>4) {
	      CImgList<T> tmp(img.get_crop(x0,y0,x1,y1), dest.get_crop(x0,y0,x1,y1));
	      if (!img0.is_empty()) tmp.insert(img0.get_crop(x0,y0,x1,y1),0);
	      if (!imgr.is_empty()) tmp.insert(imgr.get_crop(x0,y0,x1,y1));
	      zoom = tmp.get_append('x','c');
	      if (!dispz) {
		const int sx = 5*CImgDisplay::screen_dimx()/6, sy = 5*CImgDisplay::screen_dimy()/6;
		int nwidth = zoom.dimx(), nheight = zoom.dimy();
		if (nwidth>nheight) { nheight = nheight*sx/nwidth; nwidth = sx; }
		else { nwidth = nwidth*sy/nheight; nheight = sy; }
		dispz = new CImgDisplay(zoom.get_resize(nwidth,nheight));
		dispz->set_title("GREYCstoration (zoom) : - %s %s %s %s",
				 img0.is_empty()?"":"original -",
				 img.is_empty()?"":"noisy -",
				 dest.is_empty()?"":"restored -",
				 imgr.is_empty()?"":"reference -");
	      } else dispz->resize(dispz->dimx(),dispz->dimx()*zoom.dimy()/zoom.dimx(),false);
	      dispz->display(zoom).show();
	    }
	  }
	}
	if (dispz) delete dispz;
      }
    }
  }

  // Save result and exit
  //----------------------
  if (file_o) {
    if (jpg_quality) {
      std::fprintf(stderr,"\n- Saving output image '%s' (JPEG quality = %u%%)\n",file_o,jpg_quality);
      if (!append_result) dest.save_jpeg(file_o,jpg_quality);
      else CImgList<T>(img,dest).get_append('x').save_jpeg(file_o,jpg_quality);
    } else {
      std::fprintf(stderr,"\n- Saving output image '%s'\n",file_o);
      if (!append_result) dest.save(file_o);
      else CImgList<T>(img,dest).get_append('x').save(file_o);
    }
  }
  if (disp) delete disp;
  std::fprintf(stderr,"\n- Quit\n\n");
}


/*-----------------
  Main procedure
  ----------------*/
int main(int argc,char **argv) {
  const unsigned char bits = cimg_option("-bits",8,"Define image pixel type (8='uchar', 16='ushort', 32='float'");
  switch (bits) {
  case 32: {
    float pixel_type=0;
    greycstoration(argc,argv,pixel_type);
    } break;
  case 16: {
    unsigned short pixel_type=0;
    greycstoration(argc,argv,pixel_type);
    } break;
  default: {
    unsigned char pixel_type=0;
    greycstoration(argc,argv,pixel_type);
    } break;
  }
  return 0;
}
