/*
 #
 #  File        : greycstoration.cpp
 #                ( C++ source file )
 #
 #  Description : GREYCstoration - A tool to denoise, inpaint and resize images.
 #                This file is a part of the CImg Library project.
 #                ( http://cimg.sourceforge.net )
 #                See also the GREYCstoration web page
 #                ( http://www.greyc.ensicaen.fr/~dtschump/greycstoration )
 #
 #    The GREYCstoration algorithm is an implementation of tensor-directed and
 #    patch-based diffusion PDE's for image regularization and interpolation,
 #    published in
 #
 #    "Defining Some Variational Methods on the Space of Patches :
 #     Application to Multi-Valued Image Denoising and Registration"
 #    (D. Tschumperle, L. Brun)
 #    Rapport de recherche : Les cahiers du GREYC No 08-01, Mars 2008.
 #
 #    "Fast Anisotropic Smoothing of Multi-Valued Images
 #    using Curvature-Preserving PDE's"
 #    (D. Tschumperle)
 #    International Journal of Computer Vision, May 2006.
 #
 #    "Vector-Valued Image Regularization with PDE's : A Common Framework
 #    for Different Applications"
 #    (D. Tschumperle, R. Deriche).
 #    IEEE Transactions on Pattern Analysis and Machine Intelligence,
 #    Vol 27, No 4, pp 506-517, April 2005.
 #
 #  Copyright   : David Tschumperle
 #                ( http://www.greyc.ensicaen.fr/~dtschump/ )
 #
 #  License     : CeCILL v2.0
 #                ( http://www.cecill.info/licences/Licence_CeCILL_V2-en.html )
 #
 #  This software is governed by the CeCILL  license under French law and
 #  abiding by the rules of distribution of free software.  You can  use,
 #  modify and/ or redistribute the software under the terms of the CeCILL
 #  license as circulated by CEA, CNRS and INRIA at the following URL
 #  "http://www.cecill.info".
 #
 #  As a counterpart to the access to the source code and  rights to copy,
 #  modify and redistribute granted by the license, users are provided only
 #  with a limited warranty  and the software's author,  the holder of the
 #  economic rights,  and the successive licensors  have only  limited
 #  liability.
 #
 #  In this respect, the user's attention is drawn to the risks associated
 #  with loading,  using,  modifying and/or developing or reproducing the
 #  software by the user in light of its specific status of free software,
 #  that may mean  that it is complicated to manipulate,  and  that  also
 #  therefore means  that it is reserved for developers  and  experienced
 #  professionals having in-depth computer knowledge. Users are therefore
 #  encouraged to load and test the software's suitability as regards their
 #  requirements in conditions enabling the security of their systems and/or
 #  data to be ensured and,  more generally, to use and operate it in the
 #  same conditions as regards security.
 #
 #  The fact that you are presently reading this means that you have had
 #  knowledge of the CeCILL license and that you accept its terms.
 #
*/

#define cimg_plugin "plugins/greycstoration.h"
#ifndef cimg_debug
#define cimg_debug 1
#endif
#include "../CImg.h"
#if cimg_OS!=2
#include <pthread.h>
#endif
#define gprintf if (verbose) std::fprintf
using namespace cimg_library;

// The lines below are necessary when using a non-standard compiler as visualcpp6.
#ifdef cimg_use_visualcpp6
#define std
#endif
#ifdef min
#undef min
#undef max
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
  pixel_type = (T)0;
  cimg_usage(" Open Source Algorithms for Image Denoising and Interpolation.");
  cimg_help("-------------------------------------------------------------------------\n"
            " GREYCstoration v2.8, by David Tschumperle.                              \n"
            " ------------------------------------------------------------------------\n"
            " This program allows to denoise, inpaint and resize 2D color images.     \n"
            " It is the result of the research work done in the IMAGE group of the    \n"
            " GREYC Lab (CNRS, UMR 6072) (http://www.greyc.ensicaen.fr/EquipeImage/)  \n"
            " by David Tschumperle (http://www.greyc.ensicaen.fr/~dtschump/). This    \n"
            " program has been primarily released to help people processing image data\n"
            " and to allow comparisons between regularization algorithms. This is an  \n"
            " open source software, distributed within the CImg Library package       \n"
            " (http://cimg.sourceforge.net), and submitted to the CeCILL License. If  \n"
            " you are interested to distribute it in a closed-source product, please  \n"
            " read the licence file carefully. If you are using 'GREYCstored' images  \n"
            " in your own publications, be kind to reference the GREYCstoration web   \n"
            " site or the related scientific papers. More informations available at : \n"
            "    ** http://www.greyc.ensicaen.fr/~dtschump/greycstoration/ **         \n"
            "-------------------------------------------------------------------------\n");

  // Read Global parameters
  //------------------------
  cimg_help("  + Global parameters\n    -----------------------");
  const char        *restore_mode    = cimg_option("-restore",(char*)0,"Restore image specified after '-restore'");
  const char        *inpaint_mode    = cimg_option("-inpaint",(char*)0,"Inpaint image specified after '-inpaint'");
  const char        *resize_mode     = cimg_option("-resize",(char*)0,"Resize image specified after '-resize'");
  const char        *clean_mode      = cimg_option("-clean",(char*)0,"Clean image specified after '-clean'");
  const char        *reference_image = cimg_option("-ref",(char*)0,"Reference image to compare with");
                                       cimg_option("-bits",8,"Define input value type (8='uchar', 16='ushort', 32='float')");
  const unsigned int value_factor    = cimg_option("-fact",0,"Define input value factor (0=auto)");
  const float        noise_g         = cimg_option("-ng",0.0f,"Add Gaussian noise before denoising");
  const float        noise_u         = cimg_option("-nu",0.0f,"Add Uniform noise before denoising");
  const float        noise_s         = cimg_option("-ns",0.0f,"Add Salt&Pepper noise before denoising");
  const unsigned int color_base      = cimg_option("-cbase",0,"Processed color base (0=RGB, 1=YCbCr)");
  const char        *channel_range   = cimg_option("-crange",(char*)0,"Processed channels (ex: '0-2')");
  const unsigned int saving_step     = cimg_option("-save",0,"Iteration saving step (>=0)");
  const bool         visu            = cimg_option("-visu",cimg_display_type?true:false,"Enable/Disable visualization windows (0 or 1)");
  const char        *file_o          = cimg_option("-o",(char*)0,"Filename of output image");
  const bool         append_result   = cimg_option("-append",false,"Append images in output file");
  const bool         verbose         = cimg_option("-verbose",true,"Verbose mode");
  const unsigned int jpg_quality     = cimg_option("-quality",100,"Output compression quality (if JPEG format)");
  unsigned int       nb_iterations   = cimg_option("-iter",(restore_mode||clean_mode)?1:(inpaint_mode?1000:3),
                                                   "Number of iterations (>0)");
  const float        sdt             = cimg_option("-sharp",(restore_mode||clean_mode)?0.0f:(inpaint_mode?0.0f:10.0f),
                                                   "Sharpening strength (activate sharpening filter, >=0)");
  const float        sp              = cimg_option("-se",(restore_mode||clean_mode)?0.5f:(inpaint_mode?0.5f:3.0f),
                                                   "Sharpening edge threshold (>=0)");
  const unsigned int tile_size       = cimg_option("-tile",512,"Activate tiled mode and set tile size (>=0)");
  const unsigned int tile_border     = cimg_option("-btile",4,"Size of tile borders (>=0)");
  const unsigned int nb_threads      = cimg_option("-threads",1,"Number of threads used (*experimental*, tile mode only, >0)");
  const bool         fast_approx     = cimg_option("-fast",true,"Try faster algorithm (true or false)");

  // Declare specific algorithm parameters
  //--------------------------------------
  float amplitude = 0, sharpness = 0, anisotropy = 0, alpha = 0, sigma = 0, gauss_prec = 0, dl = 0, da = 0, sigma_s = 0, sigma_p = 0;
  unsigned int interpolation = 0, patch_size = 0, lookup_size = 0;

  if (argc==1 ||
      (!restore_mode && !inpaint_mode && !resize_mode && !clean_mode) ||
      (restore_mode && inpaint_mode) || (restore_mode && resize_mode) || (restore_mode && clean_mode) ||
      (inpaint_mode && resize_mode) || (inpaint_mode && clean_mode)) {
    std::fprintf(stderr,"\n%s : You must specify (only) one of the '-restore', '-inpaint', '-resize' or '-clean' options.\n"
                 "(try option '-h', '-h -restore','-h -inpaint', '-h -resize' or '-h -clean' to get options relative to specific actions\n\n",
                 cimg::basename(argv[0]));
    std::exit(0);
  }

  // Init variables
  //----------------
  CImg<T> img0, img, imgr;
  CImg<unsigned char> mask;
  CImgDisplay disp;

  // Read specific parameters for image restoration
  //------------------------------------------------
  if (restore_mode) {
    cimg_help("\n  + Restoration mode parameters\n    ---------------------------");
    amplitude      = cimg_option("-dt",40.0f,"Regularization strength per iteration (>=0)");
    sharpness      = cimg_option("-p",0.9f,"Contour preservation (>=0)");
    anisotropy     = cimg_option("-a",0.15f,"Smoothing anisotropy (0<=a<=1)");
    alpha          = cimg_option("-alpha",0.6f,"Noise scale (>=0)");
    sigma          = cimg_option("-sigma",1.1f,"Geometry regularity (>=0)");
    gauss_prec     = cimg_option("-prec",2.0f,"Computation precision (>0)");
    dl             = cimg_option("-dl",0.8f,"Spatial integration step (0<=dl<=1)");
    da             = cimg_option("-da",30.0f,"Angular integration step (0<=da<=90)");
    interpolation  = cimg_option("-interp",0,"Interpolation type (0=Nearest-neighbor, 1=Linear, 2=Runge-Kutta)");

    gprintf(stderr,"- Image Restoration mode\n");
    if (!cimg::strcmp("-restore",restore_mode)) {
      std::fprintf(stderr,"%s : You must specify a valid input image filename after the '-restore' flag.\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    gprintf(stderr,"- Load input image '%s'...",cimg::basename(restore_mode));
    img.load(restore_mode);
    gprintf(stderr,"\r- Input image : '%s' (size %dx%d, value range [%g,%g])\n",
            cimg::basename(restore_mode),img.dimx(),img.dimy(),(double)img.min(),(double)img.max());
    if (noise_g || noise_u || noise_s) {
      img0 = img;
      img.noise(noise_g,0).noise(noise_u,1).noise(noise_s,2);
      gprintf(stderr,"\r- Noisy image : value range [%g,%g], PSNR Noisy / Original : %g\n",(double)img.min(),(double)img.max(),img.PSNR(img0));
    }
  }

  // Specific parameters for image inpainting
  //-----------------------------------------
  if (inpaint_mode) {
    cimg_help("\n  + Inpainting mode parameters\n    --------------------------");
    const char *file_m        = cimg_option("-m",(char*)0,"Input inpainting mask");
    const unsigned int dilate = cimg_option("-dilate",0,"Inpainting mask dilatation (>=0)");
    const unsigned int init   = cimg_option("-init",4,"Inpainting init (0=black, 1=white, 2=noise, 3=unchanged, 4=smart)");
    amplitude                 = cimg_option("-dt",20.0f,"Regularization strength per iteration (>=0)");
    sharpness                 = cimg_option("-p",0.3f,"Contour preservation (>=0)");
    anisotropy                = cimg_option("-a",1.0f,"Smoothing anisotropy (0<=a<=1)");
    alpha                     = cimg_option("-alpha",0.8f,"Noise scale (>=0)");
    sigma                     = cimg_option("-sigma",2.0f,"Geometry regularity (>=0)");
    gauss_prec                = cimg_option("-prec",2.0f,"Computation precision (>0)");
    dl                        = cimg_option("-dl",0.8f,"Spatial integration step (0<=dl<=1)");
    da                        = cimg_option("-da",30.0f,"Angular integration step (0<=da<=90)");
    interpolation             = cimg_option("-interp",0,"Interpolation type (0=Nearest-neighbor, 1=Linear, 2=Runge-Kutta)");

    gprintf(stderr,"- Image Inpainting mode\n");
    if (!cimg::strcmp("-inpaint",inpaint_mode)) {
      std::fprintf(stderr,"%s : You must specify a valid input image filename after the '-inpaint' flag.\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    gprintf(stderr,"- Load input image '%s'...",cimg::basename(inpaint_mode));
    img.load(inpaint_mode);
    gprintf(stderr,"\r- Input image : '%s' (size %dx%d, value range [%g,%g])\n",
            cimg::basename(inpaint_mode),img.dimx(),img.dimy(),(double)img.min(),(double)img.max());
    if (noise_g || noise_u || noise_s) {
      img0 = img;
      img.noise(noise_g,0).noise(noise_u,1).noise(noise_s,2);
      gprintf(stderr,"\r- Noisy image : value range [%g,%g], PSNR Noisy / Original : %g\n",(double)img.min(),(double)img.max(),img.PSNR(img0));
    }
    if (!file_m) {
      std::fprintf(stderr,"%s : You need to specify a valid inpainting mask filename after the '-m' flag.\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    if (cimg::strncasecmp("block",file_m,5)) {
      gprintf(stderr,"- Load inpainting mask '%s'...",cimg::basename(file_m));
      mask.load(file_m);
      gprintf(stderr,"\r- Inpainting mask : '%s' (size %dx%d)\n",cimg::basename(file_m),mask.dimx(),mask.dimy());
    }
    else {
      unsigned int l = 16; std::sscanf(file_m,"block%u",&l);
      mask.assign(img.dimx()/l,img.dimy()/l);
      cimg_forXY(mask,x,y) mask(x,y) = (x+y)%2;
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
      CImg_3x3(M,uchar); Mpp = Mnp = Mpn = Mnn = 0;
      CImg_3x3(I,T); Ipp = Inp = Icc = Ipn = Inn = 0;
      while (ntmask.max()>0) {
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
  if (resize_mode) {
    cimg_help("\n  + Resizing mode parameters\n    ------------------------");
    const char *geom0         = cimg_option("-g0",(char*)0,"Input image geometry");
    const char *geom          = cimg_option("-g",(char*)0,"Output image geometry");
    const bool anchor         = cimg_option("-anchor",true,"Anchor original pixels (keep their original values)");
    const unsigned int init   = cimg_option("-init",3,"Initial estimate (1=block, 3=linear, 4=Moving average, 5=bicubic)");
    amplitude                 = cimg_option("-dt",20.0f,"Regularization strength per iteration (>=0)");
    sharpness                 = cimg_option("-p",0.2f,"Contour preservation (>=0)");
    anisotropy                = cimg_option("-a",0.9f,"Smoothing anisotropy (0<=a<=1)");
    alpha                     = cimg_option("-alpha",0.1f,"Noise scale (>=0)");
    sigma                     = cimg_option("-sigma",1.5f,"Geometry regularity (>=0)");
    gauss_prec                = cimg_option("-prec",2.0f,"Computation precision (>0)");
    dl                        = cimg_option("-dl",0.8f,"Spatial integration step (0<=dl<=1)");
    da                        = cimg_option("-da",30.0f,"Angular integration step (0<=da<=90)");
    interpolation             = cimg_option("-interp",0,"Interpolation type (0=Nearest-neighbor, 1=Linear, 2=Runge-Kutta)");

    gprintf(stderr,"- Image Resizing mode\n");
    if (!geom && !geom0) {
      std::fprintf(stderr,"%s : You need to specify an output geometry or an input geometry (option -g or -g0).\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    if (!cimg::strcmp("-resize",resize_mode)) {
      std::fprintf(stderr,"%s : You must specify a valid input image filename after the '-resize' flag.\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    gprintf(stderr,"- Load input image '%s'...",cimg::basename(resize_mode));
    img.load(resize_mode);
    gprintf(stderr,"\r- Input image : '%s' (size %dx%d, value range [%g,%g])\n",
            cimg::basename(resize_mode),img.dimx(),img.dimy(),(double)img.min(),(double)img.max());
    if (noise_g || noise_u || noise_s) {
      img0 = img;
      img.noise(noise_g,0).noise(noise_u,1).noise(noise_s,2);
      gprintf(stderr,"\r- Noisy image : value range [%g,%g], PSNR Noisy / Original : %g\n",(double)img.min(),(double)img.max(),img.PSNR(img0));
    }
    int w, h;
    if (geom0) {
      int w0, h0;
      get_geom(geom0,w0,h0);
      w0 = w0>0?w0:-w0*img.dimx()/100;
      h0 = h0>0?h0:-h0*img.dimy()/100;
      gprintf(stderr,"- Reducing geometry to %dx%d using %s interpolation.\n",w0,h0,init==1?"bloc":(init==3?"linear":(init==5?"bicubic":"moving average")));
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
    if (img0) { gprintf(stderr,"\r- PSNR Original / Thumbnail : %g\n",img.PSNR(img0)); }
  }

  // Specific parameters for image cleaning
  //----------------------------------------
  if (clean_mode) {
    cimg_help("\n  + Cleaning mode parameters\n    ------------------------");
    patch_size  = cimg_option("-p",4,"Patch size (>0)");
    sigma_s     = cimg_option("-ss",15.0f,"Spatial sigma (>0)");
    sigma_p     = cimg_option("-sp",10.0f,"Patch sigma (>0)");
    lookup_size = cimg_option("-r",7,"Size of the lookup region (>0)");

    gprintf(stderr,"- Image Cleaning mode\n");
    if (!cimg::strcmp("-clean",clean_mode)) {
      std::fprintf(stderr,"%s : You must specify a valid input image filename after the '-clean' flag.\n\n",cimg::basename(argv[0]));
      std::exit(0);
    }
    gprintf(stderr,"- Load input image '%s'...",cimg::basename(clean_mode));
    img.load(clean_mode);
    gprintf(stderr,"\r- Input image : '%s' (size %dx%d, value range [%g,%g])\n",
            cimg::basename(clean_mode),img.dimx(),img.dimy(),(double)img.min(),(double)img.max());
    if (noise_g || noise_u || noise_s) {
      img0 = img;
      img.noise(noise_g,0).noise(noise_u,1).noise(noise_s,2);
      gprintf(stderr,"\r- Noisy image : value range [%g,%g], PSNR Noisy / Original : %g\n",(double)img.min(),(double)img.max(),img.PSNR(img0));
    }
  }

  // Load reference image if any specified
  //--------------------------------------
  if (reference_image) {
    gprintf(stderr,"- Load reference image '%s'...",cimg::basename(reference_image));
    imgr.load(reference_image);
    gprintf(stderr,"\r- Reference image : '%s' (size %dx%d, value range [%g,%g])",
            cimg::basename(reference_image),imgr.dimx(),imgr.dimy(),(double)imgr.min(),(double)imgr.max());
    if (img0) { imgr.resize(img0); gprintf(stderr,", PSNR Reference / Original : %g dB\n",imgr.PSNR(img0)); }
    else { imgr.resize(img); gprintf(stderr,"\n"); }
  }

  // Init images and display
  //-------------------------
  CImg<T> dest(img);
  unsigned int crange_beg = 0, crange_end = dest.dimv()-1U;
  if (color_base) dest.RGBtoYCbCr();
  if (channel_range) std::sscanf(channel_range,"%u%*c%u",&crange_beg,&crange_end);
  gprintf(stderr,"- Color base : %s, Channels range : %u-%u\n",color_base?"YCbCr":"RGB",crange_beg,crange_end);
  if (!visu && !append_result) img.assign();
  if (visu) {
    const int sx = 2*CImgDisplay::screen_dimx()/3, sy = 2*CImgDisplay::screen_dimy()/3;
    int nwidth = dest.dimx(), nheight = dest.dimy();
    if (nwidth>sx) { nheight = nheight*sx/nwidth; nwidth = sx; }
    if (nheight>sy) { nwidth = nwidth*sy/nheight; nheight = sy; }
    disp.assign(nwidth,nheight,"GREYCstoration");
    if (color_base) dest.get_YCbCrtoRGB().display(disp);
    else dest.display(disp);
  }
  const float gfact = (value_factor>0)?value_factor:((sizeof(T)==2)?1.0f/256:1.0f);

  //---------------------------------
  // Begin GREYCstoration iterations
  //---------------------------------
  bool stop_all = false;
  for (unsigned int iter=0; iter<nb_iterations && !stop_all; iter++) {
    bool stop_iteration = false;

    // Run one iteration of the GREYCstoration filter
    //------------------------------------------------
    CImg<T> dest_range = dest.get_shared_channels(crange_beg,crange_end);
    if (restore_mode)
      dest_range.greycstoration_run(amplitude,sharpness,anisotropy,alpha,sigma,gfact,dl,da,gauss_prec,
                                    interpolation,fast_approx,tile_size,tile_border,nb_threads);
    if (clean_mode)
      dest_range.greycstoration_patch_run(patch_size,sigma_s,sigma_p,lookup_size,
                                          tile_size,tile_border,nb_threads,fast_approx);
    if (inpaint_mode || resize_mode)
      dest_range.greycstoration_run(mask,amplitude,sharpness,anisotropy,alpha,sigma,gfact,dl,da,gauss_prec,
                                    interpolation,fast_approx,tile_size,tile_border,nb_threads);
    do {
      const unsigned int progress = (unsigned int)dest_range.greycstoration_progress();
      gprintf(stderr,"\r- Processing : Iteration %u/%u (%u%%)\t\t",1+iter,nb_iterations,progress);
      if (disp) {
        if (disp.is_resized) disp.resize();
        disp.set_title("Processing : Iteration %u/%u (%u%%)",1+iter,nb_iterations,progress);
        if (disp.is_closed || disp.key==cimg::keyQ || disp.key==cimg::keyESC) {
          dest_range.greycstoration_stop();
          stop_iteration = true;
          iter = nb_iterations-1;
        }
      }
      cimg::wait(200);
    } while (dest_range.greycstoration_is_running());
    if (!stop_iteration && sdt>0) dest_range.sharpen(sdt,sp,alpha/3,sigma/3);
    dest_range.cut(cimg::type<T>::min(),cimg::type<T>::max());

    // Prepare for next iteration
    //---------------------------
    CImg<T> tmp_rgb = color_base?dest.get_YCbCrtoRGB():CImg<T>(), &dest_rgb = color_base?tmp_rgb:dest;
    if (disp && visu) dest_rgb.display(disp);
    if (file_o && saving_step && !(iter%saving_step)) dest_rgb.save(file_o,iter);

    // Display result and allows user interaction if needed.
    //-------------------------------------------------------
    if (iter==nb_iterations-1) {
      gprintf(stderr,"\r- Processing : Done !                          \n");
      if (img0) { gprintf(stderr,"- PSNR Restored / Original : %g dB\n",dest_rgb.PSNR(img0)); }
      if (disp) {
        static bool first_time = true;
        if (first_time) {
          first_time = false;
          gprintf(stderr,
                  "- GREYCstoration interface :\n"
                  " > You can now zoom to a particular rectangular region,\n"
                  "   or press one of the following key on the display window :\n"
                  "   SPACE : Swap views.\n"
                  "   S     : Save a snapshot of the current image.\n"
                  "   I     : Run another iteration.\n"
                  "   Q     : Quit GREYCstoration.\n");
        }

        CImgList<T> visu;
        visu.insert(img0,~0,true).insert(img,~0,true).insert(dest_rgb,~0,true).insert(imgr,~0U,true);
        const char *titles[4] = { "original", "noisy", "restored", "reference"};
        unsigned int visupos = 2;
        CImgDisplay dispz;
        CImg<T> zoom;
        int snb = 0;
        bool stop_interact = false;
        while (!stop_interact) {
          disp.show().set_title("GREYCstoration (%s)",titles[visupos]);
          const CImg<int> s = visu(visupos).get_coordinates(2,disp);
          if (disp.is_closed) stop_interact = true;
          switch (disp.key) {
          case cimg::keySPACE: do { visupos = (visupos+1)%visu.size; } while (!visu(visupos)); break;
          case cimg::keyBACKSPACE: do { visupos = (visupos-1+visu.size)%visu.size; } while (!visu(visupos)); break;
          case cimg::keyQ: stop_interact = stop_all = true; break;
          case cimg::keyI:
            stop_interact = true;
            gprintf(stderr,"- Perform iteration %u...\n",++nb_iterations);
            dest_rgb.display(disp);
            break;
          case cimg::keyS:
            if (!snb) {
              if (!append_result) dest_rgb.save(file_o?file_o:"GREYCstoration.bmp");
              else CImgList<T>(img,dest_rgb).get_append('x').save(file_o?file_o:"GREYCstoration.bmp");
            }
            if (zoom) zoom.save(file_o?file_o:"GREYCstoration.bmp",snb);
            gprintf(stderr,"- Snapshot %u : '%s' saved\n",snb++,file_o?file_o:"GREYCstoration.bmp");
            break;
          }
          disp.key = 0;
          if (disp.is_resized) disp.resize().display(visu(visupos));
          if (dispz && dispz.is_resized) dispz.resize().display(zoom);
          if (dispz && dispz.is_closed) dispz.assign();

          if (s[0]>=0 && s[1]>=0 && s[3]>=0 && s[4]>=0) {
            const int x0 = s[0], y0 = s[1], x1 = s[3], y1 = s[4];
            if (cimg::abs(x0-x1)>4 && cimg::abs(y0-y1)>4) {
              CImgList<T> tmp(img.get_crop(x0,y0,x1,y1), dest_rgb.get_crop(x0,y0,x1,y1));
              if (img0) tmp.insert(img0.get_crop(x0,y0,x1,y1),0);
              if (imgr) tmp.insert(imgr.get_crop(x0,y0,x1,y1));
              zoom = tmp.get_append('x','c');
              if (!dispz) {
                const int sx = 5*CImgDisplay::screen_dimx()/6, sy = 5*CImgDisplay::screen_dimy()/6;
                int nwidth = zoom.dimx(), nheight = zoom.dimy();
                if (nwidth>nheight) { nheight = nheight*sx/nwidth; nwidth = sx; }
                else { nwidth = nwidth*sy/nheight; nheight = sy; }
                dispz.assign(zoom.get_resize(nwidth,nheight));
                dispz.set_title("GREYCstoration (zoom) : - %s %s %s %s",
                                img0?"original -":"",
                                img?"noisy -":"",
                                dest?"restored -":"",
                                imgr?"reference -":"");
              } else dispz.resize(dispz.dimx(),dispz.dimx()*zoom.dimy()/zoom.dimx(),false);
              dispz.display(zoom).show();
            }
          }
        }
      }
    }
  }

  // Save result and exit
  //----------------------
  if (file_o) {
    CImg<T> tmp_rgb = color_base?dest.get_YCbCrtoRGB():CImg<T>(), &dest_rgb = color_base?tmp_rgb:dest;
    if (jpg_quality) {
      gprintf(stderr,"\n- Saving output image '%s' (JPEG quality = %u%%)\n",file_o,jpg_quality);
      if (!append_result) dest_rgb.save_jpeg(file_o,jpg_quality);
      else CImgList<T>(img,dest_rgb).get_append('x').save_jpeg(file_o,jpg_quality);
    } else {
      gprintf(stderr,"\n- Saving output image '%s'\n",file_o);
      if (!append_result) dest_rgb.save(file_o);
      else CImgList<T>(img,dest_rgb).get_append('x').save(file_o);
    }
  }
  gprintf(stderr,"\n- Quit\n\n");
}


/*-----------------
  Main procedure
  ----------------*/
int main(int argc,char **argv) {
  switch (cimg_option("-bits",8,0)) {
  case 32: {
    float pixel_type = 0;
    greycstoration(argc,argv,pixel_type);
  } break;
  case 16: {
    unsigned short pixel_type = 0;
    greycstoration(argc,argv,pixel_type);
  } break;
  default: {
    unsigned char pixel_type = 0;
    greycstoration(argc,argv,pixel_type);
  } break;
  }
  return 0;
}
