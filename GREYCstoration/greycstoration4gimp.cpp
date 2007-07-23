/*-----------------------------------------------------------------------------

    File        : greycstoration4gimp.cpp

    Description : GREYCstoration - A tool to denoise, inpaint and resize images.
                  (GIMP>=2.2 plug-in version)

    The GREYCstoration algorithm is an implementation of diffusion tensor-directed
    diffusion PDE's for image regularization and interpolation, published in

    "Fast Anisotropic Smoothing of Multi-Valued Images
    using Curvature-Preserving PDE's"
    (D. Tschumperle)
    International Journal of Computer Vision, May 2006.
    (see also http://www.greyc.ensicaen.fr/~dtschump/greycstoration)

    "Vector-Valued Image Regularization with PDE's : A Common Framework
    for Different Applications"
    (D. Tschumperle, R. Deriche).
    IEEE Transactions on Pattern Analysis and Machine Intelligence,
    Vol 27, No 4, pp 506-517, April 2005.

    Copyright  :

    Grzegorz Szwoch (GIMP plugin code)
    David Tschumperle (GREYCstoration API) - http://www.greyc.ensicaen.fr/~dtschump/

    Plug-in version: 1.1
    Version history:
    1.1 (2007.03.31)
        - Added support for GimpZoomPreview (optional)
        - Make plug-in work for 1 bpp images
        - Added button to reset parameters to the initial state
    1.0 (2007.03.09)
        - Initial release

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

/* HOW TO COMPILE :
 *-----------------
 * g++ -o greycstoration4gimp greycstoration4gimp.cpp `gimptool-2.0 --cflags` `gimptool-2.0 --libs` -lpthread -O3
 * Then, copy the file 'greycstoration4gimp' into your GIMP plug-in directory.
 */

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/* Include the CImg Library, with the GREYCstoration plugin included */
#define cimg_plugin "plugins/greycstoration.h"
#define cimg_display_type 0
#include "CImg.h"
using namespace cimg_library;
#if cimg_OS!=2
#include <pthread.h>
#endif

/* Uncomment the line below if you want to use preview with zoom (GIMP >= 2.3.4) */
/*#define ZOOMPREVIEW*/

/* Uncomment this line to get a rough estimate of how long the plug-in takes to run. */
/*  #define TIMER */

/*=== PARAMETERS ============================================================ */
typedef struct {
  gdouble  amplitude;   /* Regularization amplitude */
  gdouble  sharpness;   /* Contour preservation for regularization (sharpness) */
  gdouble  anisotropy;  /* Regularization anisotropy */
  gdouble  alpha;       /* Noise scale */
  gdouble  sigma;       /* Geometry regularity */
  gdouble  dl;          /* Spatial integration step for regularization */
  gdouble  da;          /* Angular integration step for regulatization */
  gdouble  gauss_prec;  /* Precision of the gaussian function for regularization */
  gint     interp;      /* Interpolation type */
  bool     fast_approx; /* Use fast approximation for regularization */
  gint     iterations;  /* Number of regularization iterations */
  gboolean update_preview;
} Params;

/* Default values of parameters */
static Params defparams = {
  60.0, /* amplitude */
  0.7,  /* sharpness */
  0.3,  /* anisotropy */
  0.6,  /* alpha */
  1.1,  /* sigma */
  0.8,  /* dl */
  30.0, /* da */
  2.0,  /* gauss_prec */
  0,    /* interp */
  true, /* fast_approx */
  1,    /* iterations */
  TRUE  /* default is to update the preview */
};
/* Number of params + 2 */
gint numparams = 14;

/* Parameters configurable only here (not in GUI) */
#define TILESIZE 256  /* tile size (tile, {0, 256, 512, 1024, 2048) */
#define TILEBORDER  4  /* tile border (btile, 0-16) */
#define NTHREADS 16   /* number of threads (threads, 1-16) */

/*=== DECLARATIONS ===========================================================*/

typedef struct {
  gboolean  run;
} Interface;

/* local function prototypes */
static void query(void);
static void run(const gchar*name,
                gint nparams,
                const GimpParam *param,
                gint *nreturn_vals,
                GimpParam **return_vals);

static void process(GimpPixelRgn *srcPTR,
                    GimpPixelRgn *dstPTR,
                    gint bytes,
                    gint x1,
                    gint x2,
                    gint y1,
                    gint y2,
                    gboolean show_progress);

static void
callback_response (GtkWidget *widget,
             gint       response_id,
             gpointer   data);

static void rungreycstor(CImg<unsigned char>* img, gboolean show_progress);
static gboolean dialog(GimpDrawable *drawable);
static void preview_update(GimpPreview *preview);

static gboolean runflag = FALSE;
static Params params = defparams;

GtkWidget *preview;
GtkObject* adj_amplitude;
GtkObject* adj_sharpness;
GtkObject* adj_anisotropy;
GtkObject* adj_alpha;
GtkObject* adj_sigma;
GtkObject* adj_dl;
GtkObject* adj_da;
GtkObject* adj_iterations;
GtkWidget* combo_interp;
GtkWidget* button_fast_approx;

CImg<unsigned char> img;

/*=== GIMP STUFF =============================================================*/

/* Setting PLUG_IN_INFO */
GimpPlugInInfo PLUG_IN_INFO = {
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()
  static void query(void) {
  static GimpParamDef args[] = {
    {GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE,    "image", "(unused)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Drawable to draw on"},
    {GIMP_PDB_FLOAT,    "amplitude", "Regularization strength for one iteration"},
    {GIMP_PDB_FLOAT,    "sharpness", "Contour preservation for regularization"},
    {GIMP_PDB_FLOAT,    "anisotropy", "Regularization anisotropy"},
    {GIMP_PDB_FLOAT,    "alpha", "Noise scale"},
    {GIMP_PDB_FLOAT,    "sigma", "Geometry regularity"} ,
    {GIMP_PDB_FLOAT,    "dl", "Spatial integration step for regularization"} ,
    {GIMP_PDB_FLOAT,    "da", "Angular integration step for regulatization"},
    {GIMP_PDB_FLOAT,    "gauss_prec", "Precision of the gaussian function for regularization"},
    {GIMP_PDB_INT8,     "interp", "Interpolation type"},
    {GIMP_PDB_INT32,    "fast_approx", "Use fast approximation for regularization"},
    {GIMP_PDB_INT32,    "iterations", "Iterations accuracy"}
  };

  gimp_install_procedure ("plug_in_greycstoration",
                          "plug-in-greycstore",
                          "GREYCstoration",
                          "GREYCstoration de-noising plugin",
                          "Grzegorz Szwoch & David Tschumperle",
                          "Copyright Grzegorz Szwoch & David Tschumperle",
                          "_GREYCstoration...",
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  /* Register plugin */
  gimp_plugin_menu_register ("plug_in_greycstoration", "<Image>/Filters/Enhance");
}


/*=== RUN PROCEDURE ==========================================================*/
static void run (const gchar *name,
                 gint nparams,
                 const GimpParam *param,
                 gint *nreturn_vals,
                 GimpParam **return_vals) {

  static GimpParam   values[1];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpDrawable      *drawable;
  GimpRunMode        run_mode;
#ifdef TIMER
  GTimer            *timer = g_timer_new ();
#endif

  run_mode = (GimpRunMode) param[0].data.d_int32;

  *return_vals  = values;
  *nreturn_vals = 1;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /*INIT_I18N (); */

  /* Get drawable information */
  drawable = gimp_drawable_get(param[2].data.d_drawable);

  /* Make tile cache */
  gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

  switch (run_mode) {

  case GIMP_RUN_INTERACTIVE:
    gimp_get_data ("plug_in_greycstoration", &params);
    /* Reset default values show preview unmodified */
    /* initialize pixel regions and buffer */
    if (! dialog (drawable)) return;
    break;

  case GIMP_RUN_NONINTERACTIVE:
    if (nparams != numparams) {
      status = GIMP_PDB_CALLING_ERROR;
    } else {
      params.amplitude = param[3].data.d_float;
      params.sharpness = param[4].data.d_float;
      params.anisotropy = param[5].data.d_float;
      params.alpha = param[6].data.d_float;
      params.sigma = param[7].data.d_float;
      params.dl = param[8].data.d_float;
      params.da = param[9].data.d_float;
      params.gauss_prec = param[10].data.d_float;
      params.interp = param[11].data.d_int32;
      params.fast_approx = param[12].data.d_int32;
      params.iterations = param[13].data.d_int32;

      /* make sure there are legal values */
      if((params.amplitude < 0.0) ||
         (params.sharpness < 0.0))
        status = GIMP_PDB_CALLING_ERROR;
    }
    break;

  case GIMP_RUN_WITH_LAST_VALS:
    gimp_get_data ("plug_in_greycstoration", &params);
    break;

  default:
    break;
  } /* end case */

  if(status == GIMP_PDB_SUCCESS) {
    drawable = gimp_drawable_get(param[2].data.d_drawable);

    /* Process image */
    GimpPixelRgn srcPR, destPR;
    gint         x1, y1, x2, y2;

    /* Initialize pixel regions */
    gimp_pixel_rgn_init (&srcPR, drawable,
                         0, 0, drawable->width, drawable->height, FALSE, FALSE);
    gimp_pixel_rgn_init (&destPR, drawable,
                         0, 0, drawable->width, drawable->height, TRUE, TRUE);

    /* Get the input */
    gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

    /* Process region */
    process(&srcPR, &destPR, drawable->bpp, x1, x2, y1, y2, TRUE);

    /* Update image and clean */
    gimp_drawable_flush (drawable);
    gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
    gimp_drawable_update (drawable->drawable_id, x1, y1, x2 - x1, y2 - y1);
    gimp_displays_flush ();

    /* Set data for next use of filter */
    gimp_set_data ("plug_in_greycstoration", &params, sizeof(Params));

    gimp_drawable_detach(drawable);
    values[0].data.d_status = status;
  }

#ifdef TIMER
  g_printerr ("%f seconds\n", g_timer_elapsed (timer, NULL));
  g_timer_destroy (timer);
#endif
}


/*=== PROCESSING PROCEDURE ===================================================*/
static void process(GimpPixelRgn *srcPR,
                    GimpPixelRgn *destPR,
                    gint bytes, /* Bytes per pixel */
                    gint x1,    /* Corners of subregion */
                    gint x2,
                    gint y1,
                    gint y2,
                    gboolean show_progress) {

  gint width  = x2 - x1;
  gint height = y2 - y1;
  guchar* row, *row_ptr;

  if(show_progress) gimp_progress_init("De-noising image...");

  /* Make CImg instance and fill it with image information */
  gint channels = (bytes < 3) ? 1 : 3;
  img = CImg<unsigned char>(width, height, 1, channels, 0);
  row = g_new(guchar, width * bytes);
  for(gint y = 0; y < height; y++) {
    /* get source row */
    gimp_pixel_rgn_get_row(srcPR, row, x1, y1 + y, width);
    /*process row */
    row_ptr = row;
    for(gint x = 0; x < width; x++) {
      for (gint c = 0; c < channels; c++) {
        img(x, y, c) = row_ptr[c];
      }
      row_ptr += bytes;
    }
  }

  /* Actual processing - run GREYCstoration procedure */
  rungreycstor(&img, show_progress);

  /* Write processed image */
  for(gint y=0; y<height; y++) {
    /* get source row */
    gimp_pixel_rgn_get_row(srcPR, row, x1, y1 + y, width);
    /*process row */
    row_ptr = row;
    for(gint x=0; x<width; x++) {
      for (gint c = 0; c < channels; c++) {
        row_ptr[c] = static_cast<unsigned char>(img(x, y, c));
      }
      row_ptr += bytes;
    }
    /* set destination row */
    gimp_pixel_rgn_set_row(destPR, row, x1, y1 + y, width);
  }

  g_free(row);
}


static void rungreycstor(CImg<unsigned char> *pimg, gboolean show_progress) {
  for(gint iter=0; iter < params.iterations; iter++) {
    pimg->greycstoration_run(params.amplitude,
                           params.sharpness,
                           params.anisotropy,
                           params.alpha,
                           params.sigma,
                           1.0f,
                           params.dl,
                           params.da,
                           params.gauss_prec,
                           params.interp,
                           params.fast_approx,
                           show_progress ? TILESIZE : 0,
                           TILEBORDER,
                           show_progress ? NTHREADS : 1);
    gint tick = 0;
    do {
      /* Wait a little bit */
      cimg::wait(100);
      tick++;
      if(tick == 10 && show_progress) {
        /* Update progress */
        const float pr_iteration = img.greycstoration_progress();
        const unsigned int pr_global = (unsigned int)((iter*100 + pr_iteration) / params.iterations);
        gimp_progress_update(pr_global / 100.0);
        tick = 0;
      }
    } while (pimg->greycstoration_is_running());
  }
}


/*=== UPDATE PREVIEW =========================================================*/
static void preview_update(GimpPreview *preview) {

#ifdef ZOOMPREVIEW
  /* New zoomable preview */
  gint    width, height, bytes;
  guchar *src, *row_ptr;

  if(img.greycstoration_is_running()) img.greycstoration_stop();
  src = gimp_zoom_preview_get_source (GIMP_ZOOM_PREVIEW (preview),
                                &width, &height, &bytes);
  gint channels = (bytes < 3) ? 1 : 3;
  img = CImg<unsigned char>(width, height, 1, 3, 0);
  /* Read preview */
  row_ptr = src;
  for(gint y = 0; y < height; y++) {
    for(gint x = 0; x < width; x++) {
      for (gint c = 0; c < channels; c++) {
        img(x, y, c) = row_ptr[c];
      }
      row_ptr += bytes;
    }
  }
  /* Process preview */
  rungreycstor(&img, false);
  /* Write preview */
  row_ptr = src;
  for(gint y=0; y<height; y++) {
    for(gint x=0; x<width; x++) {
      for (gint c = 0; c < channels; c++) {
        row_ptr[c] = static_cast<unsigned char>(img(x, y, c));
      }
      row_ptr += bytes;
    }
  }
  gimp_preview_draw_buffer (preview, src, width * bytes);
  g_free (src);

#else
  /* Old style preview (without zoom) */
  GimpDrawable *drawable;
  gint x, y, width, height;
  GimpPixelRgn  srcPR, destPR;
  drawable = gimp_drawable_preview_get_drawable(GIMP_DRAWABLE_PREVIEW(preview));
  gimp_pixel_rgn_init(&srcPR, drawable,0, 0, drawable->width, drawable->height, FALSE, FALSE);
  gimp_pixel_rgn_init(&destPR, drawable,0, 0, drawable->width, drawable->height, TRUE, TRUE);
  gimp_preview_get_position(preview, &x, &y);
  gimp_preview_get_size(preview, &width, &height);

  /* Stop GREYCstoration if it's running */
  if(img.greycstoration_is_running()) img.greycstoration_stop();

  process(&srcPR, &destPR, drawable->bpp, x, x+width, y, y+height, FALSE);

  gimp_pixel_rgn_init(&destPR, drawable, x, y, width, height, FALSE, TRUE);
  gimp_drawable_preview_draw_region(GIMP_DRAWABLE_PREVIEW(preview), &destPR);
#endif
}


/*=== DIALOG WINDOW ==========================================================*/
static gboolean dialog (GimpDrawable *drawable) {
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  //GtkWidget *preview;
  GtkWidget *table;
  //GtkWidget *combo;
  //GtkWidget *button;
  //GtkObject *adj;
  gboolean run;

#define SCALE_WIDTH   150
#define ENTRY_WIDTH     4
#define RESPONSE_RESET 1

  gimp_ui_init("greycstoration", TRUE);
  runflag = FALSE;

  dialog = gimp_dialog_new ("GREYCstoration", "greycstoration",
                            NULL, (GtkDialogFlags) 0,
                            gimp_standard_help_func, "plug-in-greycstoration",
                            GIMP_STOCK_RESET, RESPONSE_RESET,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,
                            NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           RESPONSE_RESET,
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

#ifdef ZOOMPREVIEW
  gimp_window_set_transient (GTK_WINDOW (dialog));
#endif

  g_signal_connect (dialog, "response",
                    G_CALLBACK (callback_response),
                    preview);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

#ifdef ZOOMPREVIEW
  preview = gimp_zoom_preview_new (drawable);
#else
  preview = gimp_drawable_preview_new (drawable, &params.update_preview);
#endif
  gtk_box_pack_start (GTK_BOX (main_vbox), preview, TRUE, TRUE, 0);
  gtk_widget_show (preview);
  g_signal_connect (preview, "invalidated", G_CALLBACK (preview_update), NULL);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj_amplitude = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              "_Strength :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.amplitude, 0.0, 200.0, 1, 10, 1,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_amplitude, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.amplitude);
  g_signal_connect_swapped (adj_amplitude, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_sharpness = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                              "Contour preser_vation :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.sharpness, 0.0, 5.0, 0.05, 0.5, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_sharpness, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.sharpness);
  g_signal_connect_swapped (adj_sharpness, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_anisotropy = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                              "_Anisotropy :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.anisotropy, 0.0, 1.0, 0.05, 0.5, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_anisotropy, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.anisotropy);
  g_signal_connect_swapped (adj_anisotropy, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_alpha = gimp_scale_entry_new (GTK_TABLE (table), 0, 3,
                              "_Noise scale :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.alpha, 0.0, 16.0, 0.1, 0.5, 1,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_alpha, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.alpha);
  g_signal_connect_swapped (adj_alpha, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_sigma = gimp_scale_entry_new (GTK_TABLE (table), 0, 4,
                              "Geometry _regularity :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.sigma, 0, 8.0, 0.1, 0.5, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_sigma, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.sigma);
  g_signal_connect_swapped (adj_sigma, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_dl = gimp_scale_entry_new (GTK_TABLE (table), 0, 5,
                              "Spatial step :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.dl, 0.1, 1.0, 0.01, 0.1, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_dl, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.dl);
  g_signal_connect_swapped (adj_dl, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_da = gimp_scale_entry_new (GTK_TABLE (table), 0, 6,
                              "Angu_lar step :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.da, 1.0, 90.0, 1.0, 10.0, 1,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_da, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &params.da);
  g_signal_connect_swapped (adj_da, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  combo_interp = gimp_int_combo_box_new ("Nearest neighbor", 0,
                                  "Linear", 1,
                                  "Runge-Kutta", 2,
                                  NULL);
  gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo_interp), params.interp);
  gimp_table_attach_aligned(GTK_TABLE (table), 0, 8,
                "Interpolation _type :", 0.0, 0.5, combo_interp, 2, FALSE);
  g_signal_connect (combo_interp, "changed",
                    G_CALLBACK (gimp_int_combo_box_get_active),
                    &params.interp);
  g_signal_connect_swapped (combo_interp, "changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  button_fast_approx = gtk_check_button_new_with_mnemonic("_Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_fast_approx), params.fast_approx);
  gimp_table_attach_aligned(GTK_TABLE (table), 0, 9,
                "Approximation :", 0.0, 0.5, button_fast_approx, 2, FALSE);
  g_signal_connect (button_fast_approx, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &params.fast_approx);
  g_signal_connect_swapped (button_fast_approx, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj_iterations = gimp_scale_entry_new (GTK_TABLE (table), 0, 10,
                              "Number of _iterations :", SCALE_WIDTH, ENTRY_WIDTH,
                              params.iterations, 1.0, 30.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj_iterations, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &params.iterations);
  g_signal_connect_swapped (adj_iterations, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  gtk_widget_show (dialog);
  /*run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);
  gtk_widget_destroy (dialog);*/
  gtk_main ();
  return runflag;
}

static void
callback_response (GtkWidget *widget,
             gint       response_id,
             gpointer   data)
{
  switch (response_id)
    {
    case RESPONSE_RESET: /* Reset parameters to default values & update window */
      params = defparams;
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_amplitude), params.amplitude);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_sharpness), params.sharpness);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_anisotropy), params.anisotropy);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_alpha), params.alpha);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_sigma), params.sigma);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_dl), params.dl);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_da), params.da);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_iterations), params.iterations);
      gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo_interp), params.interp);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_fast_approx), params.fast_approx);
      gimp_preview_invalidate((GimpPreview*)preview);
      break;

    case GTK_RESPONSE_OK:
      runflag = TRUE;
      gtk_widget_destroy (widget);
      break;

    default:
      gtk_widget_destroy (widget);
      break;
    }
}
