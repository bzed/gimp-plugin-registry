/* 
 * Wavelet denoise GIMP plugin
 * 
 * wavelet-denoise.c
 * Copyright 2008 by Marco Rossini
 * 
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * Instructions:
 * compile with gimp-tool, eg. 'gimp-tool-2.0 --install wavelet-denoise.c'
 */

#include <stdlib.h>
#include <math.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#define SQR(x) ((x) * (x))
#define MAX2(x,y) ((x) > (y) ? (x) : (y))
#define MIN2(x,y) ((x) < (y) ? (x) : (y))
#define CLIP(x,min,max) MAX2((min), MIN2((x), (max)))

#define TIMER_READ (1.0 / 10)
#define TIMER_WRITE (1.0 / 7)
#define TIMER_PROCESS (1.0 - TIMER_READ - TIMER_WRITE)

#define MODE_YCBCR 0
#define MODE_RGB 1

/* Tool-tips */
#define TT_THR_AMOUNT_COLOUR "Adjusts the amount of denoising for the selected channel in a range from 0 (none) to 10000."
#define TT_THR_AMOUNT_GRAY "Adjusts the amount of denoising in a range from 0 (none) to 10000."
#define TT_SELECT "Select an image channel to edit its denoising thresholds."
#define TT_PREVIEW_ALL "Display all channels of the image (final image)."
#define TT_PREVIEW_SEL_GRAY "Display only the selected channel in grayscale mode."
#define TT_PREVIEW_SEL_COLOUR "Display only the selected channel in color mode."
#define TT_MODEL_YCBCR "The YCbCr color model has one luminance channel (Y) which contains most of the detail information of an image (such as brightness and contrast) and two chroma channels (Cb = blueness, Cr = reddness) that hold the color information. Note that this choice drastically affects the result."
#define TT_MODEL_RGB "The RGB color model separates an image into channels of red, green, and blue. This is the default color model in The GIMP. Note that this choice drastically affects the result."
#define TT_THR_DETAIL_COLOUR "Sets the amount of 'detail' to be conserved for the selected channel. This means that on a low setting a lot of low frequency noise is removed along with image details and on a high setting only high frequency noise is affected, conserving most of the image details and sharpness. The standard value is 1.0."
#define TT_THR_DETAIL_GRAY "Sets the amount of 'detail' to be conserved. This means that on a low setting a lot of low frequency noise is removed along with image details and on a high setting only high frequency noise is affected, conserving most of the image details and sharpness. The standard value is 1.0."
#define TT_RESET_PREVIEW "Resets the settings for the selected channel while the button is pressed."
#define TT_RESET_CHANNEL_GRAY "Resets to the default values."
#define TT_RESET_CHANNEL_COLOUR "Resets the current channel to the default values."
#define TT_RESET_ALL "Resets all (selectable) channels to the default values."

static void query (void);
static void run (const gchar * name, gint nparams, const GimpParam * param,
		 gint * nreturn_vals, GimpParam ** return_vals);
static void wavelet_denoise (float *fimg[3], unsigned int width,
			     unsigned int height, float threshold, double low,
			     float a, float b);
static void denoise (GimpDrawable * drawable, GimpPreview * preview);
static void rgb2ycbcr (float *r, float *g, float *b);
static void ycbcr2rgb (float *y, float *cb, float *cr);
static void set_rgb_mode (GtkWidget * w, gpointer data);
static void set_ycbcr_mode (GtkWidget * w, gpointer data);
static void set_preview_mode (GtkWidget * w, gpointer data);
static void set_preview_channel (GtkWidget * w, gpointer data);
static void set_threshold (GtkWidget * w, gpointer data);
static void set_low (GtkWidget * w, gpointer data);
static gboolean user_interface (GimpDrawable * drawable);
static void reset_channel (GtkWidget * w, gpointer data);
static void reset_all (GtkWidget * w, gpointer data);
static void temporarily_reset (GtkWidget * w, gpointer data);

GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, query, run };

MAIN ()
     static void query (void)
{
  static GimpParamDef args[] = {
    {GIMP_PDB_INT32, "run-mode", "Run mode"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"}
  };

  gimp_install_procedure ("plug-in-wavelet-denoise",
			  "Removes noise in the image using wavelets.",
			  "Removes noise in the image using wavelets.",
			  "Marco Rossini",
			  "Copyright Marco Rossini",
			  "2008",
			  "_Wavelet denoise",
			  "RGB*, GRAY*",
			  GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_menu_register ("plug-in-wavelet-denoise",
			     "<Image>/Filters/Enhance");
}

typedef struct
{
  guint gray_thresholds[2];
  double gray_low[2];
  guint colour_thresholds[4];
  double colour_low[4];
  guint colour_mode;
  gint preview_channel;
  gboolean preview_mode;
  gboolean preview;
  float times[3];
  gint winxsize, winysize;
} wavelet_settings;

static wavelet_settings settings = {
  {0, 0},			/* gray_thresholds */
  {1.0, 1.0},			/* gray_low */
  {0, 0, 0, 0},			/* colour_thresholds */
  {1.0, 1.0, 1.0, 1.0},		/* colour_low */
  MODE_YCBCR,			/* colour_mode */
  0,				/* preview_channel */
  1,				/* preview_mode */
  TRUE,				/* preview */
  {2.03, 2.67, 7.47},		/* times */
  -1, -1			/* winxsize, winysize */
};

static char *names_ycbcr[] = { "Y", "Cb", "Cr", "Alpha" };
static char *names_rgb[] = { "R", "G", "B", "Alpha" };
static char *names_gray[] = { "Gray", "Alpha" };

static float *fimg[4];
static float *buffer[3];
static gint channels;

GTimer *timer;

static void
run (const gchar * name, gint nparams, const GimpParam * param,
     gint * nreturn_vals, GimpParam ** return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpDrawable *drawable;
  gint i;

  timer = g_timer_new ();

  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  /* restore settings saved in GIMP core */
  gimp_get_data ("plug-in-wavelet-denoise", &settings);

  drawable = gimp_drawable_get (param[2].data.d_drawable);
  channels = gimp_drawable_bpp (drawable->drawable_id);

  if (settings.preview_channel > channels - 1)
    settings.preview_channel = 0;

  /* allocate buffers */
  /* FIXME: replace by GIMP funcitons */
  for (i = 0; i < channels; i++)
    {
      fimg[i] = (float *) malloc (drawable->width * drawable->height
				  * sizeof (float));
    }
  buffer[1] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));
  buffer[2] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));

  /* run GUI if in interactiv mode */
  run_mode = param[0].data.d_int32;
  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (!user_interface (drawable))
	{
	  gimp_drawable_detach (drawable);
	  /* FIXME: should return error status here */
	  return;
	}
    }

  denoise (drawable, NULL);

  /* free buffers */
  /* FIXME: replace by GIMP functions */
  for (i = 0; i < channels; i++)
    {
      free (fimg[i]);
    }
  free (buffer[1]);
  free (buffer[2]);

  gimp_displays_flush ();
  gimp_drawable_detach (drawable);

  /* save settings in the GIMP core */
  gimp_set_data ("plug-in-wavelet-denoise", &settings,
		 sizeof (wavelet_settings));
}

static void
denoise (GimpDrawable * drawable, GimpPreview * preview)
{
  GimpPixelRgn rgn_in, rgn_out;
  gint i, x1, y1, x2, y2, width, height, x, c;
  guchar *line;
  float val[4], times[3], totaltime;

  if (preview)
    {
      gimp_preview_get_position (preview, &x1, &y1);
      gimp_preview_get_size (preview, &width, &height);
      x2 = x1 + width;
      y2 = y1 + height;
    }
  else
    {
      gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
      width = x2 - x1;
      height = y2 - y1;
    }

  gimp_pixel_rgn_init (&rgn_in, drawable, x1, y1, width, height, FALSE,
		       FALSE);
  gimp_pixel_rgn_init (&rgn_out, drawable, x1, y1, width, height,
		       preview == NULL, TRUE);

  /* cache some tiles to make reading/writing faster */
  gimp_tile_cache_ntiles (drawable->width / gimp_tile_width () + 1);

  totaltime = settings.times[0];
  for (i = 0; i < channels; i++)
    {
      if (channels > 2 && settings.colour_thresholds[i] > 0)
	totaltime += settings.times[1];
      else if (channels < 0 && settings.gray_thresholds[i] > 0)
	totaltime += settings.times[1];
    }
  totaltime += settings.times[2];

  /* FIXME: replace by GIMP functions */
  line = (guchar *) malloc (channels * width * sizeof (guchar));

  /* read the full image from GIMP */
  if (!preview)
    gimp_progress_init ("Wavelet denoising...");
  times[0] = g_timer_elapsed (timer, NULL);
  for (i = 0; i < y2 - y1; i++)
    {
      if (!preview && i % 10 == 0)
	gimp_progress_update (settings.times[0] * i
			      / (double) height / totaltime);
      gimp_pixel_rgn_get_row (&rgn_in, line, x1, i + y1, width);

      /* convert pixel values to float and do colour model conv. */
      for (x = 0; x < width; x++)
	{
	  for (c = 0; c < channels; c++)
	    val[c] = (float) line[x * channels + c];
	  if (channels > 2 && settings.colour_mode == MODE_YCBCR)
	    rgb2ycbcr (&(val[0]), &(val[1]), &(val[2]));
	  /* save pixel values and scale for denoising */
	  for (c = 0; c < channels; c++)
	    fimg[c][i * width + x] = sqrt (val[c] * (1 << 24));
	}
    }
  times[0] = g_timer_elapsed (timer, NULL) - times[0];

  /* denoise the channels individually */
  times[1] = g_timer_elapsed (timer, NULL);
  /* FIXME: variable abuse: counter for number of denoised channels */
  x = 0;
  for (c = 0; c < channels; c++)
    {
      double a, b;
      /* in preview mode only process the displayed channel */
      if (preview && settings.preview_mode > 0 &&
	  settings.preview_channel != c)
	continue;
      buffer[0] = fimg[c];
      b = preview ? 0.0 : settings.times[1] / totaltime;
      a = settings.times[0] + x * settings.times[1];
      a /= totaltime;
      if (channels > 2 && settings.colour_thresholds[c] > 0)
	{
	  wavelet_denoise (buffer, width, height, (float)
			   settings.colour_thresholds[c],
			   settings.colour_low[c], a, b);
	  x++;
	}
      if (channels < 3 && settings.gray_thresholds > 0)
	{
	  wavelet_denoise (buffer, width, height, (float)
			   settings.gray_thresholds[c], settings.gray_low[c],
			   a, b);
	  x++;
	}
    }
  times[1] = g_timer_elapsed (timer, NULL) - times[1];
  times[1] /= x;

  /* retransform the image data
     (in preview mode calculate only the selected channel) */
  for (c = 0; c < channels; c++)
    {
      if (preview && settings.preview_mode > 0 &&
	  settings.preview_channel != c)
	continue;
      for (i = 0; i < width * height; i++)
	fimg[c][i] = SQR (fimg[c][i]) / 16777216.0;
    }

  /* in single channel preview mode set all channels equal */
  /* FIXME: put this in previous loop */
  if (preview && settings.preview_mode > 0)
    {
      for (c = 0; c < channels; c++)
	{
	  if (settings.preview_channel == c)
	    continue;
	  if (settings.preview_mode == 1)
	    for (i = 0; i < width * height; i++)
	      fimg[c][i] = fimg[settings.preview_channel][i];
	  else if (settings.preview_mode == 2)
	    /* FIXME: choice MODE_RGB or MODE_YCBCR outside the for loop */
	    for (i = 0; i < width * height; i++)
	      fimg[c][i] = (settings.colour_mode == MODE_RGB) ? 0.0 : 128.0;
	}
    }

  /* set alpha to full opacity in single channel preview mode */
  /* FIXME: put this in previous loop */
  if (preview && settings.preview_mode > 0 && channels % 2 == 0
      && (settings.preview_mode == 1
	  || settings.preview_channel != channels - 1))
    for (i = 0; i < width * height; i++)
      fimg[channels - 1][i] = 255;

  /* convert to RGB if necessary */
  if (channels > 2 && settings.colour_mode == MODE_YCBCR &&
      (!preview || settings.preview_mode != 1))
    {
      for (i = 0; i < width * height; i++)
	ycbcr2rgb (&(fimg[0][i]), &(fimg[1][i]), &(fimg[2][i]));
    }

  /* write the image back to GIMP */
  times[2] = g_timer_elapsed (timer, NULL);
  for (i = 0; i < height; i++)
    {
      if (!preview)
	gimp_progress_update ((settings.times[0] + channels
			       * settings.times[1] + settings.times[2]
			       * i / (double) height) / totaltime);

      /* scale and convert back to guchar */
      for (x = 0; x < width; x++)
	{
	  for (c = 0; c < channels; c++)
	    {
	      /* avoiding rounding errors !!! */
	      line[x * channels + c] =
		(guchar) CLIP (floor (fimg[c][i * width + x] + 0.5), 0, 255);
	    }
	}
      gimp_pixel_rgn_set_row (&rgn_out, line, x1, i + y1, width);
    }
  times[2] = g_timer_elapsed (timer, NULL) - times[2];

  if (!preview)
    {
      settings.times[0] = times[0];
      settings.times[1] = times[1];
      settings.times[2] = times[2];
    }

  /* FIXME: replace by gimp functions */
  free (line);

  if (preview)
    {
      gimp_drawable_preview_draw_region (GIMP_DRAWABLE_PREVIEW (preview),
					 &rgn_out);
      return;
    }
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, width, height);
}

/* code copied from UFRaw (which originates from dcraw) */
static void
hat_transform (float *temp, float *base, int st, int size, int sc)
{
  int i;
  for (i = 0; i < sc; i++)
    temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
  for (; i + sc < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
  for (; i < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)]
      + base[st * (2 * size - 2 - (i + sc))];
}

/* actual denoising algorithm. code copied from UFRaw (originates from dcraw) */
static void
wavelet_denoise (float *fimg[3], unsigned int width,
		 unsigned int height, float threshold, double low, float a,
		 float b)
{
  float *temp, thold;
  unsigned int i, lev, lpass, hpass, size, col, row;

  size = width * height;

  /* FIXME: replace by GIMP functions */
  temp = (float *) malloc (MAX2 (width, height) * sizeof (float));

  hpass = 0;
  for (lev = 0; lev < 5; lev++)
    {
      if (b != 0)
	gimp_progress_update (a + b * lev / 5.0);
      lpass = ((lev & 1) + 1);
      for (row = 0; row < height; row++)
	{
	  hat_transform (temp, fimg[hpass] + row * width, 1, width, 1 << lev);
	  for (col = 0; col < width; col++)
	    {
	      fimg[lpass][row * width + col] = temp[col] * 0.25;
	    }
	}
      if (b != 0)
	gimp_progress_update (a + b * (lev + 0.4) / 5.0);
      for (col = 0; col < width; col++)
	{
	  hat_transform (temp, fimg[lpass] + col, width, height, 1 << lev);
	  for (row = 0; row < height; row++)
	    {
	      fimg[lpass][row * width + col] = temp[row] * 0.25;
	    }
	}
      if (b != 0)
	gimp_progress_update (a + b * (lev + 0.8) / 5.0);

      thold =
	threshold * exp (-2.6 * sqrt (lev + 1) * low * low) * 0.8002 /
	exp (-2.6 * low * low);
      for (i = 0; i < size; i++)
	{
	  fimg[hpass][i] -= fimg[lpass][i];
	  if (fimg[hpass][i] < -thold)
	    fimg[hpass][i] += thold;
	  else if (fimg[hpass][i] > thold)
	    fimg[hpass][i] -= thold;
	  else
	    fimg[hpass][i] = 0;

	  if (hpass)
	    fimg[0][i] += fimg[hpass][i];
	}
      hpass = lpass;
    }

  for (i = 0; i < size; i++)
    fimg[0][i] = fimg[0][i] + fimg[lpass][i];

  /* FIXME: replace by GIMP functions */
  free (temp);
}

/* ### GUI code below ###################################################### */

/* declare all widgets global (static) to make things a lot easier */

/* colour mode frame */
static GtkWidget *fr_mode, *mode_radio[2], *mode_vbox;
static GSList *mode_list;

/* preview select frame */
static GtkWidget *fr_preview, *preview_radio[3], *preview_vbox;
static GSList *preview_list;

/* channel select frame */
static GtkWidget *fr_channel, *channel_radio[4], *channel_vbox;
static GSList *channel_list;

/* threshold frame */
static GtkWidget *fr_threshold, *thr_label[2], *thr_spin[2];
static GtkWidget *thr_hbox[2], *thr_vbox, *thr_scale[2];
static GtkObject *thr_adj[2];

/* reset buttons */
static GtkWidget *reset_button[2], *reset_hbox, *reset_align;

/* dialog */
static GtkWidget *dialog, *dialog_hbox, *dialog_vbox;
static GtkWidget *preview, *preview_reset, *preview_hbox;

static GtkWidget **radios_labels[] = { channel_radio, thr_label };

static char **names;

static gboolean
user_interface (GimpDrawable * drawable)
{
  /* can ui code be beautiful? */
  gboolean run;
  gint i;

  if (channels < 3)
    {
      names = names_gray;
    }
  else
    {
      if (settings.colour_mode == MODE_YCBCR)
	names = names_ycbcr;
      else
	names = names_rgb;
    }

  gimp_ui_init ("Wavelet denoise", FALSE);

  /* prepare the preview */
  preview = gimp_drawable_preview_new (drawable, &settings.preview);
  preview_hbox = gimp_preview_get_controls ((GimpPreview *) preview);
  g_signal_connect_swapped (preview, "invalidated", G_CALLBACK (denoise),
			    drawable);
  gtk_widget_show (preview);
  if (channels > 1)
    {
      preview_reset = gtk_button_new_with_label ("Reset");
      g_signal_connect (preview_reset, "pressed",
			G_CALLBACK (temporarily_reset), (gpointer) 1);
      g_signal_connect (preview_reset, "released",
			G_CALLBACK (temporarily_reset), (gpointer) 0);
      gtk_box_pack_start (GTK_BOX (preview_hbox), preview_reset, TRUE, TRUE,
			  0);
      gtk_widget_set_tooltip_text (preview_reset, TT_RESET_PREVIEW);
      gtk_widget_show (preview_reset);
    }

  /* prepare the colour mode frame */
  if (channels > 2)
    {
      fr_mode = gtk_frame_new ("Color model");
      mode_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (mode_vbox), 5);
      mode_radio[0] = gtk_radio_button_new_with_label (NULL,
						       channels ==
						       3 ? "YCbCr" :
						       "YCbCr(A)");
      mode_list =
	gtk_radio_button_get_group (GTK_RADIO_BUTTON (mode_radio[0]));
      mode_radio[1] =
	gtk_radio_button_new_with_label (mode_list,
					 channels == 3 ? "RGB" : "RGB(A)");
      if (settings.colour_mode == MODE_YCBCR)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_radio[0]), 1);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_radio[1]), 1);
      g_signal_connect (mode_radio[0], "toggled",
			G_CALLBACK (set_ycbcr_mode), radios_labels);
      g_signal_connect (mode_radio[1], "toggled",
			G_CALLBACK (set_rgb_mode), radios_labels);
      g_signal_connect_swapped (mode_radio[0], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      g_signal_connect_swapped (mode_radio[1], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_container_add (GTK_CONTAINER (fr_mode), mode_vbox);
      gtk_box_pack_start (GTK_BOX (mode_vbox), mode_radio[0], FALSE,
			  FALSE, 0);
      gtk_box_pack_start (GTK_BOX (mode_vbox), mode_radio[1], FALSE,
			  FALSE, 0);
      gtk_widget_set_tooltip_text (mode_radio[0], TT_MODEL_YCBCR);
      gtk_widget_set_tooltip_text (mode_radio[1], TT_MODEL_RGB);
      gtk_widget_show (mode_radio[0]);
      gtk_widget_show (mode_radio[1]);
      gtk_widget_show (mode_vbox);
      gtk_widget_show (fr_mode);
    }
  else
    fr_mode = NULL;

  /* prepere the preview select frame */
  if (channels > 1)
    {
      fr_preview = gtk_frame_new ("Preview channel");
      preview_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (preview_vbox), 5);
      gtk_container_add (GTK_CONTAINER (fr_preview), preview_vbox);

      preview_list = NULL;
      preview_radio[0] = gtk_radio_button_new_with_label (preview_list,
							  "All");
      if (settings.preview_mode == 0)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preview_radio[0]),
				      1);
      gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[0], FALSE,
			  FALSE, 0);
      g_signal_connect (preview_radio[0], "toggled",
			G_CALLBACK (set_preview_mode), (gpointer) 0);
      g_signal_connect_swapped (preview_radio[0], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_widget_set_tooltip_text (preview_radio[0], TT_PREVIEW_ALL);
      gtk_widget_show (preview_radio[0]);

      preview_list =
	gtk_radio_button_get_group (GTK_RADIO_BUTTON (preview_radio[0]));
      preview_radio[1] =
	gtk_radio_button_new_with_label (preview_list,
					 (channels >
					  2) ? "Selected (gray)" :
					 "Selected");
      if (settings.preview_mode == 1)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preview_radio[1]),
				      1);
      gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[1], FALSE,
			  FALSE, 0);
      g_signal_connect (preview_radio[1], "toggled",
			G_CALLBACK (set_preview_mode), (gpointer) 1);
      g_signal_connect_swapped (preview_radio[1], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_widget_set_tooltip_text (preview_radio[1], TT_PREVIEW_SEL_GRAY);
      gtk_widget_show (preview_radio[1]);

      if (channels > 2)
	{
	  preview_list =
	    gtk_radio_button_get_group (GTK_RADIO_BUTTON (preview_radio[1]));
	  preview_radio[2] =
	    gtk_radio_button_new_with_label (preview_list,
					     "Selected (color)");
	  if (settings.preview_mode == 2)
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					  (preview_radio[2]), 1);
	  gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[2], FALSE,
			      FALSE, 0);
	  g_signal_connect (preview_radio[2], "toggled",
			    G_CALLBACK (set_preview_mode), (gpointer) 2);
	  g_signal_connect_swapped (preview_radio[2], "toggled",
				    G_CALLBACK (gimp_preview_invalidate),
				    preview);
	  gtk_widget_set_tooltip_text (preview_radio[2],
				       TT_PREVIEW_SEL_COLOUR);
	  gtk_widget_show (preview_radio[2]);
	}
      gtk_widget_show (preview_vbox);
      gtk_widget_show (fr_preview);
    }
  else
    fr_preview = NULL;

  /* prepare the channel select frame */
  if (channels > 1)
    {
      fr_channel = gtk_frame_new ("Channel select");
      channel_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (channel_vbox), 5);
      gtk_container_add (GTK_CONTAINER (fr_channel), channel_vbox);

      channel_list = NULL;
      for (i = 0; i < channels; i++)
	{
	  channel_radio[i] =
	    gtk_radio_button_new_with_label (channel_list, names[i]);
	  if (settings.preview_channel == i)
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					  (channel_radio[i]), 1);
	  g_signal_connect (channel_radio[i], "toggled",
			    G_CALLBACK (set_preview_channel), (gpointer) i);
	  g_signal_connect_swapped (channel_radio[i], "toggled",
				    G_CALLBACK (gimp_preview_invalidate),
				    preview);
	  gtk_box_pack_start (GTK_BOX (channel_vbox), channel_radio[i], FALSE,
			      FALSE, 0);
	  gtk_widget_set_tooltip_text (channel_radio[i], TT_SELECT);
	  gtk_widget_show (channel_radio[i]);
	  channel_list =
	    gtk_radio_button_get_group (GTK_RADIO_BUTTON (channel_radio[i]));
	}
      gtk_widget_show (channel_vbox);
      gtk_widget_show (fr_channel);
    }
  else
    fr_channel = NULL;

  /* prepare the threshold frame */
  fr_threshold =
    gtk_frame_new ((channels > 1) ? "Channel thresholds" : "Thresholds");
  gtk_widget_show (fr_threshold);
  thr_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (fr_threshold), thr_vbox);
  gtk_container_border_width (GTK_CONTAINER (thr_vbox), 5);
  gtk_widget_show (thr_vbox);

  thr_hbox[0] = gtk_hbox_new (FALSE, 10);
  thr_label[0] = gtk_label_new ("Amount");
  gtk_misc_set_alignment (GTK_MISC (thr_label[0]), 0.0, 0.0);
  if (channels > 2)
    thr_adj[0] =
      gtk_adjustment_new (settings.
			  colour_thresholds[settings.preview_channel], 0,
			  10000, 100, 100, 0);
  else
    thr_adj[0] =
      gtk_adjustment_new (settings.gray_thresholds[settings.preview_channel],
			  0, 10000, 100, 100, 0);
  thr_spin[0] = gtk_spin_button_new (GTK_ADJUSTMENT (thr_adj[0]), 1, 0);
  thr_scale[0] = gtk_hscale_new (GTK_ADJUSTMENT (thr_adj[0]));
  gtk_scale_set_draw_value (GTK_SCALE (thr_scale[0]), FALSE);
  gtk_box_pack_start (GTK_BOX (thr_vbox), thr_hbox[0], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_label[0], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_scale[0], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_spin[0], FALSE, FALSE, 0);
  g_signal_connect_swapped (thr_adj[0], "value_changed",
			    G_CALLBACK (gimp_preview_invalidate), preview);
  g_signal_connect (thr_adj[0], "value_changed",
		    G_CALLBACK (set_threshold), NULL);
  gtk_widget_show (thr_scale[0]);
  gtk_widget_show (thr_spin[0]);
  gtk_widget_show (thr_label[0]);
  if (channels > 1)
    {
      gtk_widget_set_tooltip_text (thr_label[0], TT_THR_AMOUNT_COLOUR);
      gtk_widget_set_tooltip_text (thr_scale[0], TT_THR_AMOUNT_COLOUR);
      gtk_widget_set_tooltip_text (thr_spin[0], TT_THR_AMOUNT_COLOUR);
    }
  else
    {
      gtk_widget_set_tooltip_text (thr_label[0], TT_THR_AMOUNT_GRAY);
      gtk_widget_set_tooltip_text (thr_scale[0], TT_THR_AMOUNT_GRAY);
      gtk_widget_set_tooltip_text (thr_spin[0], TT_THR_AMOUNT_GRAY);
    }
  gtk_widget_show (thr_hbox[0]);

  thr_hbox[1] = gtk_hbox_new (FALSE, 10);
  thr_label[1] = gtk_label_new ("Detail");
  gtk_misc_set_alignment (GTK_MISC (thr_label[1]), 0.0, 0.0);
  if (channels > 2)
    thr_adj[1] =
      gtk_adjustment_new (settings.
			  colour_low[settings.preview_channel], 0,
			  2, 0.01, 0.01, 0);
  else
    thr_adj[1] =
      gtk_adjustment_new (settings.gray_low[settings.preview_channel],
			  0, 2, 0.01, 0.01, 0);
  thr_spin[1] = gtk_spin_button_new (GTK_ADJUSTMENT (thr_adj[1]), 0.01, 2);
  thr_scale[1] = gtk_hscale_new (GTK_ADJUSTMENT (thr_adj[1]));
  gtk_scale_set_draw_value (GTK_SCALE (thr_scale[1]), FALSE);
  gtk_box_pack_start (GTK_BOX (thr_vbox), thr_hbox[1], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_label[1], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_scale[1], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_spin[1], FALSE, FALSE, 0);
  g_signal_connect_swapped (thr_adj[1], "value_changed",
			    G_CALLBACK (gimp_preview_invalidate), preview);
  g_signal_connect (thr_adj[1], "value_changed", G_CALLBACK (set_low), NULL);
  gtk_widget_show (thr_scale[1]);
  gtk_widget_show (thr_spin[1]);
  gtk_widget_show (thr_label[1]);
  if (channels > 1)
    {
      gtk_widget_set_tooltip_text (thr_label[1], TT_THR_DETAIL_COLOUR);
      gtk_widget_set_tooltip_text (thr_scale[1], TT_THR_DETAIL_COLOUR);
      gtk_widget_set_tooltip_text (thr_spin[1], TT_THR_DETAIL_COLOUR);
    }
  else
    {
      gtk_widget_set_tooltip_text (thr_label[1], TT_THR_DETAIL_GRAY);
      gtk_widget_set_tooltip_text (thr_scale[1], TT_THR_DETAIL_GRAY);
      gtk_widget_set_tooltip_text (thr_spin[1], TT_THR_DETAIL_GRAY);
    }
  gtk_widget_show (thr_hbox[1]);

  /* prepare the reset buttons */
  reset_align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  reset_hbox = gtk_hbox_new (FALSE, 10);
  reset_button[0] =
    gtk_button_new_with_label ((channels > 1) ? "Reset channel" : "Reset");
  g_signal_connect (reset_button[0], "clicked", G_CALLBACK (reset_channel),
		    NULL);
  gtk_box_pack_start (GTK_BOX (reset_hbox), reset_button[0], FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (reset_button[0],
			       (channels >
				1) ? TT_RESET_CHANNEL_COLOUR :
			       TT_RESET_CHANNEL_GRAY);
  gtk_widget_show (reset_button[0]);

  if (channels > 1)
    {
      reset_button[1] = gtk_button_new_with_label ("Reset all");
      gtk_button_set_use_stock (GTK_BUTTON (reset_button[1]), TRUE);
      g_signal_connect (reset_button[1], "clicked", G_CALLBACK (reset_all),
			NULL);
      g_signal_connect_swapped (reset_button[1], "clicked",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_box_pack_start (GTK_BOX (reset_hbox), reset_button[1], FALSE, FALSE,
			  0);
      gtk_widget_set_tooltip_text (reset_button[1], TT_RESET_ALL);
      gtk_widget_show (reset_button[1]);
    }
  gtk_container_add (GTK_CONTAINER (reset_align), reset_hbox);
  gtk_widget_show (reset_hbox);
  gtk_widget_show (reset_align);

  /* prepeare the dialog */
  dialog_vbox = gtk_vbox_new (FALSE, 10);
  dialog_hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_set_homogeneous (GTK_BOX (dialog_hbox), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_vbox), 10);
  dialog = gimp_dialog_new ("Wavelet denoise", "wavelet denoise", NULL, 0,
			    gimp_standard_help_func,
			    "plug-in-wavelet-denoise", GTK_STOCK_CANCEL,
			    GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
			    GTK_RESPONSE_OK, NULL);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), preview, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), dialog_hbox, FALSE, FALSE, 0);
  if (fr_preview)
    gtk_box_pack_start (GTK_BOX (dialog_hbox), fr_preview, TRUE, TRUE, 0);
  if (fr_channel)
    gtk_box_pack_start (GTK_BOX (dialog_hbox), fr_channel, TRUE, TRUE, 0);
  if (fr_mode)
    gtk_box_pack_start (GTK_BOX (dialog_hbox), fr_mode, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), fr_threshold, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), reset_align, FALSE, FALSE, 0);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  if (settings.winxsize > 0 && settings.winysize > 0)
    gtk_window_resize (GTK_WINDOW (dialog), settings.winxsize,
		       settings.winysize);

  gtk_widget_show (dialog_hbox);
  gtk_widget_show (dialog_vbox);
  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_window_get_size (GTK_WINDOW (dialog), &(settings.winxsize),
		       &(settings.winysize));

  /* FIXME: destroy all widgets - memory leak! */
  gtk_widget_destroy (dialog);

  return run;
}

static void
temporarily_reset (GtkWidget * w, gpointer data)
{
  static int threshold = -1;
  static double low = -1.0;
  int do_reset = !!data;
  int current = settings.preview_channel;
  if (do_reset)
    {
      if (threshold >= 0 || low >= 0.0)
	g_print ("wavelet denoise: impossible event" "happened!\n");
      if (channels > 2)
	{
	  threshold = settings.colour_thresholds[current];
	  low = settings.colour_low[current];
	  settings.colour_thresholds[current] = 0;
	  settings.colour_low[current] = 1.0;
	}
      else
	{
	  threshold = settings.gray_thresholds[current];
	  low = settings.gray_low[current];
	  settings.gray_thresholds[current] = 0;
	  settings.gray_low[current] = 1.0;
	}
    }
  else
    {
      if (channels > 2)
	{
	  settings.colour_thresholds[current] = threshold;
	  settings.colour_low[current] = low;
	}
      else
	{
	  settings.gray_thresholds[current] = threshold;
	  settings.gray_low[current] = low;
	}
      threshold = -1;
      low = -1.0;
    }
  gimp_preview_invalidate ((GimpPreview *) preview);
}

static void
reset_channel (GtkWidget * w, gpointer data)
{
  if (channels > 2)
    {
      settings.colour_thresholds[settings.preview_channel] = 0;
      settings.colour_low[settings.preview_channel] = 1.0;
    }
  else
    {
      settings.gray_thresholds[settings.preview_channel] = 0;
      settings.gray_low[settings.preview_channel] = 1.0;
    }
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]), 0);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]), 1.0);
}

static void
reset_all (GtkWidget * w, gpointer data)
{
  if (channels > 2)
    {
      settings.colour_thresholds[0] = 0;
      settings.colour_thresholds[1] = 0;
      settings.colour_thresholds[2] = 0;
      settings.colour_low[0] = 1.0;
      settings.colour_low[1] = 1.0;
      settings.colour_low[2] = 1.0;
      if (channels == 4)
	{
	  settings.colour_thresholds[3] = 0;
	  settings.colour_low[3] = 1.0;
	}
    }
  else
    {
      settings.gray_thresholds[0] = 0;
      settings.gray_low[0] = 1.0;
      if (channels == 2)
	{
	  settings.gray_thresholds[1] = 0;
	  settings.gray_low[1] = 1.0;
	}
    }
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]), 0);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]), 1.0);
}

static void
set_rgb_mode (GtkWidget * w, gpointer data)
{
  /* change labels in the dialog */
  names = names_rgb;
  gtk_button_set_label (GTK_BUTTON (channel_radio[0]), "R");
  gtk_button_set_label (GTK_BUTTON (channel_radio[1]), "G");
  gtk_button_set_label (GTK_BUTTON (channel_radio[2]), "B");
  settings.colour_mode = MODE_RGB;
}

static void
set_ycbcr_mode (GtkWidget * w, gpointer data)
{
  /* change labels in the dialog */
  names = names_ycbcr;
  gtk_button_set_label (GTK_BUTTON (channel_radio[0]), "Y");
  gtk_button_set_label (GTK_BUTTON (channel_radio[1]), "Cb");
  gtk_button_set_label (GTK_BUTTON (channel_radio[2]), "Cr");
  settings.colour_mode = MODE_YCBCR;
}

static void
set_preview_mode (GtkWidget * w, gpointer data)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    return;
  settings.preview_mode = (gint) data;
}

static void
set_preview_channel (GtkWidget * w, gpointer data)
{
  gint c = (gint) data;
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    return;
  settings.preview_channel = c;
  if (channels > 2)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]),
				settings.colour_thresholds[c]);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]),
				settings.colour_low[c]);
    }
  else
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]),
				settings.gray_thresholds[c]);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]),
				settings.gray_low[c]);
    }
}

static void
rgb2ycbcr (float *r, float *g, float *b)
{
  /* using JPEG conversion here - expecting all channels to be
   * in [0:255] range */
  static float y, cb, cr;
  y = 0.2990 * *r + 0.5870 * *g + 0.1140 * *b;
  cb = -0.1687 * *r - 0.3313 * *g + 0.5000 * *b + 128.0;
  cr = 0.5000 * *r - 0.4187 * *g - 0.0813 * *b + 128.0;
  *r = y;
  *g = cb;
  *b = cr;
}

static void
ycbcr2rgb (float *y, float *cb, float *cr)
{
  /* using JPEG conversion here - expecting all channels to be
   * in [0:255] range */
  static float r, g, b;
  r = *y + 1.40200 * (*cr - 128.0);
  g = *y - 0.34414 * (*cb - 128.0) - 0.71414 * (*cr - 128.0);
  b = *y + 1.77200 * (*cb - 128.0);
  *y = r;
  *cb = g;
  *cr = b;
}

static void
set_threshold (GtkWidget * w, gpointer data)
{
  double val;
  val = gtk_adjustment_get_value (GTK_ADJUSTMENT (w));
  if (channels > 2)
    settings.colour_thresholds[settings.preview_channel] = (int) val;
  else
    settings.gray_thresholds[settings.preview_channel] = (int) val;
}
static void
set_low (GtkWidget * w, gpointer data)
{
  double val;
  val = gtk_adjustment_get_value (GTK_ADJUSTMENT (w));
  if (channels > 2)
    settings.colour_low[settings.preview_channel] = val;
  else
    settings.gray_low[settings.preview_channel] = val;
}
