/* Focus Blur -- blur with focus plug-in.
 * Copyright (C) 2002-2006 Kyoichiro Suda
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Focus Blur plug-in version 2.3, Sep 2006
 *
 * This plug-in is a kind of blur. It is able to show a out of focus.
 * This blur has four Models it is used for diffusion of color. You
 * might choose also Brush instead of these models. you can specify a
 * Distance map. It makes distance for each pixels in source image.
 * a amount of blur is decided by Radius and difference of two values.
 * They are distance of pixel and Focus. You could specify threshold
 * of Light. It would make a scene has natural shine as you have seen.
 *
 * How to build
 *
 *  CFLAGS="-O2 -DG_DISABLE_ASSERT -DFBLUR_PREVIEW=1 -DENABLE_NLS" \
 *      gimptool --install fblur-2.3.c
 *
 *  FBLUR_PREVIEW:      No preview (0), Fblur original (1), Gimp 2.2 (2)
 *  ENABLE_NLS:         Native language support.
 *  DISTRIBUTER:        Information about binary distributer.
 *
 * Changes
 *
 *  added erasing algo for shining.
 *  added utility for focus animation.
 *  tuned "Depth order". deal with non-flat distance.
 *  renamed Gap to Difference. (can it be understood?)
 *
 * TODO
 *
 *  fix srcimg about selection rectangle. radius should look outside of it.
 *   or inspect selection value in each pixels.
 *  remove or write depth division.
 *  split source.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gdk/gdkkeysyms.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#define TILE_WIDTH  gimp_tile_width()
#define TILE_HEIGHT gimp_tile_height()

#define PLUG_IN_NAME    "plug_in_focus_blur"
#define PLUG_IN_VERSION "2.3"
#define PREVIEW_SIZE     128
#define SCALE_WIDTH      256

#ifdef ENABLE_NLS

#define GETTEXT_PACKAGE "gimp20-fblur"

#include <libintl.h>
#define _(String) gettext (String)
#ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#else
#    define N_(String) (String)
#endif

/* #define bind_textdomain_codeset(Domain, Codeset) (Domain) */
#define INIT_I18N()     G_STMT_START{                           \
  bindtextdomain (GETTEXT_PACKAGE, gimp_locale_directory ());   \
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");           \
  textdomain (GETTEXT_PACKAGE);                                 \
}G_STMT_END

#else /* ENABLE_NLS */

#define _(String) (String)
#define N_(String) (String)

#endif /* ENABLE_NLS */

#if (FBLUR_PREVIEW == 2)
#define PREVIEW_INVALIDATE(widget,signal) g_signal_connect_swapped (widget, signal, G_CALLBACK (gimp_preview_invalidate), preview)
#else
#if (FBLUR_PREVIEW == 1)
#define PREVIEW_INVALIDATE(widget,signal) g_signal_connect_swapped (widget, signal, G_CALLBACK (fblur_preview_invalidate), preview->viewport)
#else
#define PREVIEW_INVALIDATE(widget,signal) {}
#endif
#endif

#define COLORMAX        255
#define COLORBUF        guint8

#define SWAP(a,b) G_STMT_START{ a ^= b; b ^= a; a ^= b; }G_STMT_END


/*---- Types ---*/

typedef enum
{
  SHINE_NONE,
  SHINE_INTENSITY,
  SHINE_HSV,
  SHINE_MAP,
} ShineMode;

typedef enum
{
  DWEIGHT_NONE,
  DWEIGHT_OBSTRUCTION,
  DWEIGHT_DEPTH_ORDER,
} DistanceMode;

typedef enum
{
  REPORT_NONE,
  REPORT_TIME_CON,
  REPORT_TIME_WIN,
} ReportMode;

typedef struct
{
  gint          model;
  gdouble       radius;
  gdouble       softness;
  gdouble       fill;
  gdouble       bscale;
  gboolean      use_modeladj;
  gdouble       diff;
  gdouble       direction;
  gint          focus;
  gboolean      use_map;
  gint32        map_id;
  gint          near;
  DistanceMode  dist_mode;
  gboolean      use_shine;
  ShineMode     shine_mode;
  gint32        shine_map_id;
  gint          light;
  gdouble       saturation;
  gdouble       shine_level;
  gdouble       shine_curve;
  gboolean	erase_white;
  gdouble	erase_white_val;
  gdouble       whiteglow;
  gboolean      preview;
  gboolean	focus_anim;
  gint		focus_start;
  gint		focus_end;
  gdouble	focus_curve;
  gint		focus_wait;
  gint		focus_div;
  ReportMode    report;
} FblurValues;

typedef enum
{
  FBLUR_WIDGET_DIALOG,
  FBLUR_WIDGET_MODEL,
  FBLUR_WIDGET_RADIUS,
  FBLUR_WIDGET_SOFTNESS,
  FBLUR_WIDGET_FILL,
  FBLUR_WIDGET_FILL_BOX,
  FBLUR_WIDGET_BSCALE,
  FBLUR_WIDGET_BSCALE_BOX,
  FBLUR_WIDGET_USE_MODELADJ,
  FBLUR_WIDGET_DIFF,
  FBLUR_WIDGET_DIRECTION,
  FBLUR_WIDGET_BRUSH_SEL,
  FBLUR_WIDGET_FOCUS,
  FBLUR_WIDGET_USE_MAP,
  FBLUR_WIDGET_MAP_ID,
  FBLUR_WIDGET_NEAR,
  FBLUR_WIDGET_DIST_MODE,
  FBLUR_WIDGET_USE_SHINE,
  FBLUR_WIDGET_SHINE_MODE,
  FBLUR_WIDGET_LIGHT,
  FBLUR_WIDGET_LIGHT_LABEL,
  FBLUR_WIDGET_SATURATION,
  FBLUR_WIDGET_SATURATION_LABEL,
  FBLUR_WIDGET_LEVEL,
  FBLUR_WIDGET_LEVEL_LABEL,
  FBLUR_WIDGET_CURVE,
  FBLUR_WIDGET_CURVE_LABEL,
  FBLUR_WIDGET_RESET_LEVEL_CURVE,
  FBLUR_WIDGET_ERASE_WHITE_BOX,
  FBLUR_WIDGET_ERASE_WHITE,
  FBLUR_WIDGET_ERASE_WHITE_VAL,
  FBLUR_WIDGET_WHITEGLOW,
  FBLUR_WIDGET_WHITEGLOW_LABEL,
  FBLUR_WIDGET_FOCUS_ANIM,
  FBLUR_WIDGET_FOCUS_START,
  FBLUR_WIDGET_FOCUS_END,
  FBLUR_WIDGET_FOCUS_CURVE,
  FBLUR_WIDGET_FOCUS_WAIT,
  FBLUR_WIDGET_FOCUS_DIV,
  FBLUR_WIDGET_REPORT_TIME_WIN,
  FBLUR_WIDGET_REPORT_TIME_CON,
  FBLUR_WIDGET_NOTEBOOK,
  FBLUR_WIDGET_MENU_AUTO,
  FBLUR_WIDGET_MENU_DBLSIZE,
  NUM_FBLUR_WIDGETS
} FblurWidgets;

typedef struct
{
  guint         x, y;
  guint         w, h;
  gint          bpp;
  gint          ch;
  gboolean      is_rgb;
  gboolean      has_alpha;
  gsize         rowstride;
  gsize         length;
  COLORBUF      data[1];
} ImageBuffer;

typedef struct
{
  guint         width;
  guint         height;
  guchar        data[1];
} MapBuffer;

typedef struct
{
  guint         x, y;
  guint         w, h;
  gdouble       data[1];
} ShineBuffer;

typedef struct
{
  gchar         *name;
  guint          width;
  guint          height;
  gboolean       is_color;
  gboolean       new;   /* for dtable */
  gsize          length;
  guint8         data[1];
} BrushData;

typedef enum
{
  MODEL_FLAT,
  MODEL_RING,
  MODEL_CONCAVE,
  MODEL_GAUSS,
  MODEL_BRUSH,
} ModelTypes;

/* diffusion table */
typedef struct
{
  gdouble        density[4];
  gdouble       *value[4];
} DiffusionTableSub;

typedef struct
{
  ModelTypes             model;
  BrushData             *brush;
  gboolean               color;
  gdouble                radius;
  gdouble                diff_x, diff_y;
  gint                   x_min, x_max;
  gint                   y_min, y_max;
  guint                  width;
  guint                  height;
  guint                  ntables;
  DiffusionTableSub     *table;
  guint                  nelements;
  gsize                  length;
  gdouble               *data;
} DiffusionTable;


#if (FBLUR_PREVIEW == 2)

typedef struct
{
  guint                  idle;
  guint                  idle_draw;
  gint                   com_line;
  gint                   draw_line;
  GimpPreview           *preview;
  ImageBuffer           *src;
  ImageBuffer           *dest;
  DiffusionTable        *dtable;
  ModelTypes             model;
  gint                   focus;
  DistanceMode           dist_mode;
  MapBuffer             *map;
  ShineBuffer           *shine;
  gboolean               use_color;
  gdouble                whiteglow;
} PreviewValues;

static PreviewValues fblur_preview =
  { 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL, FALSE, 0.0 };

#endif

#if (FBLUR_PREVIEW == 1)

typedef enum
{
  PREVIEW_CANCEL,
  PREVIEW_STANDBY,
  PREVIEW_CURSOR_SET,
  PREVIEW_CURSOR_DRAG,
  PREVIEW_CURSOR_OFF,
  PREVIEW_PREVIEW,
  PREVIEW_PICKUP,
} PreviewStatusType;

typedef struct _FblurPreview FblurPreview;

typedef struct
{
  guchar shift;
  guchar c[2];
} CheckValues;

typedef struct
{
  FblurPreview          *parent;
  GtkWidget             *widget;
  gboolean               active;
  PreviewStatusType      status;
  gint                   width, height;
  gint                   grip_x, grip_y;
  gint                   old_x,  old_y;
  gint                   cursor_x, cursor_y;
  gint                   cursor_w, cursor_h;
  gboolean               cursor_on;
  GdkGC                 *cursor_gc;
  gint                   dash_offset;
  guint                  dash_interval;
  guint                  idle;
  gint                   com_line;
  gdouble                run_y;
  gdouble               *sum;
  ImageBuffer           *img;
} FblurPreviewNavigate;

typedef struct
{
  FblurPreview          *parent;
  GtkWidget             *widget;
  GtkWidget             *menu;
  GtkWidget             *picker;
  gboolean               active;
  PreviewStatusType      status;
  gint                   grip_x, grip_y;
  gint                   pick_x, pick_y;
  guint32                pre_time;
  gboolean               dblsize;
  guint                  idle;
  guint                  idle_draw;
  gint                   com_line;
  gint                   draw_line;
  DiffusionTable        *dtable;
  ModelTypes             model;
  gint                   focus;
  DistanceMode           dist_mode;
  MapBuffer             *map;
  ShineBuffer           *shine;
  gboolean               use_color;
  gdouble                whiteglow;
  ImageBuffer           *img;
} FblurPreviewViewport;

struct _FblurPreview
{
  GtkWidget             *widget;
  GtkWidget             *indicater;
  gboolean               active;
  PreviewStatusType      status;
  GimpDrawable          *drawable;
  ImageBuffer           *img;
  CheckValues           *check;
  FblurPreviewNavigate  *navigate;
  FblurPreviewViewport  *viewport;
};


static gint8  dash_list[] = { 12, 4 };
const  gint   dash_offset_max = 16;

static GdkCursor *dragcursor, *crosshair;

#endif /* FBLUR_PREVIEW */

/*---- Prottypes ---*/

static void query       (void);

static void run         (const gchar             *name,
                         gint                     nparams,
                         const GimpParam         *param,
                         gint                    *nreturn_vals,
                         GimpParam              **return_vals);

static void             focus_blur_pixel        (COLORBUF       *d_buf,
                                                 ImageBuffer    *src,
                                                 guint           posx,
                                                 guint           posy,
                                                 DiffusionTable *dtable,
                                                 ModelTypes      model,
                                                 gint            focus,
                                                 DistanceMode    dist_mode,
                                                 MapBuffer      *map,
                                                 ShineBuffer    *shine,
                                                 gboolean        use_color,
                                                 gdouble         whiteglow);

static void             focus_blur              (GimpDrawable   *drawable);

static void		focus_blur_free		(void);

static void		focus_blur_anim		(GimpDrawable	*drawable);

static void             hour_to_coordinates     (gdouble        *coord_x,
                                                 gdouble        *coord_y,
                                                 gdouble         hour);

static DiffusionTable*  dtable_update           (gboolean        free);

static void             dtable_free             (DiffusionTable *dt);

static DiffusionTable*  dtable_new              (ModelTypes      model,
                                                 BrushData      *brush,
                                                 gdouble         radius,
                                                 gdouble         softness,
                                                 gdouble         fill,
                                                 gdouble         scale,
                                                 gdouble         diff_x,
                                                 gdouble         diff_y,
                                                 gint            ntables);

static void             make_dtable_sub         (DiffusionTable *dt,
                                                 gint            n,
                                                 gdouble         fval,
                                                 gdouble         fill);

static void             make_dtable_brush       (DiffusionTable *dt,
                                                 gint            n,
                                                 gdouble         fval);

static void             make_dtable_blur        (DiffusionTable *dt,
                                                 gint            n,
                                                 gdouble         fval);

static ImageBuffer*     fblur_update_srcimg     (GimpDrawable   *drawable);

static MapBuffer*       fblur_update_map        (gboolean        free);

static ShineBuffer*     fblur_update_shine      (ImageBuffer    *img);

static ImageBuffer*     img_allocate            (gint            bpp,
                                                 guint           width,
                                                 guint           height);

static void             img_free                (ImageBuffer    *img);

static ImageBuffer*     img_new                 (GimpDrawable   *drawable,
                                                 gint            x,
                                                 gint            y,
                                                 gint            width,
                                                 gint            height);

static MapBuffer*       map_new                 (gint32          map_id,
                                                 gint            focus,
                                                 gint            dmax,
                                                 gint            near);

static ShineBuffer*     shine_new               (ImageBuffer    *img,
                                                 gint            light,
                                                 gdouble         saturation,
                                                 gdouble         level,
                                                 gdouble         gamma);

static void		shine_erase_white	(ShineBuffer	*sp,
						 gint		 gate);

static void		shine_erase_white_find	(ShineBuffer	*sp,
						 gint8		*tp,
						 gint		*cp,
						 gdouble	 min,
						 gint		 x,
						 gint		 y);

static void		shine_erase_white_judge	(ShineBuffer	*sp,
						 gint8		*tp,
						 gint		 x,
						 gint		 y,
						 gint		 val);

static void		shine_erase_white_flush (ShineBuffer	*sp,
						 gint8		*tp);

static ShineBuffer*     shine_new_from_image    (ImageBuffer    *src,
                                                 gint32          shine_map_id);

static gint             shine_mapmenu_constraint (gint32         image_id,
                                                  gint32         drawable_id,
                                                  gpointer       data);

static GtkWidget*       shine_mapmenu_dialog_new (gint          *map_id,
                                                  GtkWindow     *parent,
                                                  ImageBuffer   *srcimg);

static void             shine_radio_button_update (GtkWidget    *widget,
                                                   gpointer      data);

static void             brush_free              (BrushData      *brush);

static BrushData*       get_brush_data          (const gchar    *name);

static void             model_combobox_append   (GtkListStore   *store,
                                                 GtkTreeIter    *iter,
                                                 gint            val,
                                                 gchar          *label,
                                                 const GdkPixdata *pixdata);

static void             change_brush_callback   (const gchar    *brush_name,
                                                 gdouble         opacity,
                                                 gint            spacing,
                                                 GimpLayerModeEffects paint_mode,
                                                 gint            width,
                                                 gint            height,
                                                 const guchar   *mask_data,
                                                 gboolean        dialog_closing,
                                                 gpointer        user_data);

static guint8*          scale_brush             (gdouble         scale,
                                                 BrushData      *brush);

static guint8*          scale_brush_color       (gdouble         scale,
                                                 BrushData      *brush);

static GdkPixdata*      make_brush_thumbnail    (BrushData      *brush,
                                                 gint            width,
                                                 gint            height,
                                                 gint            padding);

static GtkWidget*       notebook_append_vbox    (GtkNotebook    *notebook,
                                                 GtkWidget      *tab_label);

static gboolean         focus_blur_dialog       (GimpDrawable   *drawable);

static void             dialog_vals_init        (GtkButton       *button,
                                                 GtkWidget      **widgets);

static void             dialog_diff_reset       (GtkButton       *button,
                                                 GtkWidget      **widgets);

static void             dialog_level_reset      (GtkButton       *button,
                                                 GtkWidget      **widgets);

static void             dialog_vals_store       (GtkButton       *button,
                                                 GtkWidget       *entry);

static void             dialog_center_to_diff   (GtkButton       *button,
                                                 GtkWidget      **widgets);

static void             dialog_shine_as_new     (GtkButton       *button,
                                                 GimpDrawable    *src_d);

static gint32           shine_as_new            (GimpDrawable    *src_d);

static gint             mapmenu_constraint      (gint32           image_id,
                                                 gint32           drawable_id,
                                                 gpointer         data);

static void             model_combobox_callback (GtkComboBox     *combobox,
                                                  gpointer        user_data);

static void             report_button_update    (GtkToggleButton *toggle,
                                                 gpointer         user_data);


#if (FBLUR_PREVIEW == 1 || FBLUR_PREVIEW == 2)
static ImageBuffer*     img_dup_crop            (ImageBuffer    *img,
                                                 guint           x,
                                                 guint           y,
                                                 guint           width,
                                                 guint           height);

static void             focus_point             (guint32        map_id,
                                                 gint           x,
                                                 gint           y,
                                                 gboolean       invert);

#endif

#if (FBLUR_PREVIEW == 2)

static void             update_preview_free     (void);

static void             update_preview_draw     (gpointer        data);

static void             update_preview_progress (gpointer        data);

static void             update_preview_init     (GimpPreview    *preview,
                                                 GimpDrawable   *drawable);

static void             update_preview_event    (GtkWidget      *area,
                                                 GdkEvent       *event,
                                                 gpointer        data);
#endif

#if (FBLUR_PREVIEW == 1)

static ImageBuffer*     img_duplicate           (ImageBuffer    *img);

static ImageBuffer*     img_dup_noalpha         (ImageBuffer    *img,
                                                 CheckValues    *check);

static ImageBuffer*     make_inactive_img       (ImageBuffer    *img,
                                                 GtkWidget      *widget);

static guint     thumbnail_progress_update      (FblurPreviewNavigate *navi);

static void      thumbnail_progress_callback    (gpointer        data);

static void      preview_progress_draw          (gpointer        data);

static void      preview_progress_update        (gpointer        data);

static void              preview_progress_init  (FblurPreviewViewport *view);

static void              preview_progress_free  (FblurPreviewViewport *view);

static void      fblur_preview_invalidate       (FblurPreviewViewport *view);

static ImageBuffer*     make_thumbnail_img_raw  (ImageBuffer    *img,
                                                 guint           width,
                                                 guint           height);

static CheckValues*     check_values_new        (void);

static FblurPreviewNavigate* preview_navigate_new (guint         width,
                                                   guint         height);

static void             preview_navigate_free   (FblurPreviewNavigate *navi);

static FblurPreviewViewport* preview_viewport_new (guint         width,
                                                   guint         height);

static void             preview_viewport_free   (FblurPreviewViewport *view);

static void             preview_img_set         (FblurPreview   *preview,
                                                 ImageBuffer    *src);

static FblurPreview*    fblur_preview_new       (GimpDrawable   *drawable);

static void             fblur_preview_free      (FblurPreview   *preview);

static void             preview_draw_img_area   (GtkWidget      *widget,
                                                 ImageBuffer    *img,
                                                 GdkRectangle   *area);

static void     preview_draw_img_area_dbl       (GtkWidget      *widget,
                                                 ImageBuffer    *img,
                                                 GdkRectangle   *area);

static void             preview_dash_interval   (gpointer        data);

static void             preview_cursor_draw     (FblurPreviewNavigate *navi);

static gboolean         preview_cursor_set      (FblurPreviewNavigate *navi,
                                                 gint            new_x,
                                                 gint            new_y);

static void             preview_cursor_off      (FblurPreviewNavigate *navi);

static void             preview_cursor_update   (FblurPreviewViewport *view);

static void             preview_revert          (FblurPreviewViewport *view);

static void             preview_cursor_show     (FblurPreviewNavigate *navi);

static gboolean preview_navigate_event_callback (GtkWidget      *widget,
                                                 GdkEvent       *event,
                                                 gpointer        data);

static gboolean preview_viewport_event_callback (GtkWidget      *widget,
                                                 GdkEvent       *event,
                                                 gpointer        data);

static gboolean indicater_expose_callback       (GtkWidget      *widget,
                                                 GdkEventExpose *event,
                                                 gpointer        data);

static gboolean preview_dialog_event_hook       (GtkWidget      *widget,
                                                 GdkEvent       *event,
                                                 gpointer        data);

static void     preview_menu_revert_callback    (GtkMenuItem    *menuitem,
                                                 gpointer        user_data);

static void     preview_menu_render_callback    (GtkMenuItem    *menuitem,
                                                 gpointer        user_data);

static void     preview_menu_auto_callback (GtkCheckMenuItem    *checkmenuitem,
                                            gpointer             user_data);

static void     preview_menu_scroll_callback    (GtkMenuItem    *menuitem,
                                                 gpointer        user_data);

static void     preview_menu_pickup_callback    (GtkMenuItem    *menuitem,
                                                 gpointer        user_data);

static void     preview_menu_resize_callback (GtkCheckMenuItem  *checkmenuitem,
                                              gpointer           user_data);

static void     preview_viewport_resize         (FblurPreviewViewport *view,
                                                 gboolean        dblsize);

static void     preview_viewport_pickup         (FblurPreviewViewport   *view,
                                                 gint                    x,
                                                 gint                    y);

static void     preview_navigate_pickup         (FblurPreviewNavigate   *navi,
                                                 gint                    x,
                                                 gint                    y);

#endif


/*---- Variables ----*/

GimpPlugInInfo PLUG_IN_INFO = {
  NULL,                         /* init_proc  */
  NULL,                         /* quit_proc  */
  query,                        /* query_proc */
  run,                          /* run_proc   */
};

const FblurValues fblur_init_vals = {
  MODEL_FLAT,                   /* model of focus */
  5.0,                          /* maxmum radius for blurring */
  0.0,                          /* softness */
  0.0,                          /* filling inside */
  100.0,                        /* scale for brush */
  FALSE,                        /* use model adjustment */
  0.0,                          /* difference for shadow */
  0.0,                          /* directon for shadow */
  COLORMAX,                     /* focal distance */
  FALSE,                        /* use map */
  -1,                           /* distance map id */
  0,                            /* black means near */
  DWEIGHT_NONE,                 /* distance weight mode */
  FALSE,                        /* use shine */
  SHINE_INTENSITY,              /* shine mode */
  -1,                           /* shine map id */
  8,                            /* threshold of shine */
  20.0,                         /* percentage of saturation */
  50.0,                         /* percentage of shine */
  1.0,                          /* gamma for shine */
  FALSE,			/* erase white */
  1.0,				/* thresold for erasing */
  50.0,                         /* white glow color brush */
  FALSE,                        /* auto update preview */
  FALSE,			/* focus animation */
  0,				/* focus start point */
  COLORMAX,			/* focus end point */
  1.0,				/* curve for changing */
  200,				/* waiting time for one frame */
  5,				/* frame number of animation */
  REPORT_NONE,                  /* report processing time */
};
static FblurValues fblur_vals;

static GtkWidget *fblur_widgets[NUM_FBLUR_WIDGETS];

static BrushData *fblur_brush = NULL;

/* GdkPixbuf RGB C-Source image dump 1-byte-run-length-encoded */

static const GdkPixdata pixdata_flat = {
  0x47646b50,                   /* Pixbuf magic: 'GdkP' */
  24 + 384,                     /* header length + pixel_data length */
  0x2010001,                    /* pixdata_type */
  51,                           /* rowstride */
  17,                           /* width */
  17,                           /* height */
  /* pixel_data: */
  "\222\377\335\244\205\0\0\0\5\21\21\21FFFYYYFFF\21\21\21\205\0\0\0\202"
    "\377\335\244\203\0\0\0\3\37\37\37\247\247\247\372\372\372\203\377\377"
    "\377\3\372\372\372\247\247\247\37\37\37\203\0\0\0\202\377\335\244\202"
    "\0\0\0\2<<<\356\356\356\207\377\377\377\2\356\356\356<<<\202\0\0\0\202"
    "\377\335\244\3\0\0\0\37\37\37\356\356\356\211\377\377\377\3\356\356\356"
    "\37\37\37\0\0\0\202\377\335\244\2\0\0\0\247\247\247\213\377\377\377\2"
    "\247\247\247\0\0\0\202\377\335\244\2\21\21\21\372\372\372\213\377\377"
    "\377\2\372\372\372\21\21\21\202\377\335\244\1FFF\215\377\377\377\1FF"
    "F\202\377\335\244\1YYY\215\377\377\377\1YYY\202\377\335\244\1FFF\215"
    "\377\377\377\1FFF\202\377\335\244\2\21\21\21\372\372\372\213\377\377"
    "\377\2\372\372\372\21\21\21\202\377\335\244\2\0\0\0\247\247\247\213\377"
    "\377\377\2\247\247\247\0\0\0\202\377\335\244\3\0\0\0\37\37\37\356\356"
    "\356\211\377\377\377\3\356\356\356\37\37\37\0\0\0\202\377\335\244\202"
    "\0\0\0\2<<<\356\356\356\207\377\377\377\2\356\356\356<<<\202\0\0\0\202"
    "\377\335\244\203\0\0\0\3\37\37\37\247\247\247\372\372\372\203\377\377"
    "\377\3\372\372\372\247\247\247\37\37\37\203\0\0\0\202\377\335\244\205"
    "\0\0\0\5\21\21\21FFFYYYFFF\21\21\21\205\0\0\0\222\377\335\244",
};

static const GdkPixdata pixdata_ring = {
  0x47646b50,                   /* Pixbuf magic: 'GdkP' */
  24 + 508,                     /* header length + pixel_data length */
  0x2010001,                    /* pixdata_type */
  51,                           /* rowstride */
  17,                           /* width */
  17,                           /* height */
  /* pixel_data: */
  "\222\377\335\244\205\0\0\0\5\21\21\21FFFYYYFFF\21\21\21\205\0\0\0\202"
    "\377\335\244\203\0\0\0\3\37\37\37\247\247\247\372\372\372\203\377\377"
    "\377\3\372\372\372\247\247\247\37\37\37\203\0\0\0\202\377\335\244\202"
    "\0\0\0\1<<<\202\356\356\356\5\202\202\202777\37\37\37" "777\202\202\202"
    "\202\356\356\356\1<<<\202\0\0\0\202\377\335\244\5\0\0\0\37\37\37\356"
    "\356\356\320\320\320\33\33\33\205\0\0\0\5\33\33\33\320\320\320\356\356"
    "\356\37\37\37\0\0\0\202\377\335\244\4\0\0\0\247\247\247\356\356\356\33"
    "\33\33\207\0\0\0\4\33\33\33\356\356\356\247\247\247\0\0\0\202\377\335"
    "\244\3\21\21\21\372\372\372\202\202\202\211\0\0\0\3\202\202\202\372\372"
    "\372\21\21\21\202\377\335\244\3FFF\377\377\377777\211\0\0\0\3" "777\377"
    "\377\377FFF\202\377\335\244\3YYY\377\377\377\37\37\37\211\0\0\0\3\37"
    "\37\37\377\377\377YYY\202\377\335\244\3FFF\377\377\377777\211\0\0\0\3"
    "777\377\377\377FFF\202\377\335\244\3\21\21\21\372\372\372\202\202\202"
    "\211\0\0\0\3\202\202\202\372\372\372\21\21\21\202\377\335\244\4\0\0\0"
    "\247\247\247\356\356\356\33\33\33\207\0\0\0\4\33\33\33\356\356\356\247"
    "\247\247\0\0\0\202\377\335\244\5\0\0\0\37\37\37\356\356\356\320\320\320"
    "\33\33\33\205\0\0\0\5\33\33\33\320\320\320\356\356\356\37\37\37\0\0\0"
    "\202\377\335\244\202\0\0\0\1<<<\202\356\356\356\5\202\202\202777\37\37"
    "\37" "777\202\202\202\202\356\356\356\1<<<\202\0\0\0\202\377\335\244\203"
    "\0\0\0\3\37\37\37\247\247\247\372\372\372\203\377\377\377\3\372\372\372"
    "\247\247\247\37\37\37\203\0\0\0\202\377\335\244\205\0\0\0\5\21\21\21"
    "FFFYYYFFF\21\21\21\205\0\0\0\222\377\335\244",
};

static const GdkPixdata pixdata_concave = {
  0x47646b50,                   /* Pixbuf magic: 'GdkP' */
  24 + 678,                     /* header length + pixel_data length */
  0x2010001,                    /* pixdata_type */
  51,                           /* rowstride */
  17,                           /* width */
  17,                           /* height */
  /* pixel_data: */
  "\222\377\335\244\205\0\0\0\5\21\21\21FFFYYYFFF\21\21\21\205\0\0\0\202"
    "\377\335\244\203\0\0\0\11\37\37\37\246\246\246\364\364\364\363\363\363"
    "\360\360\360\363\363\363\364\364\364\246\246\246\37\37\37\203\0\0\0\202"
    "\377\335\244\202\0\0\0\1<<<\202\352\352\352\5\330\330\330\314\314\314"
    "\310\310\310\314\314\314\330\330\330\202\352\352\352\1<<<\202\0\0\0\202"
    "\377\335\244\17\0\0\0\37\37\37\352\352\352\343\343\343\310\310\310\263"
    "\263\263\245\245\245\241\241\241\245\245\245\263\263\263\310\310\310"
    "\343\343\343\352\352\352\37\37\37\0\0\0\202\377\335\244\17\0\0\0\246"
    "\246\246\352\352\352\310\310\310\252\252\252\221\221\221\177\177\177"
    "yyy\177\177\177\221\221\221\252\252\252\310\310\310\352\352\352\246\246"
    "\246\0\0\0\202\377\335\244\17\21\21\21\364\364\364\330\330\330\263\263"
    "\263\221\221\221rrrZZZQQQZZZrrr\221\221\221\263\263\263\330\330\330\364"
    "\364\364\21\21\21\202\377\335\244\17FFF\363\363\363\314\314\314\245\245"
    "\245\177\177\177ZZZ:::***:::ZZZ\177\177\177\245\245\245\314\314\314\363"
    "\363\363FFF\202\377\335\244\17YYY\360\360\360\310\310\310\241\241\241"
    "yyyQQQ***\17\17\17***QQQyyy\241\241\241\311\311\311\360\360\360YYY\202"
    "\377\335\244\17FFF\363\363\363\314\314\314\245\245\245\177\177\177ZZ"
    "Z:::***:::ZZZ\177\177\177\245\245\245\314\314\314\363\363\363FFF\202"
    "\377\335\244\17\21\21\21\364\364\364\330\330\330\263\263\263\221\221"
    "\221rrrZZZQQQZZZrrr\221\221\221\263\263\263\330\330\330\364\364\364\21"
    "\21\21\202\377\335\244\17\0\0\0\246\246\246\352\352\352\311\311\311\252"
    "\252\252\221\221\221\177\177\177yyy\177\177\177\221\221\221\252\252\252"
    "\311\311\311\352\352\352\246\246\246\0\0\0\202\377\335\244\17\0\0\0\37"
    "\37\37\352\352\352\343\343\343\310\310\310\263\263\263\245\245\245\241"
    "\241\241\245\245\245\263\263\263\310\310\310\343\343\343\352\352\352"
    "\37\37\37\0\0\0\202\377\335\244\202\0\0\0\1<<<\202\352\352\352\5\330"
    "\330\330\314\314\314\310\310\310\314\314\314\330\330\330\202\352\352"
    "\352\1<<<\202\0\0\0\202\377\335\244\203\0\0\0\11\37\37\37\246\246\246"
    "\364\364\364\363\363\363\360\360\360\363\363\363\364\364\364\246\246"
    "\246\37\37\37\203\0\0\0\202\377\335\244\205\0\0\0\5\21\21\21FFFYYYFF"
    "F\21\21\21\205\0\0\0\222\377\335\244",
};

static const GdkPixdata pixdata_gauss = {
  0x47646b50,                   /* Pixbuf magic: 'GdkP' */
  24 + 722,                     /* header length + pixel_data length */
  0x2010001,                    /* pixdata_type */
  51,                           /* rowstride */
  17,                           /* width */
  17,                           /* height */
  /* pixel_data: */
  "\222\377\335\244\203\0\0\0\2\1\1\1\2\2\2\202\3\3\3\1\4\4\4\202\3\3\3"
    "\2\2\2\2\1\1\1\203\0\0\0\202\377\335\244\202\0\0\0\13\1\1\1\3\3\3\5\5"
    "\5\10\10\10\12\12\12\13\13\13\12\12\12\10\10\10\5\5\5\3\3\3\1\1\1\202"
    "\0\0\0\202\377\335\244\17\0\0\0\1\1\1\3\3\3\7\7\7\15\15\15\25\25\25\33"
    "\33\33\35\35\35\33\33\33\25\25\25\15\15\15\7\7\7\3\3\3\1\1\1\0\0\0\202"
    "\377\335\244\17\1\1\1\3\3\3\7\7\7\20\20\20\35\35\35---:::@@@:::---\35"
    "\35\35\20\20\20\7\7\7\3\3\3\1\1\1\202\377\335\244\17\2\2\2\5\5\5\15\15"
    "\15\35\35\35" "666SSSkkkuuukkkSSS666\35\35\35\15\15\15\5\5\5\2\2\2\202"
    "\377\335\244\17\3\3\3\10\10\10\25\25\25---SSS\177\177\177\245\245\245"
    "\264\264\264\245\245\245\177\177\177SSS---\25\25\25\10\10\10\3\3\3\202"
    "\377\335\244\17\3\3\3\12\12\12\33\33\33:::kkk\245\245\245\326\326\326"
    "\352\352\352\326\326\326\245\245\245kkk:::\33\33\33\12\12\12\3\3\3\202"
    "\377\335\244\17\4\4\4\13\13\13\35\35\35@@@uuu\264\264\264\352\352\352"
    "\377\377\377\352\352\352\264\264\264uuu@@@\35\35\35\13\13\13\4\4\4\202"
    "\377\335\244\17\3\3\3\12\12\12\33\33\33:::kkk\245\245\245\326\326\326"
    "\352\352\352\326\326\326\245\245\245kkk:::\33\33\33\12\12\12\3\3\3\202"
    "\377\335\244\17\3\3\3\10\10\10\25\25\25---SSS\177\177\177\245\245\245"
    "\264\264\264\245\245\245\177\177\177SSS---\25\25\25\10\10\10\3\3\3\202"
    "\377\335\244\17\2\2\2\5\5\5\15\15\15\35\35\35" "666SSSkkkuuukkkSSS666"
    "\35\35\35\15\15\15\5\5\5\2\2\2\202\377\335\244\17\1\1\1\3\3\3\7\7\7\20"
    "\20\20\35\35\35---:::@@@:::---\35\35\35\20\20\20\7\7\7\3\3\3\1\1\1\202"
    "\377\335\244\17\0\0\0\1\1\1\3\3\3\7\7\7\15\15\15\25\25\25\33\33\33\35"
    "\35\35\33\33\33\25\25\25\15\15\15\7\7\7\3\3\3\1\1\1\0\0\0\202\377\335"
    "\244\202\0\0\0\13\1\1\1\3\3\3\5\5\5\10\10\10\12\12\12\13\13\13\12\12"
    "\12\10\10\10\5\5\5\3\3\3\1\1\1\202\0\0\0\202\377\335\244\203\0\0\0\2"
    "\1\1\1\2\2\2\202\3\3\3\1\4\4\4\202\3\3\3\2\2\2\2\1\1\1\203\0\0\0\222"
    "\377\335\244",
};


/* inline funcs */

G_INLINE_FUNC COLORBUF*
img_get_p (ImageBuffer    *img,
           gint            x,
           gint            y)
{
  gsize offset;
  x -= img->x;
  y -= img->y;
  g_assert (x >= 0 && x < img->w && y >= 0 && y < img->h);

  offset = img->rowstride * y + img->bpp * x;
  g_assert (offset < img->length);

  return &(img->data[offset]);
}


G_INLINE_FUNC gint
map_get (MapBuffer *map,
         gint       x,
         gint       y)
{
  gint d;

  g_assert (map);

  /* tile */
  while (x >= map->width)
    x -= map->width;
  while (x < 0)
    x += map->width;

  while (y >= map->height)
    y -= map->height;
  while (y < 0)
    y += map->height;

  d = map->data[y * map->width + x];

  return d;
}


G_INLINE_FUNC gdouble
shine_get (ShineBuffer *sp,
           gint         x,
           gint         y)
{
  x -= sp->x;
  y -= sp->y;
  g_assert (x >= 0 && x < sp->w && y >= 0 && y < sp->h);

  return sp->data[y * sp->w + x];
}

static void
brush_free (BrushData *brush)
{
  if (! brush)
    return;

  if (brush->name)
    g_free (brush->name);

  g_free (brush);
}


/*---- Spell of GIMP ---*/

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] = {
    {GIMP_PDB_INT32,    "run_mode",   "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE,    "image",      "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable",   "Input drawable"},
    {GIMP_PDB_INT32,    "model",      "Model of blurring: (Flat(0), Ring(1), Concave(2), Gauss(3), Brush(4))"},
    {GIMP_PDB_FLOAT,    "radius",     "Radius for blurring: (0.0 < radius))"},
    {GIMP_PDB_FLOAT,    "softness",   "Softness level for all models: (0.0 <= softness <= 100))"},
    {GIMP_PDB_FLOAT,    "fill",       "Filling level for Ring and Concave: (0.0 <= fill <= 100))"},
    {GIMP_PDB_FLOAT,    "bscale",      "Scale factor for Brush: (0.0 < scale <= 100))"},
    {GIMP_PDB_FLOAT,    "radius",     "Radius for blurring: (0.0 < radius))"},
    {GIMP_PDB_FLOAT,    "diff",        "Difference for shadowing: (percentage of radius)"},
    {GIMP_PDB_FLOAT,    "direction",  "Direction for shadowing: (angle by hour 0..12)"},
    {GIMP_PDB_INT32,    "focus",      "Focal distance: (0 <= focus <= 255)"},
    {GIMP_PDB_INT32,    "map_id",     "Distance map drawable id: (drawable_ID or -1 unused)"},
    {GIMP_PDB_INT32,    "dist_mode",  "Distance weight mode: None(0), Obstruction(1), Depth order(2)"},
    {GIMP_PDB_INT32,    "light",      "Threshold of light: (1 <= light <= 255 or light < 1 unused)"},
    {GIMP_PDB_FLOAT,    "saturation", "Percentage of saturation: (0 <= saturation <= 100 in % or saturation < 0 unused)"},
    {GIMP_PDB_FLOAT,    "level",      "Percentage of shine: (0 < level <= 100 in % or level <= 0 unused)"},
    {GIMP_PDB_FLOAT,    "curve",      "Curve of shine: (0 <= curve, curve == 1 direct proportion)"},
    {GIMP_PDB_FLOAT,    "erase",      "erase white: (0 == disabled, 0 < erase < 10)"},
    {GIMP_PDB_FLOAT,    "whiteglow",  "White glowing level for color brush: (0 <= whiteglow <= 100 in %)"},
  };

  gimp_install_procedure (PLUG_IN_NAME,
                          "Make a out of focus",
                          "This plug-in is a kind of blur. It is albe to show a out of focus. this blur has four MODELS that is used for diffusion of color. You might choose also BRUSH instead of these models. You can specify a DISTANCE MAP. It makes distance for each pixels in source image. A amount of blur is decided by RADIUS and difference of two values. They are distance of pixel and FOCUS. You could specify threshold of LIGHT. It would make a scene has natural shine as you have seen.",
                          "Kyoichiro Suda <sudakyo\100fat.coara.or.jp>",
                          "Kyoichiro Suda", "2002-2006",
                          N_("_Focus Blur..."),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_menu_register (PLUG_IN_NAME, "<Image>/Filters/Blur");
}

static void
run (const gchar         *name,
     gint                 nparams,
     const GimpParam     *param,
     gint                *nreturn_vals,
     GimpParam          **return_vals)
{
  static GimpParam values[1];
  GimpDrawable *drawable;
  GimpRunMode run_mode = param[0].data.d_int32;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /* get image and drawable */
  drawable = gimp_drawable_get (param[2].data.d_drawable);

#ifdef INIT_I18N
  INIT_I18N ();
#endif

  fblur_vals = fblur_init_vals;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data */
      if (gimp_get_data_size (PLUG_IN_NAME) == sizeof (FblurValues))
        gimp_get_data (PLUG_IN_NAME, &fblur_vals);

      /* Get information from the dialog */
      if (! focus_blur_dialog (drawable))
        return;

      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* Make sure all the arguments are present */

      if (nparams == 19)
        {
          fblur_vals.model      = CLAMP (param[3].data.d_int32,
                                         MODEL_FLAT, MODEL_BRUSH);
          fblur_vals.radius     = CLAMP (param[4].data.d_float, 0, 999);
          fblur_vals.softness   = CLAMP (param[5].data.d_float, 0, 100);
          fblur_vals.fill       = CLAMP (param[6].data.d_float, 0, 100);
          fblur_vals.bscale     = CLAMP (param[7].data.d_float, 0, 100);
          fblur_vals.diff       = CLAMP (param[8].data.d_float, -200, 200);
          if (fblur_vals.diff)
            fblur_vals.use_modeladj = TRUE;
          fblur_vals.direction  = param[9].data.d_float;
          fblur_vals.focus      = CLAMP (param[10].data.d_int32, 0, COLORMAX);
          fblur_vals.use_map    = (param[11].data.d_int32 != -1) ? TRUE : FALSE;
          fblur_vals.map_id     = param[11].data.d_int32;
          fblur_vals.near       = 0; /* always FALSE */
          fblur_vals.dist_mode  = CLAMP (param[12].data.d_int32,
                                         DWEIGHT_NONE, DWEIGHT_DEPTH_ORDER);
          fblur_vals.use_shine  = (param[13].data.d_int32 > 0) ? TRUE : FALSE;
          fblur_vals.shine_mode = (param[14].data.d_int32 < 0) ? SHINE_INTENSITY : SHINE_HSV;
          fblur_vals.light      = CLAMP (param[13].data.d_int32, 0, COLORMAX * 2);
          fblur_vals.saturation  = CLAMP (param[14].data.d_float, 0.0, 200.0);
          fblur_vals.shine_level = CLAMP (param[15].data.d_float, 0.0, 200.0);
          fblur_vals.shine_curve = CLAMP (param[16].data.d_float, 0.0, 10.0);
	  fblur_vals.erase_white = (param[17].data.d_float >   0.0 &&
				    param[17].data.d_float <= 10.0
				    ) ? TRUE : FALSE;
	  fblur_vals.erase_white_val
				 = CLAMP (param[17].data.d_float, 0.0, 10.0);
          fblur_vals.whiteglow   = CLAMP (param[18].data.d_float, 0.0, 100.0);
        }
      else
        status = GIMP_PDB_CALLING_ERROR;
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data */
      if (gimp_get_data_size (PLUG_IN_NAME) != sizeof (FblurValues) ||
          ! gimp_get_data (PLUG_IN_NAME, &fblur_vals))
        status = GIMP_PDB_CALLING_ERROR;
      break;

    default:
      break;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      if (gimp_drawable_is_rgb (drawable->drawable_id) ||
           gimp_drawable_is_gray (drawable->drawable_id))
        {

	  /* with focus animation */
	  if (fblur_vals.focus_anim &&
	      fblur_vals.focus_div > 1 &&
	      fblur_vals.focus_start != fblur_vals.focus_end)

	    focus_blur_anim (drawable);

	  else

	    /* Run! */
	    focus_blur (drawable);

	  focus_blur_free ();

          /* If run mode is interactive, flush displays */
          if (run_mode != GIMP_RUN_NONINTERACTIVE)
            gimp_displays_flush ();

          /* forget shine map */
          if (fblur_vals.shine_mode == SHINE_MAP)
            fblur_vals.shine_mode = fblur_init_vals.shine_mode;
          /* Store data */
          if (run_mode == GIMP_RUN_INTERACTIVE)
            gimp_set_data (PLUG_IN_NAME, &fblur_vals, sizeof (FblurValues));
        }
      else
        status = GIMP_PDB_EXECUTION_ERROR;
    }
  else
    status = GIMP_PDB_EXECUTION_ERROR;

  values[0].data.d_status = status;
  gimp_drawable_detach (drawable);
}


/*---- Functions ---*/

static void
focus_blur_pixel (COLORBUF       *d_buf,
                  ImageBuffer    *src,
                  guint           posx,
                  guint           posy,
                  DiffusionTable *dtable,
                  ModelTypes      model,
                  gint            focus,
                  DistanceMode    dist_mode,
                  MapBuffer      *map,
                  ShineBuffer    *shine,
                  gboolean        use_color,
                  gdouble         whiteglow)
{
  COLORBUF      *src_bp;

  gdouble       *value[4] = { NULL, NULL, NULL, NULL };
  gdouble        density[4] = { 0, 0, 0, 0 };
  gdouble        fval  = 0;
  gdouble        alpha = 1;
  gdouble        light = 0;
  gdouble        color[4] = { 1, 1, 1, 0 };

  gint           reso = (dtable) ? dtable->ntables : 1;
  gint           depth = (dist_mode == DWEIGHT_DEPTH_ORDER) ? reso : 1;
  gint           d, dist, idx;
  gint           pix_dist  = 0;
  gdouble        pix_trans = 0;
  gdouble        a_sum[depth], p_sum[depth], c_sum[depth][4], trans[depth];
  gdouble        d_a_sum, d_p_sum, d_c_sum[4];
  gdouble        hit, hitd, hitx, hita;
  gint           x1, x2, y1, y2, x, y, c;
  gint           offset, pad, tmp;

  g_assert (dist_mode == DWEIGHT_NONE || map);
  if (dist_mode == DWEIGHT_OBSTRUCTION)
    {
      pix_dist = map_get (map, posx, posy);

      gint    idx = abs (pix_dist - focus);
      gint    center = - dtable->y_min * dtable->width - dtable->x_min;
      gdouble v = dtable->table[idx].value[0][center];
      gdouble d = dtable->table[idx].density[0];
      gdouble op = v / d;
      if (op < 0.0000001)
        dist_mode = DWEIGHT_NONE;
      pix_trans = 1.0 - op;
    }
  else if (dist_mode == DWEIGHT_DEPTH_ORDER)
    {
      for (d = 0; d < depth; d ++)
        a_sum[d] = 0;
      for (d = 0; d < depth; d ++)
        p_sum[d] = 0;
      for (d = 0; d < depth; d ++)
	trans[d] = 0;
      for (d = 0; d < depth; d ++)
        c_sum[d][0] = c_sum[d][1] = c_sum[d][2] = c_sum[d][3] = 0;

      pix_dist = map_get (map, posx, posy);
    }

  d = 0;
  dist = idx = 0;
  a_sum[0] = p_sum[0] = 0;
  trans[d] = 0;
  c_sum[0][0] = c_sum[0][1] = c_sum[0][2] = c_sum[0][3] = 0;

  if (! map)
    {
      fval = (gdouble) focus / COLORMAX;
      value[0]   = dtable->table[0].value[0];
      value[1]   = dtable->table[0].value[1];
      value[2]   = dtable->table[0].value[2];
      value[3]   = dtable->table[0].value[3];
      density[0] = dtable->table[0].density[0];
    }

  x1 = posx + dtable->x_min;
  x2 = posx + dtable->x_max;
  y1 = posy + dtable->y_min;
  y2 = posy + dtable->y_max;

  offset = 0;
  pad    = 0;
  tmp = (gint) src->x - x1;
  if (tmp > 0)
    {
      offset += tmp;
      pad    += tmp;
      x1     += tmp;
    }

  tmp = x2 - ((gint) (src->x + src->w) - 1);
  if (tmp > 0)
    {
      pad += tmp;
      x2  -= tmp;
    }

  tmp = (gint) src->y - y1;
  if (tmp > 0)
    {
      offset += tmp * dtable->width;
      y1     += tmp;
    }

  tmp = (gint) (src->y + src->h) - 1;
  if (tmp < y2)
    y2 = tmp;

  tmp = offset; /* backup */

  for (y = y1; y <= y2; y ++, offset += pad)
    {
      for (x = x1; x <= x2; x ++, offset ++)
        {
          src_bp = img_get_p (src, x, y);

          if (map)
            {
              dist = map_get (map, x, y);

              idx = abs (dist - focus);
              fval = (gdouble) idx / COLORMAX;
              value[0]   = dtable->table[idx].value[0];
              value[1]   = dtable->table[idx].value[1];
              value[2]   = dtable->table[idx].value[2];
              value[3]   = dtable->table[idx].value[3];
              density[0] = dtable->table[idx].density[0];

              if (dist_mode == DWEIGHT_OBSTRUCTION)
                {
                  if (dist < pix_dist)
                    {
                      if (pix_trans < 0.0000001)
                        continue;
                      density[0] /= pix_trans;
                    }
                }
              else if (dist_mode == DWEIGHT_DEPTH_ORDER)
                {
                  d = dist;
                }
            }

          g_assert (offset < dtable->nelements);
          hit = value[0][offset];

          if (hit <= 0)
            continue;
          hitd = hit / density[0];

          if (use_color)
            {
              color[0] = value[1][offset];
              color[1] = value[2][offset];
              color[2] = value[3][offset];
            }

          hitx = hitd;
          if (shine)
            {
              light = shine_get (shine, x, y);
              if (light > 0)
                hitx = light * hit + (1 - light) * hitx;
            }

          hita = hitx;
          if (src->has_alpha)
            alpha = (gdouble) src_bp[src->ch] / COLORMAX;
          hita *= alpha;

          if (use_color)
            {
              for (c = 0; c < src->ch; c ++)
                {
                  c_sum[d][c] += alpha * src_bp[c] *
                    (light * hit * (fval * (color[c] + whiteglow) + 1)
                     + (1 - light) * hitd);
                }
            }
          else
            {
              for (c = 0; c < src->ch; c ++)
                c_sum[d][c] += hita * src_bp[c];
            }
          a_sum[d] += hita;
          p_sum[d] += hitx;
	  trans[d] += hitd;
        }
    }

  if (dist_mode == DWEIGHT_DEPTH_ORDER)
    {
      gdouble d_trans = 1;

      d_a_sum = d_p_sum = d_c_sum[0] =
        d_c_sum[1] = d_c_sum[2] = d_c_sum[3] = 0;

      d = depth;
      while (d --)
        {
	  if (trans[d] > 0.0)
	    {
	      d_a_sum    += d_trans * a_sum[d];
	      d_p_sum    += d_trans * p_sum[d];
	      d_c_sum[0] += d_trans * c_sum[d][0];
	      d_c_sum[1] += d_trans * c_sum[d][1];
	      d_c_sum[2] += d_trans * c_sum[d][2];
	      d_c_sum[3] += d_trans * c_sum[d][3];

	      if (trans[d] < 1.0)
		{
		  d_trans  *= (1.0 - trans[d]);
		  if (d_trans < 0.0000001)
		    break;
		}
	      else
		{
		  d_trans = 0;
		  break;
		}
	    }
        }

      /* fill behind */
      if (d_p_sum < 1.0)
        {
          gdouble *b_value;
          gdouble  b_density;
          gdouble  b_a_sum = 0, b_p_sum = 0;
          gdouble  b_c_sum[4] = { 0, 0, 0, 0 };
	  gint     max_dist, faz_dist;

          pix_dist = map_get (map, posx, posy);
	  max_dist = pix_dist;
	  faz_dist = 0;

          idx = abs (pix_dist - focus);
          fval = (gdouble) idx / COLORMAX;
          b_value   = dtable->table[idx].value[0];
          b_density = dtable->table[idx].density[0];

	  /* fuzzy distance blending */
          offset = tmp;
          for (y = y1; y <= y2; y ++, offset += pad)
	    for (x = x1; x <= x2; x ++, offset ++)
	      {
		if (b_value[offset] <= 0)
		  continue;
		dist = map_get (map, x, y);
		if (dist > max_dist)
		  max_dist = dist;
	      }
	  max_dist += 1;
	  faz_dist = max_dist - pix_dist;

          offset = tmp;
          for (y = y1; y <= y2; y ++, offset += pad)
	    for (x = x1; x <= x2; x ++, offset ++)
	      {
		hit = b_value[offset];
		if (hit <= 0)
		  continue;

		dist = map_get (map, x, y);
		if (dist > pix_dist)
		  /*
		  {
		    if (faz_dist)
		  */
		      hit *= (gdouble) (max_dist - dist) / faz_dist;
		/*
		    else
		      continue;
		  }
		*/

		if (hit <= 0)
		  continue;

		hitd = hit / b_density;
		hita = hitd;

		src_bp = img_get_p (src, x, y);

		if (src->has_alpha)
		  alpha = (gdouble) src_bp[src->ch] / COLORMAX;

		hita *= alpha;

		for (c = 0; c < src->ch; c ++)
		  b_c_sum[c] += hita * src_bp[c];

		b_a_sum += hita;
		b_p_sum += hitd;
	      }

          if (b_p_sum > 0.0)
            {
              gdouble val;

	      val = 1.0 - d_p_sum;
	      //val /= b_p_sum; too strong

              d_a_sum    += val * b_a_sum;
              d_p_sum    += val * b_p_sum;
              d_c_sum[0] += val * b_c_sum[0];
              d_c_sum[1] += val * b_c_sum[1];
              d_c_sum[2] += val * b_c_sum[2];
              d_c_sum[3] += val * b_c_sum[3];
            }
        }
    }
  else
    {
      d_a_sum    = a_sum[0];
      d_p_sum    = p_sum[0];
      d_c_sum[0] = c_sum[0][0];
      d_c_sum[1] = c_sum[0][1];
      d_c_sum[2] = c_sum[0][2];
      d_c_sum[3] = c_sum[0][3];
    }

  if (d_a_sum >= (1.0 / COLORMAX))
    {
      gdouble val;
      for (c = 0; c < src->ch; c ++)
        {
          val = d_c_sum[c] / d_a_sum;
          val = CLAMP (val, 0, COLORMAX);
          d_buf[c] = RINT (val);
        }
      if (src->has_alpha)
        {
          val = d_a_sum / d_p_sum * COLORMAX;
          d_buf[src->ch] = RINT (val);
        }
    }
  else
    for (c = 0; c < src->bpp; c ++)
      d_buf[c] = 0;
}


static void
focus_blur (GimpDrawable *drawable)
{
  if ((fblur_vals.model != MODEL_BRUSH && fblur_vals.radius <= 0.0) ||
      (fblur_vals.model == MODEL_BRUSH && fblur_vals.bscale <= 0.0) ||
      ((! fblur_vals.use_map) && fblur_vals.focus <= 0))
    return;

  GTimeVal time0, time1;
  clock_t clocks = 0;
  if (fblur_vals.report != REPORT_NONE)
    {
      g_get_current_time (&time0);
      clocks = clock ();
    }

  GimpPixelRgn   dest_pr;
  gpointer       reg_pr;
  COLORBUF       buf[drawable->bpp * TILE_WIDTH];
  COLORBUF      *bp, *dp;
  gdouble        progress, full_progress;
  gint           x, y, len;

  DiffusionTable *dtable    = dtable_update (FALSE);
  ModelTypes      model     = fblur_vals.model;
  ImageBuffer    *src       = fblur_update_srcimg (drawable);
  MapBuffer      *map       = fblur_update_map (FALSE);
  gint            focus     = fblur_vals.focus;
  DistanceMode    dist_mode = (map) ? fblur_vals.dist_mode : DWEIGHT_NONE;
  ShineBuffer    *shine     = fblur_update_shine (src);
  gboolean        use_color = (src->is_rgb &&
                               model == MODEL_BRUSH &&
                               dtable->color && shine);
  gdouble         whiteglow = (fblur_vals.whiteglow / 100) - 1.0;

  gimp_tile_cache_ntiles ((drawable->width + TILE_WIDTH - 1) / TILE_WIDTH);
  gimp_pixel_rgn_init (&dest_pr, drawable,
                       src->x, src->y, src->w, src->h, TRUE, TRUE);

  progress = 0;
  full_progress = src->w * src->h;
  gimp_progress_init (_("Focus Blur..."));
  gimp_progress_update (0.0001);

  for (reg_pr = gimp_pixel_rgns_register (1, &dest_pr);
       reg_pr != NULL; reg_pr = gimp_pixel_rgns_process (reg_pr))
    {
      dp = dest_pr.data;
      g_assert (dp);
      for (y = dest_pr.y; y < dest_pr.y + dest_pr.h; y ++)
        {
          bp = buf;
          len = dest_pr.w * dest_pr.bpp;
          for (x = dest_pr.x; x < dest_pr.x + dest_pr.w; x ++)
            {
              focus_blur_pixel (bp, src, x, y, dtable, model,
                                focus, dist_mode, map,
                                shine, use_color, whiteglow);
              bp += src->bpp;
            }
          memcpy (dp, buf, len);
          dp += dest_pr.rowstride;
        }
      progress += (dest_pr.w * dest_pr.h);
      gimp_progress_update (progress / full_progress);
    }

  /* update the blurred region */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id,
                        src->x, src->y, src->w, src->h);

  /* report processing time */
  if (fblur_vals.report != REPORT_NONE)
    {
      clocks = clock () - clocks;
      g_get_current_time (&time1);
      g_time_val_add (&time1, -time0.tv_usec);
      time1.tv_sec -= time0.tv_sec;
      gint tr_m, tr_s, tr_ms, tp_m, tp_s, tp_ms;
      tr_m = time1.tv_sec / 60;
      tr_s = time1.tv_sec % 60;
      tr_ms = time1.tv_usec / 1000;
      tp_m = clocks / (CLOCKS_PER_SEC * 60);
      tp_s = (clocks / CLOCKS_PER_SEC) % 60;
      tp_ms = (1000 * clocks / CLOCKS_PER_SEC) % 1000;

      gdouble fval = (gdouble) focus / COLORMAX;
      gchar *text = g_strdup_printf
        (_("Focus Blur Performance Reports\n"
           "  Area: \t%d x %d, %s Distance map.\n"
           "  Model:\t%s, radius = %.2f, %s Shine efx.\n"
           "  Time [Real]:\t%2dm %2ds %3dms\n"
           "  Time [Proc]:\t%2dm %2ds %3dms"),
         src->w, src->h, map ? _("with") : _("without"),
         (model == MODEL_FLAT) ? _("Flat") :
         (model == MODEL_RING) ? _("Ring") :
         (model == MODEL_CONCAVE) ? _("Concave") :
         (model == MODEL_GAUSS) ? _("Gauss") :
         (model == MODEL_BRUSH) ? _("Brush") : "??????",
         ((model != MODEL_BRUSH) ? fblur_vals.radius :
          (gdouble) (MAX (fblur_brush->width, fblur_brush->height) -
                     1) / 2) * fval,
         shine ? _("with") : _("without"),
         tr_m, tr_s, tr_ms, tp_m, tp_s, tp_ms);

      if (fblur_vals.report == REPORT_TIME_CON)
        g_printf ("%s\n", text);
      else if (fblur_vals.report == REPORT_TIME_WIN)
        gimp_message (text);

      g_free (text);
    }

  return;
}


static void
focus_blur_free ()
{
  fblur_update_srcimg (NULL);
  dtable_update (TRUE);
  fblur_update_map (TRUE);
  fblur_update_shine (NULL);
  brush_free (fblur_brush);
  fblur_brush = NULL;
}


static void
focus_blur_anim (GimpDrawable *drawable)
{
  regex_t	 regex;
  regmatch_t	 match[1];
  gchar		*str;
  gint		 max_len, len;
  const gchar	 num_str[] = "[ \t]*#[0-9]+\0";

  gdouble	 fval;
  gdouble	 ig;
  gint32	 image_ID;
  gint32	 drawable_ID;
  gint32	 layer_ID;
  GimpDrawable	*new_d;
  gchar		*orig_name;
  gchar		*layer_name;
  gint32	*layers;
  gint		 num_layers;

  gint		 pos;
  gint		 bak;
  gint		 wait;
  gint		 pre_focus;
  gint		 focus;
  gint		 i, n;
  gboolean	 noa;

  g_assert (fblur_vals.focus_div > 1 &&
	    fblur_vals.focus_start != fblur_vals.focus_end);

  drawable_ID = drawable->drawable_id;
  image_ID = gimp_drawable_get_image (drawable_ID);

  g_assert (gimp_drawable_is_layer (drawable->drawable_id));

  bak = fblur_vals.focus;
  noa = ! gimp_drawable_has_alpha (drawable_ID);
  orig_name = gimp_drawable_get_name (drawable_ID);

  /* get layer position */
  layers = gimp_image_get_layers (image_ID, &num_layers);
  for (i = 0; i < num_layers; i ++)
    if (layers[i] == drawable_ID)
      break;
  //pos = (num_layers - 1) - i;
  pos = i;
  /* Reference manual says 0 is bottom, but it is same as view order.
     Top to bottom. It is good. But I think still it has bug.
     0 and -1 mean same position. End position is not prepared.
     I think bad, a function for getting a layer posision is
     not prepared, too. Compatibility may be lost. */

  /* delete last white space */
  regcomp (&regex, "[ \t]+$", REG_EXTENDED);
  if (! regexec (&regex, orig_name, 1, match, 0))
    orig_name[match[0].rm_so] = '\0';
  regfree (&regex);

  /* make regex (escape) */
  len = strlen (orig_name) + 1 + 1;
  max_len = (len + 255) & ~255;
  str = g_malloc (max_len);
  str[0] = '^';
  n = 1;
  memcpy (&str[1], orig_name, len - 1);

  regcomp (&regex, "[\\^\\.\\[\\$\\(\\)\\|\\*\\+\\?\\{\\\\]", REG_EXTENDED);
  while (! regexec (&regex, &str[n], 1, match, 0))
    {
      if (len >= max_len)
	{
	  max_len += 256;
	  str = g_realloc (str, max_len);
	}

      n += match[0].rm_so;
      memmove (&str[n + 1], &str[n], len - n);
      len ++;

      str[n] = '\\';
      n += 2;
    }
  regfree (&regex);

  len --;
  if (len + sizeof num_str > max_len)
    {
      max_len += 256;
      str = g_realloc (str, max_len);
    }
  memcpy (&str[len], num_str, sizeof num_str);
  regcomp (&regex, str, REG_EXTENDED);
  g_free (str);

  /* find max number */
  n = 1;
  for (i = 0; i < num_layers; i ++)
    {
      layer_name = gimp_drawable_get_name (layers[i]);
      if (! regexec (&regex, layer_name, 1, match, 0))
	{
	  gchar *sp, *ep;
	  gint lnum;

	  sp = ep = layer_name + match[0].rm_eo;
	  *ep = '\0';
	  while (*(-- sp) != '#');
	  while (*(++ sp) == '0');
	  lnum = atoi (sp);
	  if (lnum >= n)
	    n = lnum + 1;
	}
      g_free (layer_name);
    }
  regfree (&regex);

  g_free (layers);

  /* bind all work */
  gimp_image_undo_group_start (image_ID);

  layer_ID = -1;
  wait = 0;
  pre_focus = -1;
  fval = fblur_vals.focus_start;
  ig = (fblur_vals.focus_curve > 0) ? (1.0 / fblur_vals.focus_curve) : 0;

  for (i = 0; i < fblur_vals.focus_div; i ++)
    {
      fval = (gdouble) i / (fblur_vals.focus_div - 1);
      if (ig > 0)
	fval = pow (fval, ig);
      else if (fval < 0.99999)
	fval = 0;

      fval = fblur_vals.focus_start
	+ (fblur_vals.focus_end - fblur_vals.focus_start) * fval;

      focus = RINT (fval);

      if (focus != pre_focus)
	{
	  layer_ID = gimp_layer_copy (drawable_ID);
	  gimp_image_add_layer (image_ID, layer_ID, pos);

	  pre_focus = focus;
	  wait = fblur_vals.focus_wait;
	  if (wait > 0)
	    layer_name =
	      g_strdup_printf ("%s #%03d (%dms)", orig_name, n, wait);
	  else
	    layer_name = g_strdup_printf ("%s #%03d", orig_name, n);
	  gimp_drawable_set_name (layer_ID, layer_name);
	  g_free (layer_name);
	  n ++;

	  new_d = gimp_drawable_get (layer_ID);
	  fblur_vals.focus = focus;
	  focus_blur (new_d);
	  gimp_drawable_detach (new_d);
	  if (noa)
	    gimp_layer_add_alpha (layer_ID);
	}

      else if (wait > 0)
	{
	  wait += fblur_vals.focus_wait;
	  layer_name =
	    g_strdup_printf ("%s #%03d (%dms)", orig_name, n, wait);
	  gimp_drawable_set_name (layer_ID, layer_name);
	  g_free (layer_name);
	}
    }

  //gimp_image_remove_layer (image_ID, drawable_ID);
  gimp_image_set_active_layer (image_ID, drawable_ID);

  gimp_image_undo_group_end (image_ID);

  g_free (orig_name);
  fblur_vals.focus = bak;
}


/* Many people know what does an angle means,
 * but they do not know how much angle about one direction.
 * There are people they do not want to remember a arithmetic.
 * Besides custom is different by programmer or mathematician.
 * I do not want to remember what kind of implement for one plug-in.
 * So that, I want to use a hour format for a direction.
 * Because a clock always starts from top, and it turns clockwise.
 */
static void
hour_to_coordinates (gdouble *coord_x,
                     gdouble *coord_y,
                     gdouble  hour)
{
  gdouble radian = 2 * G_PI * (hour + 3) / 12;
  *coord_x = cos (radian);
  *coord_y = sin (radian);
}

/* diffusion table management */
static DiffusionTable*
dtable_update (gboolean free)
{
  static DiffusionTable *dtable = NULL;
  static gdouble softness_bak = -1;
  static gdouble fill_bak = -1;
  static gdouble scale_bak = -1;

  if (free)
    {
      if (dtable)
        dtable_free (dtable);
      return NULL;
    }

  gint    reso = COLORMAX + 1; /* fblur_vals.dreso */
  gint    ntables;
  gdouble softness, fill, scale;
  gdouble diff_x, diff_y;

  g_assert (fblur_vals.radius > 0.0 || fblur_vals.model == MODEL_BRUSH);
  g_assert (fblur_vals.bscale > 0.0 || fblur_vals.model != MODEL_BRUSH);
  g_assert (fblur_vals.focus > 0 || fblur_vals.use_map);

  ntables = fblur_vals.use_map ? reso : 1;

  softness = fblur_vals.softness;

  if (fblur_vals.model == MODEL_RING ||
      fblur_vals.model == MODEL_CONCAVE)
    fill = fblur_vals.fill;
  else
    fill = -1;

  if (fblur_vals.model == MODEL_BRUSH)
    scale = fblur_vals.bscale / 100.0;
  else if (! fblur_vals.use_map)
    scale = (gdouble) fblur_vals.focus / COLORMAX;
  else
    scale = 1.0;

  if (fblur_vals.model == MODEL_BRUSH && ! fblur_brush)
    fblur_brush = get_brush_data ("");

  /* convert to xy coordinates */
  if (fblur_vals.use_modeladj)
    {
      gdouble r;
      if (fblur_vals.model != MODEL_BRUSH)
        r = fblur_vals.radius;
      else
        r = hypot ((fblur_brush->width - 1)/2, (fblur_brush->height - 1)/2);

      hour_to_coordinates (&diff_x, &diff_y, fblur_vals.direction);
      gdouble f = r * fblur_vals.diff / 100;
      diff_x *= f;
      diff_y *= f;
    }
  else
    {
      diff_x = 0;
      diff_y = 0;
    }

  if (dtable)
    {
      if (dtable->model == fblur_vals.model &&
          (dtable->model != MODEL_BRUSH ||
           (dtable->model == MODEL_BRUSH && ! fblur_brush->new)) &&
          dtable->radius == fblur_vals.radius &&
          softness_bak == softness &&
          fill_bak == fill &&
          scale_bak == scale &&
          dtable->diff_x == diff_x && dtable->diff_y == diff_y &&
          dtable->ntables == ntables)
        return dtable;

      dtable_free (dtable);
    }

  softness_bak = softness;
  softness /= 100.0;
  softness = CLAMP (softness, 0.0, 1.0);
  fill_bak = fill;
  fill /= 100.0;
  fill = CLAMP (fill, 0.0, 1.0);
  scale_bak = scale;
  dtable = dtable_new (fblur_vals.model, fblur_brush,
                       fblur_vals.radius, softness, fill, scale,
                       diff_x, diff_y, ntables);

  if (dtable->model == MODEL_BRUSH)
    fblur_brush->new = FALSE;

  return dtable;
}

static void
dtable_free (DiffusionTable *dt)
{
  if (! dt)
    return;

  g_free (dt->data);
  g_free (dt->table);
  g_free (dt);
}

static DiffusionTable*
dtable_new (ModelTypes   model,
            BrushData   *brush,
            gdouble      radius,
            gdouble      softness,
            gdouble      fill,
            gdouble      scale,
            gdouble      diff_x,
            gdouble      diff_y,
            gint         ntables)
{
  gint    x_min, x_max, y_min, y_max;
  gdouble rx, ry, rs;
  gint    n, m;

  g_assert (softness >= 0.0 && softness <= 1.0);
  g_assert (fill >= 0.0 && fill <= 1.0);


  if (model != MODEL_BRUSH)
    {
      rx = ry = radius;
    }
  else
    {
      g_assert (brush);
      rx = (gdouble) (brush->width  - 1) / 2;
      ry = (gdouble) (brush->height - 1) / 2;
    }

  rs = softness * MIN (rx, ry);
  x_min = floor (scale * (diff_x - rx - rs));
  y_min = floor (scale * (diff_y - rx - rs));
  x_max =  ceil (scale * (diff_x + ry + rs));
  y_max =  ceil (scale * (diff_y + ry + rs));


  /* converge to origin */
  x_max = MAX (x_max, 0);
  y_max = MAX (y_max, 0);
  x_min = MIN (x_min, 0);
  y_min = MIN (y_min, 0);

  g_assert (x_min <= x_max);
  g_assert (y_min <= y_max);

  DiffusionTable *dt = g_new (DiffusionTable, 1);
  dt->model     = model;
  dt->brush     = (model == MODEL_BRUSH) ? brush : NULL;
  dt->color     = dt->brush && brush->is_color;
  dt->radius    = (model != MODEL_BRUSH) ? radius : 0;
  dt->diff_x     = diff_x;
  dt->diff_y     = diff_y;
  dt->x_min     = x_min;
  dt->x_max     = x_max;
  dt->y_min     = y_min;
  dt->y_max     = y_max;
  dt->width     = dt->x_max - dt->x_min + 1;
  dt->height    = dt->y_max - dt->y_min + 1;
  dt->ntables   = ntables;
  dt->table     = g_new0 (DiffusionTableSub, ntables);
  dt->nelements = dt->width * dt->height;
  dt->length    = sizeof (gdouble) * dt->ntables * dt->nelements * (dt->color ? 4 : 1);
  g_assert (dt->length > 0);
  dt->data      = g_malloc (dt->length);
  gdouble       *datap = dt->data;
  if (dt->color)
    {
      for (n = 0; n < dt->ntables; n ++)
        {
          dt->table[n].value[0] = datap;
          datap += dt->nelements;
          dt->table[n].value[1] = datap;
          datap += dt->nelements;
          dt->table[n].value[2] = datap;
          datap += dt->nelements;
          dt->table[n].value[3] = datap;
          datap += dt->nelements;
        }
    }
  else
    {
      for (n = 0; n < dt->ntables; n ++)
        {
          dt->table[n].value[0] = datap;
          datap += dt->nelements;
          dt->table[n].value[1] = NULL;
          dt->table[n].value[2] = NULL;
          dt->table[n].value[3] = NULL;
        }
    }
  g_assert ((gsize) datap - (gsize) dt->data == dt->length);

  m = ntables - 1;
  if (m)
    {
      for (n = 0; n <= m; n ++)
        {
          gdouble fval = scale * n / m;
          make_dtable_sub (dt, n, fval, fill);
          make_dtable_blur (dt, n, fval * softness);
        }
    }
  else
    {
      make_dtable_sub (dt, 0, scale, fill);
      make_dtable_blur (dt, 0, scale * softness);
    }

  return dt;
}

static void
make_dtable_sub (DiffusionTable *dt,
                 gint            n,
                 gdouble         fval,
                 gdouble         fill)
{
  if (dt->model == MODEL_BRUSH)
    {
      make_dtable_brush (dt, n, fval);
      return;
    }

  gdouble *vp     = dt->table[n].value[0];
  gdouble  radius = dt->radius * fval;
  gdouble  diff_x  = dt->diff_x * fval;
  gdouble  diff_y  = dt->diff_y * fval;
  gdouble  dx, dy, len;
  gdouble  hit = 0;
  gdouble  vr = 0;
  gdouble  fs;
  gint     x, y, b;

  if (dt->model == MODEL_GAUSS)
    {
      vr = 0.3003866304 * (radius + 1);
      vr = -1 / (2 * vr * vr);
    }

  for (y = dt->y_min; y <= dt->y_max; y ++)
    {
      for (x = dt->x_min; x <= dt->x_max; x ++)
        {
          dx = (gdouble) x - diff_x;
          dy = (gdouble) y - diff_y;
          len = hypot (dx, dy);

          switch (dt->model)
            {
            case MODEL_FLAT:
              hit = 1 + radius - len;
              if (hit <= 0)
                {
                  hit = 0;
                  break;
                }
              else if (hit > 1.0)
                hit = 1.0;
              break;

            case MODEL_RING:
              hit = 1 + radius - len;
              if (hit <= 0)
                {
                  hit = 0;
                  break;
                }
              else if (hit >= 2.0)
                {
                  hit = fill;
                  break;
                }
              else if (hit > 1.0)
                {
                  hit = 2.0 - hit;
                  hit = fill + (1.0 - fill) * hit;
                }

              break;

            case MODEL_CONCAVE:
              hit = 1 + radius - len;
              if (hit <= 0)
                {
                  hit = 0;
                  break;
                }
              else if (hit > 1.0)
                {
                  hit = (len + 0.5) / (radius + 0.5);
                  hit = hit * hit;
                  hit = fill + (1.0 - fill) * hit;
                }
              break;

            case MODEL_GAUSS:
              hit = exp (len * len * vr);
              if (hit < (1.0 / COLORMAX))
                  hit = 0; /* cut off */
              break;

            default:
              g_assert_not_reached ();
            }

          g_assert (hit <= 1.0);
          /* hit = MIN (hit, 1.0); */
          *vp ++ = hit;
        }
    }

  fs = 0;
  vp = dt->table[n].value[0];
  for (b = 0; b < dt->nelements; b++)
    {
      fs += *vp ++;
    }

  dt->table[n].density[0] = fs;

  dt->table[n].density[1] = 0;
  dt->table[n].density[2] = 0;
  dt->table[n].density[3] = 0;
}

static void
make_dtable_brush (DiffusionTable *dt,
                   gint            n,
                   gdouble         fval)
{
  gdouble        converge       = 1.0 / MAX (dt->brush->width,
                                             dt->brush->height);
  gdouble        scale          = converge + (1 - converge) * fval;
  g_assert (scale <= 1.0);

  BrushData     *brush          = dt->brush;
  guint          src_width      = brush->width;
  guint          src_height     = brush->height;
  guint8        *src_data[4];
  guint8        *srcp;
  guint          src_x          = 0;
  guint          src_y          = 0;
  /* It will be changed at end of loop when brush is color. */
  gint           src_bpp        = 1;

  guint          dest_width     = dt->width;
  guint          dest_height    = dt->height;
  gdouble        dest_x_off     = -dt->x_min + 0.5 - scale * src_width  / 2 + fval * dt->diff_x;
  gdouble        dest_y_off     = -dt->y_min + 0.5 - scale * src_height / 2 + fval * dt->diff_y;
  gdouble        dest_x;
  gdouble        dest_y;
  guint          dest_x_filled;
  guint          dest_y_filled;

  gint           work_x1                = floor (dest_x_off + 0.0000001);
  gint           work_y1                = floor (dest_y_off + 0.0000001);
  work_x1 = MAX (work_x1, 0);
  work_y1 = MAX (work_y1, 0);
  gint           work_x2        = ceil (dest_x_off + src_width  * scale - 0.0000001);
  gint           work_y2        = ceil (dest_y_off + src_height * scale - 0.0000001);
  work_x2 = MIN (work_x2, dest_width);
  work_y2 = MIN (work_y2, dest_height);
  gint           work_w         = work_x2 - work_x1;
  gint           work_h         = work_y2 - work_y1;

  gdouble        value_tmp[work_w];
  gdouble       *value, *valp, *sump;
  gint           ch             = dt->color ? 4 : 1;
  gint           c;
  guint          b;

  g_assert (work_w <= dest_width);
  g_assert (work_h <= dest_height);
  g_assert (work_w > 0);
  g_assert (work_h > 0);

  src_data[0] = &(brush->data[brush->length - 1]); /* reversed */
  if (brush->is_color)
    {
      src_data[1] = &(brush->data[brush->length * 4 - 3]);
      src_data[2] = &(brush->data[brush->length * 4 - 2]);
      src_data[3] = &(brush->data[brush->length * 4 - 1]);
    }
  else
    src_data[1] = src_data[2] = src_data[3] = NULL;


  for (b = 0; b < work_w; b ++)
    value_tmp[b] = 0;

  for (c = 0; c < ch; c ++)
    {
      value = dt->table[n].value[c];
      for (b = 0; b < dt->nelements; b ++)
        value[b] = 0;

      sump = &(value[work_y1 * dt->width + work_x1]);

      dest_y        = dest_y_off;
      dest_y_filled = work_y1;
      srcp          = src_data[c];

      for (src_y = 0; src_y < src_height; src_y ++)
        {
          gdouble dest_y_pre, across_y, val_y;

          if (dest_y_filled > work_y2)
            break;

          dest_x        = dest_x_off;
          dest_x_filled = work_x1;
          valp          = value_tmp;
          *valp         = 0;

          for (src_x = 0; src_x < src_width; src_x ++, srcp -= src_bpp)
            {
              gdouble dest_x_pre, across_x, val_x;

              if (dest_x_filled > work_x2)
                {
                  while (src_x < src_width)
                    {
                      /* skip to next line */
                      src_x ++;
                      srcp -= src_bpp;
                    }
                  break;
                }

              dest_x_pre = dest_x;
              dest_x += scale;
              across_x   = dest_x - dest_x_filled;

              if (across_x <= 1)
                val_x = scale;
              else
                val_x = (dest_x_filled + 1) - dest_x_pre;
              g_assert (val_x > 0);

              *valp += *srcp * val_x;

              if (across_x < 1)
                continue;

              for (;;)
                {
                  dest_x_filled ++;
                  valp ++;
                  across_x --;
                  g_assert (across_x >= 0);

                  if (across_x >= 1)
                    {
                      *valp = *srcp;

                      if (dest_x_filled > work_x2)
                        break;
                    }
                  else
                    {
                      *valp = *srcp * across_x;
                      break;
                    }
                }
            }

          dest_y_pre = dest_y;
          dest_y += scale;
          across_y   = dest_y - dest_y_filled;

          if (across_y <= 1)
            val_y = scale;
          else
            val_y = (dest_y_filled + 1) - dest_y_pre;
          g_assert (val_y > 0);

          for (b = 0; b < work_w; b ++)
            sump[b] += value_tmp[b] * val_y;

          if (across_y < 1)
            continue;

          for (;;)
            {
              sump += dest_width;
              dest_y_filled ++;
              across_y --;
              g_assert (across_y >= 0);

              if (across_y >= 1)
                {
                  for (b = 0; b < work_w; b ++)
                    sump[b] = value_tmp[b];

                  if (dest_y_filled > work_y2)
                    break;
                }
              else if (across_y > 0)
                {
                  for (b = 0; b < work_w; b ++)
                    sump[b] = value_tmp[b] * across_y;
                  break;
                }
              else /* across_y == 0 */
                break;
            }
        }

      dt->table[n].density[c] = 0;
      for (b = 0; b < dt->nelements; b ++)
        {
          value[b] /= COLORMAX;
          if (value[b] > 1)
            value[b] = 1;

          dt->table[n].density[c] += value[b];
        }

      src_bpp = 3; /* brush color section */
    }

  if (dt->table[n].density[0] <= 0) /* blank data */
    {
      for (b = 0; b < dt->nelements; b ++)
        dt->table[n].value[0][b] = 0;
      dt->table[n].value[0][dt->nelements/2] = 1;
      dt->table[n].density[0] = 1;
    }
  else if (dt->table[n].density[0] < 1) /* this scaling has harmful effect */
    {
      for (b = 0; b < dt->nelements; b ++)
        dt->table[n].value[0][b] /= dt->table[n].density[0];
      dt->table[n].density[0] = 1;
    }
}

static void
make_dtable_blur (DiffusionTable *dt,
                  gint            n,
                  gdouble         fval)
{
  if (fval <= 0)
    return;

  gint     max = MAX (dt->width, dt->height);
  gdouble  radius = ((dt->model != MODEL_BRUSH) ? dt->radius :
                     MIN ((dt->brush->width - 1) / 2,
                          (dt->brush->height - 1) / 2)) * fval;
  gint     r = ceil (radius);

  if (r <= 0)
    return;

  gint     width  = r + 1 + r;
  gdouble  btab[width];
  gdouble *tp = btab + r;
  gdouble  vr;
  gint     ch = dt->color ? 4 : 1;
  gint     x, y, i, b, c;

  vr = 0.3003866304 * (radius + 1);
  vr = -1 / (2 * vr * vr);
  for (i = -r; i <= r; i ++)
    {
      gdouble val = exp (i * i * vr);
      tp[i] = val;
    }

  if (! dt->color)
    {
      gdouble  buf[max], *bp;
      gdouble *vlp, *vp, val, sum;

      for (y = dt->y_min, vlp = dt->table[n].value[0]; y <= dt->y_max;
           y ++, vlp += dt->width)
        {
          for (x = dt->x_min, vp = vlp, bp = buf; x <= dt->x_max;
               x ++, vp ++, bp ++)
            {
              val = 0;
              sum = 0;
              for (i = -r; i <= r; i ++)
                {
                  gint x2 = x + i;

                  if (x2 < dt->x_min || x2 > dt->x_max)
                    continue;

                  val += tp[i] * vp[i];
                  sum += tp[i];
                }
              *bp = val / sum;
            }
          
          for (x = dt->x_min, vp = vlp, bp = buf; x <= dt->x_max;
               x ++, vp ++, bp ++)
            *vp = *bp;
        }

      for (x = dt->x_min, vlp = dt->table[n].value[0]; x <= dt->x_max;
           x ++, vlp ++)
        {
          for (y = dt->y_min, vp = vlp, bp = buf; y <= dt->y_max;
               y ++, vp += dt->width, bp ++)
            {
              val = 0;
              sum = 0;
              for (i = -r; i <= r; i ++)
                {
                  gint y2 = y + i;

                  if (y2 < dt->y_min || y2 > dt->y_max)
                    continue;

                  val += tp[i] * vp[i * dt->width];
                  sum += tp[i];
                }
              *bp = val / sum;
            }
          for (y = dt->y_min, vp = vlp, bp = buf; y <= dt->y_max;
               y ++, vp += dt->width, bp ++)
            *vp = *bp;
        }
    }
  else
    {
      gdouble  buf[4 * max], *bp;
      gdouble *vlp[4], *vp[4], sum;

      vlp[0] = dt->table[n].value[0];
      vlp[1] = dt->table[n].value[1];
      vlp[2] = dt->table[n].value[2];
      vlp[3] = dt->table[n].value[3];

      for (y = dt->y_min; y <= dt->y_max; y ++)
        {
          vp[0] = vlp[0];
          vp[1] = vlp[1];
          vp[2] = vlp[2];
          vp[3] = vlp[3];
          bp = buf;

          for (x = dt->x_min; x <= dt->x_max; x ++)
            {
              bp[0] = bp[1] = bp[2] = bp[3] = 0;
              sum = 0;
              for (i = -r; i <= r; i ++)
                {
                  gdouble f;
                  gint x2 = x + i;

                  if (x2 < dt->x_min || x2 > dt->x_max)
                    continue;

                  f = tp[i] * vp[0][i];
                  bp[0] += f;
                  bp[1] += f * vp[1][i];
                  bp[2] += f * vp[2][i];
                  bp[3] += f * vp[3][i];
                  sum += tp[i];
                }

              if (bp[0] > 0)
                {
                  bp[1] /= bp[0];
                  bp[2] /= bp[0];
                  bp[3] /= bp[0];
                  bp[0] /= sum;
                }
              else
                {
                  bp[0] = bp[1] = bp[2] = bp[3] = 0;
                }

              vp[0] ++;
              vp[1] ++;
              vp[2] ++;
              vp[3] ++;
              bp += 4;
            }

          vp[0] = vlp[0];
          vp[1] = vlp[1];
          vp[2] = vlp[2];
          vp[3] = vlp[3];
          bp = buf;

          for (x = dt->x_min; x <= dt->x_max; x ++)
            {
              *vp[0] = bp[0];
              *vp[1] = bp[1];
              *vp[2] = bp[2];
              *vp[3] = bp[3];

              vp[0] ++;
              vp[1] ++;
              vp[2] ++;
              vp[3] ++;
              bp += 4;
            }

          vlp[0] += dt->width;
          vlp[1] += dt->width;
          vlp[2] += dt->width;
          vlp[3] += dt->width;
        }

      vlp[0] = dt->table[n].value[0];
      vlp[1] = dt->table[n].value[1];
      vlp[2] = dt->table[n].value[2];
      vlp[3] = dt->table[n].value[3];

      for (x = dt->x_min; x <= dt->x_max; x ++)
        {
          vp[0] = vlp[0];
          vp[1] = vlp[1];
          vp[2] = vlp[2];
          vp[3] = vlp[3];
          bp = buf;

          for (y = dt->y_min; y <= dt->y_max; y ++)
            {
              bp[0] = bp[1] = bp[2] = bp[3] = 0;
              sum = 0;
              for (i = -r; i <= r; i ++)
                {
                  gdouble f;
                  gint n;
                  gint y2 = y + i;

                  if (y2 < dt->y_min || y2 > dt->y_max)
                    continue;

                  n = i * dt->width;
                  f = tp[i] * vp[0][n];
                  bp[0] += f;
                  bp[1] += f * vp[1][n];
                  bp[2] += f * vp[2][n];
                  bp[3] += f * vp[3][n];
                  sum += tp[i];
                }

              if (bp[0] > 0)
                {
                  bp[1] /= bp[0];
                  bp[2] /= bp[0];
                  bp[3] /= bp[0];
                  bp[0] /= sum;
                }
              else
                {
                  bp[0] = bp[1] = bp[2] = bp[3] = 0;
                }

              vp[0] += dt->width;
              vp[1] += dt->width;
              vp[2] += dt->width;
              vp[3] += dt->width;
              bp += 4;
            }

          vp[0] = vlp[0];
          vp[1] = vlp[1];
          vp[2] = vlp[2];
          vp[3] = vlp[3];
          bp = buf;

          for (y = dt->y_min; y <= dt->y_max; y ++)
            {
              *vp[0] = bp[0];
              *vp[1] = bp[1];
              *vp[2] = bp[2];
              *vp[3] = bp[3];

              vp[0] += dt->width;
              vp[1] += dt->width;
              vp[2] += dt->width;
              vp[3] += dt->width;
              bp += 4;
            }

          vlp[0] ++;
          vlp[1] ++;
          vlp[2] ++;
          vlp[3] ++;
        }
    }

  for (c = 0; c < ch; c ++)
    {
      gdouble *vp = dt->table[n].value[c];
      dt->table[n].density[c] = 0;
      for (b = 0; b < dt->nelements; b ++, vp ++)
        dt->table[n].density[c] += (*vp = CLAMP (*vp, 0.0, 1.0));
    }
}


/*---- cache previous data (for preview) ---*/

static ImageBuffer*
fblur_update_srcimg (GimpDrawable *drawable)
{
  static ImageBuffer *srcimg = NULL;
  gint x, y, w, h;

  if (! drawable)
    {
      if (srcimg)
        {
          img_free (srcimg);
          srcimg = NULL;
        }
      return NULL;
    }

  /* drawable is always same value */
  if (srcimg)
    return srcimg;

  gimp_drawable_mask_intersect (drawable->drawable_id, &x, &y, &w, &h);
  srcimg = img_new (drawable, x, y, w, h);

  return srcimg;
}

static MapBuffer*
fblur_update_map (gboolean free)
{
  static MapBuffer *map    = NULL;
  static guint32    map_id = -1;
  static gint       focus  = 0;
  static gint       near   = 0;
  gint dmax = COLORMAX;

  if (free)
    {
      if (map)
        {
          g_free (map);
          map = NULL;
        }
      map_id = -1;
      focus  = 0;
      return NULL;
    }

  if (! fblur_vals.use_map)
    return NULL;
  g_assert (fblur_vals.map_id != -1);

  if (map)
    {
      if (map_id == fblur_vals.map_id && focus == fblur_vals.focus)
        {
          if (near != fblur_vals.near) /* invert only */
            {
              guchar *dp  = map->data;
              guchar *eod = &(map->data[map->width * map->height]);
              while (dp < eod)
                {
                  *dp ^= dmax;
                  dp ++;
            }
              near = fblur_vals.near;
            }
          return map;
        }

      g_free (map);
    }

  focus  = fblur_vals.focus;
  map_id = fblur_vals.map_id;
  /* reso = fblur_vals.dreso; */
  near   = fblur_vals.near;
  map    = map_new (map_id, focus, dmax, near);

  return map;
}

static ShineBuffer*
fblur_update_shine (ImageBuffer *img)
{
  static ShineBuffer *shine = NULL;
  static gint light = 0;
  static gdouble saturation = -1;
  static gdouble level = 0;
  static gdouble curve = -1;
  static gint erase = 0;

  gdouble val;
  gint new_erase;
  gdouble new_saturation;


  if (! img)
    {
      if (shine)
        {
          g_free (shine);
          shine = NULL;
        }
      light = 0;
      saturation = -1;
      level = 0;
      curve = -1;
      erase = 0;
      return NULL;
    }

  /* from map image */
  if (fblur_vals.shine_mode == SHINE_MAP)
    {
      if (! fblur_vals.use_shine ||
          fblur_vals.shine_map_id == -1)
        return NULL;

      if (shine)
        {
          if (light == 0)
            return shine;

          g_free (shine);
        }

      shine = shine_new_from_image (img, fblur_vals.shine_map_id);
      light = 0;
      return shine;
    }

  if (! fblur_vals.light || ! fblur_vals.use_shine)
    return NULL;

  if (fblur_vals.shine_mode == SHINE_HSV && fblur_vals.saturation < 0)
    return NULL;

  if (fblur_vals.shine_level <= 0 && fblur_vals.light <= COLORMAX)
    return NULL;

  if (fblur_vals.erase_white)
    {
      val = fblur_vals.erase_white_val / 100;
      new_erase = RINT (img->w * img->h * val * val);

      if (! new_erase)
	return NULL;
    }
  else
    new_erase = -1;

  new_saturation =
    (fblur_vals.shine_mode == SHINE_HSV) ? fblur_vals.saturation : -1;

  /* img is always srcimg */
  if (light == fblur_vals.light &&
      saturation == new_saturation &&
      level == fblur_vals.shine_level &&
      curve == fblur_vals.shine_curve &&
      erase == new_erase)
    return shine;

  if (shine)
    g_free (shine);

  light = fblur_vals.light;
  saturation = new_saturation;
  level = fblur_vals.shine_level;
  curve = fblur_vals.shine_curve;
  erase = new_erase;

  shine = shine_new (img, light, saturation / 100, level / 100, curve);
  if (erase > 0)
    shine_erase_white (shine, erase);

  return shine;
}

/*---- srcdata manage ---*/

static ImageBuffer*
img_allocate (gint       bpp,
              guint      width,
              guint      height)
{
  gsize rowstride = bpp * width;
  gsize length = rowstride * height;
  ImageBuffer *img = g_malloc (sizeof (ImageBuffer) - 1 + length);

  img->x = 0;             /* unsettled */
  img->y = 0;             /* unsettled */
  img->w = width;
  img->h = height;
  img->bpp = bpp;
  img->ch = bpp;          /* unsettled */
  img->is_rgb = FALSE;    /* unsettled */
  img->has_alpha = FALSE; /* unsettled */
  img->rowstride = rowstride;
  img->length = length;

  return img;
}

static void
img_free (ImageBuffer *img)
{
  g_free (img);
}

static ImageBuffer*
img_new (GimpDrawable   *drawable,
         gint            x,
         gint            y,
         gint            width,
         gint            height)
{
  ImageBuffer  *img;
  GimpPixelRgn  pr;
  gpointer      reg_pr;

  /* clip */
  if (x < 0)
    {
      width += x;
      x = 0;
    }
  else if (x + width > drawable->width)
    width = drawable->width - x;
  if (y < 0)
    {
      height += y;
      y = 0;
    }
  else if (y + height > drawable->height)
    height = drawable->height - y;

  img = img_allocate (drawable->bpp, width, height);
  img->x = x;
  img->y = y;
  img->is_rgb = gimp_drawable_is_rgb (drawable->drawable_id);
  img->has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  img->ch = img->has_alpha ? img->bpp - 1 : img->bpp;

  gimp_tile_cache_ntiles (img->w / TILE_WIDTH + 2);
  gimp_pixel_rgn_init
    (&pr, drawable, img->x, img->y, img->w, img->h, FALSE, FALSE);

  for (reg_pr = gimp_pixel_rgns_register (1, &pr);
       reg_pr != NULL; reg_pr = gimp_pixel_rgns_process (reg_pr))
    {
      guchar  *dp, *sp;
      gint     y, len;

      sp = pr.data;
      g_assert (sp);

      dp = &(img->data[img->rowstride * (pr.y - img->y)
                       + img->bpp * (pr.x - img->x)]);
      len = pr.w * pr.bpp;

      for (y = 0; y < pr.h; y ++)
        {
          memcpy (dp, sp, len);
          dp += img->rowstride;
          sp += pr.rowstride;
        }
    }

  return img;
}


/*---- mapdata manage ---*/

static MapBuffer*
map_new (gint32 map_id,
         gint   focus,
         gint   dmax,
         gint   near)
{
  GimpDrawable  *drawable = gimp_drawable_get (map_id);
  GimpPixelRgn   pr;
  gpointer       p;
  COLORBUF      *slp, *sp, *dlp, *dp;

  gsize          length = sizeof (guchar) * drawable->width * drawable->height;
  MapBuffer     *map    = g_malloc (sizeof (MapBuffer) - 1 + length);

  gboolean has_alpha = gimp_drawable_has_alpha (map_id);
  gboolean is_rgb    = gimp_drawable_is_rgb (map_id);
  gint     bpp       = drawable->bpp;
  gint     ch        = has_alpha ? bpp - 1 : bpp;
  gdouble  ab        = 1.0;
  gdouble  f         = (gdouble) dmax / COLORMAX;

  gint           x, y;

  map->width  = drawable->width;
  map->height = drawable->height;

  gimp_tile_cache_ntiles ((drawable->width + TILE_WIDTH - 1) / TILE_WIDTH);
  gimp_pixel_rgn_init (&pr, drawable, 0, 0, drawable->width, drawable->height,
                       FALSE, FALSE);

  for (p = gimp_pixel_rgns_register (1, &pr);
       p != NULL; p = gimp_pixel_rgns_process (p))
    {
      slp = pr.data;
      dlp = &(map->data[pr.y * map->width + pr.x]);
      for (y = pr.y; y < pr.y + pr.h; y ++)
        {
          sp = slp;
          dp = dlp;
          for (x = pr.x; x < pr.x + pr.w; x ++)
            {
              gdouble gray;

              if (is_rgb)
                gray = GIMP_RGB_INTENSITY (sp[0], sp[1], sp[2]);
              else
                gray = sp[0];

              if (has_alpha)
                {
                  ab = (gdouble) sp[ch] / COLORMAX;
                  gray = ab * gray + (1 - ab) * focus;
                }

              if (near) /* when black means near */
                gray = COLORMAX - gray;

              *dp = RINT (f * gray);
              dp ++;
              sp += bpp;
            }
          slp += pr.rowstride;
          dlp += map->width;
        }
    }

  gimp_drawable_detach (drawable);

  return map;
}


/* pre-compute shine values */

static ShineBuffer*
shine_new (ImageBuffer *img,
           gint         light,
           gdouble      saturation,
           gdouble      level,
           gdouble      gamma)
{
  gboolean with_gray      = (! img->is_rgb);
  gboolean with_intensity = (saturation < 0.0) ? TRUE : FALSE;

  gsize length = sizeof (gdouble) * img->w * img->h;
  ShineBuffer *sp = g_malloc (sizeof (ShineBuffer) - 1 + length);
  gdouble *dp = sp->data;

  gdouble offset, offset_sat;
  gint gate;

  gdouble  val;
  gdouble  sat = 1.0; /* inverted */
  gdouble  ig = (gamma > 0) ? (1.0 / gamma) : 0;

  gint     x, y;

  offset = (gdouble) (light - COLORMAX) / COLORMAX;
  offset = CLAMP (offset, 0, 1);
  light = MIN (COLORMAX, light);
  gate = COLORMAX - light;

  offset_sat = saturation - 1.0;
  offset_sat = CLAMP (offset_sat, 0.0, 1.0);
  saturation = MIN (1.0, saturation);

  sp->x = img->x;
  sp->y = img->y;
  sp->w = img->w;
  sp->h = img->h;

  for (y = sp->y; y < sp->y + sp->h; y ++)
    {
      for (x = sp->x; x < sp->x + sp->w; x ++)
        {
          COLORBUF *bp = img_get_p (img, x, y);

          if (with_gray)
            {
              val = bp[0];

            }
          else if (with_intensity)
            {
              val = GIMP_RGB_INTENSITY (bp[0], bp[1], bp[2]);

            }
          else
            {                   /* with HSV */
              COLORBUF max = MAX (MAX (bp[0], bp[1]), bp[2]);
              COLORBUF min = MIN (MIN (bp[0], bp[1]), bp[2]);

              if (max)
                sat = saturation - (gdouble) (max - min) / max;
              else
                sat = 0;

              val = max;
              if (sat > 0)
                sat /= saturation;
              else
                {
                  sat = 1.0;
                  val = 0;
                }

              sat += offset_sat;
              sat = CLAMP (sat, 0, 1);
            }

          val -= gate;
          if (val > 0)
            {
              val /= light;
              val *= sat;

              if (ig > 0)
                val = pow (val, ig);
              else if (val < 0.99999)
                val = 0;
              g_assert (finite (val));

              val *= level;
            }
          else
            val = 0;

          val += offset;
          val = CLAMP (val, 0, 1);

          if (img->has_alpha)
            val *= (gdouble) bp[img->ch] / COLORMAX;

          *dp = val;

          dp ++;
        }
    }

  return sp;
}


static void
shine_erase_white (ShineBuffer	*sp,
		   gint		 gate)
{
  gsize		 num;
  gint8		*table;
  gint8		*tip;
  gint		 count;
  gint		 x, y;

  num = sp->w * sp->h;
  table = g_new0 (gint8, num);

  tip = table;
  for (y = 0; y < sp->h; y ++)
    for (x = 0; x < sp->w; x ++, tip ++)
      {
	if (*tip)
	  continue;

	count = 0;
	shine_erase_white_find (sp, table, &count, 1.0, x, y);
	shine_erase_white_judge (sp, table, x, y,
				 (count == -1 || count > gate) ? 2 : 0);
      }

  shine_erase_white_flush (sp, table);

  g_free (table);
}


static void
shine_erase_white_find (ShineBuffer	*sp,
			gint8		*tp,
			gint		*cp,
			gdouble		 min,
			gint		 x,
			gint		 y)
{
  gdouble val;
  gsize offset;

  if (x < 0 || x >= sp->w ||
      y < 0 || y >= sp->h)
    return;
  offset = y * sp->w + x;

  if (tp[offset] & 1)
    return;

  val = sp->data[offset];

  if (val <= 0.0 || val > min)
    return;

  if (tp[offset] & 2)
    {
      *cp = -1;
      return;
    }

  tp[offset] = 1;

  (*cp) ++;

  shine_erase_white_find (sp, tp, cp, val, x, y - 1);
  shine_erase_white_find (sp, tp, cp, val, x - 1, y);
  shine_erase_white_find (sp, tp, cp, val, x + 1, y);
  shine_erase_white_find (sp, tp, cp, val, x, y + 1);
}


static void
shine_erase_white_judge (ShineBuffer	*sp,
			 gint8		*tp,
			 gint		 x,
			 gint		 y,
			 gint		 val)
{
  gsize offset;

  if (x < 0 || x >= sp->w ||
      y < 0 || y >= sp->h)
    return;
  offset = y * sp->w + x;

  if (! (tp[offset] & 1))
    return;

  tp[offset] = val;

  shine_erase_white_judge (sp, tp, x, y - 1, val);
  shine_erase_white_judge (sp, tp, x - 1, y, val);
  shine_erase_white_judge (sp, tp, x + 1, y, val);
  shine_erase_white_judge (sp, tp, x, y + 1, val);
}


static void
shine_erase_white_flush (ShineBuffer	*sp,
			 gint8		*tp)
{
  gsize offset, max;

  offset = 0;
  max = sp->w * sp->h;

  for (offset = 0; offset < max; offset ++)
    if (tp[offset] & 2)
      sp->data[offset] = 0.0;
}


static ShineBuffer*
shine_new_from_image (ImageBuffer *src,
                      gint32       shine_map_id)
{
  GimpDrawable  *drawable;
  GimpPixelRgn   pr;
  gpointer       reg_pr;

  gsize          length;
  ShineBuffer   *shine;

  g_assert (shine_map_id != -1);
  g_assert (gimp_drawable_is_gray (shine_map_id));
  g_assert (! gimp_drawable_has_alpha (shine_map_id));
  g_assert (gimp_drawable_width  (shine_map_id) == src->w &&
            gimp_drawable_height (shine_map_id) == src->h);

  length = sizeof (gdouble) * src->w * src->h;
  shine = g_malloc (sizeof (ShineBuffer) - 1 + length);

  shine->x = src->x;
  shine->y = src->y;
  shine->w = src->w;
  shine->h = src->h;

  drawable = gimp_drawable_get (fblur_vals.shine_map_id);
  gimp_pixel_rgn_init
    (&pr, drawable, 0, 0, drawable->width, drawable->height, FALSE, FALSE);

  for (reg_pr = gimp_pixel_rgns_register (1, &pr);
       reg_pr != NULL; reg_pr = gimp_pixel_rgns_process (reg_pr))
    {
      gdouble *dlp, *dp;
      guchar  *slp, *sp;
      gint x, y;

      slp = pr.data;
      dlp = &(shine->data[pr.y * shine->w + pr.x]);

      for (y = 0; y < pr.h; y ++)
        {
          dp = dlp;
          sp = slp;
          for (x = 0; x < pr.h; x ++)
            dp[x] = (gdouble) sp[x] / 255;

          dlp += shine->w;
          slp += pr.rowstride;
        }
    }

  gimp_drawable_detach (drawable);

  return shine;
}


static gint
shine_mapmenu_constraint (gint32   image_id,
                          gint32   drawable_id,
                          gpointer data)
{
  ImageBuffer *src = (ImageBuffer *) data;

  if (drawable_id == -1)
    return FALSE;

  if (! gimp_drawable_is_gray (drawable_id))
    return FALSE;

  if (gimp_drawable_has_alpha (drawable_id))
    return FALSE;

  if (gimp_drawable_width (drawable_id) != src->w ||
      gimp_drawable_height (drawable_id) != src->h)
    return FALSE;

  return TRUE;
}


static GtkWidget*
shine_mapmenu_dialog_new (gint          *map_id,
                          GtkWindow     *parent,
                          ImageBuffer   *srcimg)
{
  GtkWidget     *dialog;
  GtkWidget     *vbox;
  GtkWidget     *combo;

  combo = gimp_drawable_combo_box_new (shine_mapmenu_constraint, srcimg);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              -1,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              map_id);
  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (combo), map_id);

  if (*map_id == -1)
    {
      gtk_widget_destroy (combo);
      return NULL;
    }


  dialog = gtk_dialog_new_with_buttons (_("Select Shine Map"), parent, 0,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK,     GTK_RESPONSE_OK,
                                        NULL);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), combo, TRUE, TRUE, 0);

  return dialog;
}


static void
shine_radio_button_update (GtkWidget    *widget,
                           gpointer      data)
{
  static gboolean lock = FALSE;
  GimpDrawable *drawable = (GimpDrawable *) data;

  if (lock)
    return;

  if (! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget)))
    return;

  gint mode =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "gimp-item-data"));

  if (fblur_vals.shine_mode == mode)
    return;

  if (mode == SHINE_MAP)
    {
      ImageBuffer *srcimg = fblur_update_srcimg (drawable);
      GtkWindow *parent = GTK_WINDOW (fblur_widgets[FBLUR_WIDGET_DIALOG]);
      gint32 map_id = fblur_vals.shine_map_id;
      GtkWidget *dialog = shine_mapmenu_dialog_new (&map_id, parent, srcimg);

      if (dialog)
        {
          gtk_widget_show_all (dialog);
          if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
            map_id = -1;

          gtk_widget_destroy (dialog);
        }

      if (map_id == -1)
        {
          lock = TRUE;
          gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (widget),
                                           fblur_vals.shine_mode);
          mode = fblur_vals.shine_mode;
          lock = FALSE;
        }
      else
        {
          fblur_vals.shine_mode = mode;
          fblur_vals.shine_map_id = map_id;
          /* set sensitive */
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LIGHT], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LIGHT_LABEL], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_SATURATION], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_SATURATION_LABEL], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LEVEL], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LEVEL_LABEL], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_CURVE], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_CURVE_LABEL], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_RESET_LEVEL_CURVE], FALSE);
          gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_ERASE_WHITE_BOX], FALSE);
        }
    }

  else
    {
      gboolean t;

      fblur_vals.shine_mode = mode;
      t = (fblur_vals.shine_mode == SHINE_HSV) ? TRUE : FALSE;

      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LIGHT], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LIGHT_LABEL], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_SATURATION], t);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_SATURATION_LABEL], t);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LEVEL], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_LEVEL_LABEL], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_CURVE], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_CURVE_LABEL], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_RESET_LEVEL_CURVE], TRUE);
      gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_ERASE_WHITE_BOX], TRUE);
    }

  //gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));
}


static BrushData*
get_brush_data (const gchar *name)
{
  BrushData *brush;
  guchar *bname;
  gint width, height;
  gint mask_len, color_len;
  gint mask_bpp, color_bpp;
  guint8 *mask_data;
  guint8 *color_data;
  gboolean success;

  if (name[0] == '\0')
    bname = gimp_context_get_brush ();
  else
    bname = g_strdup (name);

  success = gimp_brush_get_pixels (bname, &width, &height,
                                   &mask_bpp, &mask_len, &mask_data,
                                   &color_bpp, &color_len,
                                   &color_data);
  g_assert (success);
  g_assert (mask_bpp == 1);
  g_assert (color_bpp == 0 || color_bpp == 3);

  if (color_data)
    {
      if (color_bpp * width * height != color_len)
        {
          static gboolean shush = FALSE;
          if (! shush)
            {
              g_warning ("This version of Gimp has gimp_brush_get_pixels() it is broken. It doesn't allow to use a color brush from plug-ins. Therefor this plug-in use only mask data at this time.");
              shush = TRUE;
            }
          g_free (color_data);
          color_data = NULL;
          color_bpp = color_len = 0;
        }
    }

  brush = g_malloc (sizeof (BrushData) - 1 + mask_len + color_len);
  brush->name     = bname;
  brush->width    = width;
  brush->height   = height;
  brush->is_color = (color_bpp == 3);
  brush->length   = mask_len; /* if is_color then length *= 4 */

  memcpy (brush->data, mask_data, mask_len);
  g_free (mask_data);
  if (color_data)
    {
      memcpy (brush->data + mask_len, color_data, color_len);
      g_free (color_data);
    }

  return brush;
}

static guint8*
scale_brush (gdouble     scale,
             BrushData  *brush)
{
  g_assert (scale > 0);

  if (brush->is_color)
    return scale_brush_color (scale, brush);

  guint          src_width      = brush->width;
  guint          src_height     = brush->height;
  guint8        *src_data       = brush->data;
  guint8        *srcp           = src_data;

  gdouble        dest_width_f   = src_width * scale;
  gdouble        dest_height_f  = src_height * scale;
  guint          dest_width     = ceil (dest_width_f);
  guint          dest_height    = ceil (dest_height_f);
  gdouble        dest_x_orig    = (dest_width - dest_width_f) / 2;
  gdouble        dest_y_orig    = (dest_height - dest_height_f) / 2;
  gdouble        dest_x         = dest_x_orig;
  gdouble        dest_y         = dest_y_orig;
  guint          dest_x_filled  = 0;
  guint          dest_y_filled  = 0;

  guint          src_x          = 0;
  guint          src_y          = 0;
  gdouble        val_buf[dest_width];
  gdouble        val_buf_sum[dest_width];
  gdouble       *valp;
  guint8        *dest_buf       = g_malloc (dest_width * dest_height);
  guint8        *destp          = dest_buf;
  guint          b;

  for (b = 0; b < dest_width; b ++)
    val_buf_sum[b] = 0;

  for (src_y = 0; src_y < src_height; src_y ++)
    {
      gdouble dest_y_pre, across_y, val_y;

      valp      = val_buf;
      valp[0]   = 0;
      dest_x    = dest_x_orig;
      dest_x_filled = 0;

      for (src_x = 0; src_x < src_width; src_x ++, srcp ++)
        {
          gdouble dest_x_pre, across_x, val_x;

          dest_x_pre = dest_x;
          dest_x += scale;
          across_x = dest_x - dest_x_filled;

          if (across_x <= 1)
            val_x = scale;
          else
            val_x = (dest_x_filled + 1) - dest_x_pre;
          g_assert (val_x > 0);

          valp[0] += srcp[0] * val_x;

          if (across_x < 1)
            continue;

          for (;;)
            {
              dest_x_filled ++;
              valp ++;
              across_x --;
              g_assert (across_x >= 0);

              if (across_x == 0)
                {
                  valp[0] = 0;
                  break;
                }
              else if (across_x >= 1)
                {
                  valp[0] = srcp[0];
                }
              else
                {
                  valp[0] = srcp[0] * across_x;
                  break;
                }
            }
        }

      dest_y_pre = dest_y;
      dest_y += scale;
      across_y = dest_y - dest_y_filled;

      if (across_y <= 1)
        val_y = scale;
      else
        val_y = (dest_y_filled + 1) - dest_y_pre;
      g_assert (val_y > 0);

      for (b = 0; b < dest_width; b ++)
        val_buf_sum[b] += val_buf[b] * val_y;

      if (across_y < 1)
        continue;

      for (b = 0; b < dest_width; b ++)
        destp[b] = RINT (CLAMP (val_buf_sum[b], 0, 255));

      for (;;)
        {
          destp += dest_width;
          dest_y_filled ++;
          across_y --;

          if (across_y >= 1)
            {
              for (b = 0; b < dest_width; b ++)
                destp[b] = RINT (CLAMP (val_buf[b], 0, 255));
            }
          else if (across_y > 0)
            {
              for (b = 0; b < dest_width; b ++)
                val_buf_sum[b] = across_y * val_buf[b];
              break;
            }
          else
            {
              for (b = 0; b < dest_width; b ++)
                val_buf_sum[b] = 0;
              break;
            }
        }
    }

  if (dest_y_filled < dest_height)
    {
      for (b = 0; b < dest_width; b ++)
        *destp ++ = RINT (CLAMP (val_buf_sum[b], 0, 255));
      dest_y_filled ++;
    }
  g_assert (dest_y_filled == dest_height);

  return dest_buf;
}

static guint8*
scale_brush_color (gdouble    scale,
                   BrushData *brush)
{
  g_assert (brush->is_color);
  g_assert (scale > 0);

  guint src_width = brush->width;
  guint src_height = brush->height;
  guint8 *src_mskp = brush->data;
  guint8 *src_colp = brush->data + brush->length;

  gdouble dest_width_f = src_width * scale;
  gdouble dest_height_f = src_height * scale;
  guint dest_width = ceil (dest_width_f);
  guint dest_height = ceil (dest_height_f);
  gdouble dest_x_orig = (dest_width - dest_width_f) / 2;
  gdouble dest_y_orig = (dest_height - dest_height_f) / 2;
  gdouble dest_x = dest_x_orig;
  gdouble dest_y = dest_y_orig;
  guint dest_x_filled = 0;
  guint dest_y_filled = 0;

  guint src_x = 0;
  guint src_y = 0;
  gdouble val_buf[4 * dest_width];
  gdouble sum_buf[4 * dest_width];
  gdouble *mask_val = val_buf;
  gdouble *color_val = val_buf + dest_width;
  gdouble *mask_sum = sum_buf;
  gdouble *color_sum = sum_buf + dest_width;
  gdouble *mskp;
  gdouble *colp;
  gint dest_length = dest_width * dest_height;
  guint8 *dest_buf = g_malloc (4 * dest_length);
  guint8 *dest_mskp = dest_buf;
  guint8 *dest_colp = dest_buf + dest_length;
  guint x, b;

  for (b = 0; b < 4 * dest_width; b ++)
    sum_buf[b] = 0;

  for (src_y = 0; src_y < src_height; src_y ++)
    {
      gdouble dest_y_pre, across_y, val_y;

      mskp = mask_val;
      colp = color_val;
      mskp[0] = colp[0] = colp[1] = colp[2] = 0;

      dest_x = dest_x_orig;
      dest_x_filled = 0;

      for (src_x = 0; src_x < src_width; src_x ++, src_mskp ++, src_colp += 3)
        {
          gdouble dest_x_pre, across_x, val_x, alpha;

          dest_x_pre = dest_x;
          dest_x += scale;
          across_x = dest_x - dest_x_filled;

          if (across_x <= 1)
            val_x = scale;
          else
            val_x = (dest_x_filled + 1) - dest_x_pre;
          g_assert (val_x > 0);

          alpha = (gdouble) src_mskp[0] * val_x;
          mskp[0] += alpha;
          colp[0] += alpha * src_colp[0];
          colp[1] += alpha * src_colp[1];
          colp[2] += alpha * src_colp[2];

          if (across_x < 1)
            continue;

          for (;;)
            {
              dest_x_filled ++;
              mskp ++;
              colp += 3;
              across_x --;
              g_assert (across_x >= 0);

              if (across_x >= 1)
                {
                  mskp[0] = src_mskp[0];
                  colp[0] = src_mskp[0] * src_colp[0];
                  colp[1] = src_mskp[0] * src_colp[1];
                  colp[2] = src_mskp[0] * src_colp[2];
                }
              else if (across_x > 0)
                {
                  mskp[0] = src_mskp[0] * across_x;
                  colp[0] = mskp[0] * src_colp[0];
                  colp[1] = mskp[0] * src_colp[1];
                  colp[2] = mskp[0] * src_colp[2];
                  break;
                }
              else
                {
                  mskp[0] = colp[0] = colp[1] = colp[2] = 0;
                  break;
                }
            }
        }

      dest_y_pre = dest_y;
      dest_y += scale;
      across_y = dest_y - dest_y_filled;

      if (across_y <= 1)
        val_y = scale;
      else
        val_y = (dest_y_filled + 1) - dest_y_pre;
      g_assert (val_y > 0);

      for (x = 0, b = 0; x < dest_width; x ++, b += 3)
        {
          mask_sum[x] += val_y * mask_val[x];
          color_sum[b] += val_y * color_val[b];
          color_sum[b + 1] += val_y * color_val[b + 1];
          color_sum[b + 2] += val_y * color_val[b + 2];
        }

      if (across_y < 1)
        continue;

      for (x = 0, b = 0; x < dest_width; x ++, b += 3)
        {
          gdouble alpha = mask_sum[x];
          gint val = 0;
          if (alpha > 0)
            {
              val = RINT (alpha);
              dest_mskp[x] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b] / alpha);
              dest_colp[b] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b + 1] / alpha);
              dest_colp[b + 1] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b + 2] / alpha);
              dest_colp[b + 2] = CLAMP (val, 0, 255);
            }
          else
            {
              dest_mskp[x] = 0;
              dest_colp[b] = 0;
              dest_colp[b + 1] = 0;
              dest_colp[b + 2] = 0;
            }
        }

      for (;;)
        {
          dest_mskp += dest_width;
          dest_colp += 3 * dest_width;
          dest_y_filled ++;
          across_y --;

          if (across_y >= 1)
            {
              for (x = 0, b = 0; x < dest_width; x ++, b += 3)
                {
                  gdouble alpha = mask_val[x];
                  gint val = 0;
                  if (alpha > 0)
                    {
                      val = RINT (alpha);
                      dest_mskp[x] = CLAMP (val, 0, 255);
                      val = RINT (color_val[b] / alpha);
                      dest_colp[b] = CLAMP (val, 0, 255);
                      val = RINT (color_val[b + 1] / alpha);
                      dest_colp[b + 1] = CLAMP (val, 0, 255);
                      val = RINT (color_val[b + 2] / alpha);
                      dest_colp[b + 2] = CLAMP (val, 0, 255);
                    }
                  else
                    {
                      dest_mskp[x] = 0;
                      dest_colp[b] = 0;
                      dest_colp[b + 1] = 0;
                      dest_colp[b + 2] = 0;
                    }
                }
            }
          else if (across_y > 0)
            {
              for (x = 0, b = 0; x < dest_width; x ++, b += 3)
                {
                  mask_sum[x] = across_y * mask_val[x];
                  color_sum[b] = across_y * color_val[b];
                  color_sum[b + 1] = across_y * color_val[b + 1];
                  color_sum[b + 2] = across_y * color_val[b + 2];
                }
              break;

            }
          else
            {
              for (b = 0; b < 4 * dest_width; b ++)
                sum_buf[b] = 0;
              break;
            }
        }
    }

  if (dest_y_filled < dest_height)
    {
      for (x = 0, b = 0; x < dest_width; x ++, b += 3)
        {
          gdouble alpha = mask_sum[x];
          gint val = 0;
          if (alpha > 0)
            {
              val = RINT (alpha);
              dest_mskp[x] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b] / alpha);
              dest_colp[b] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b + 1] / alpha);
              dest_colp[b + 1] = CLAMP (val, 0, 255);
              val = RINT (color_sum[b + 2] / alpha);
              dest_colp[b + 2] = CLAMP (val, 0, 255);
            }
          else
            {
              dest_mskp[x] = 0;
              dest_colp[b] = 0;
              dest_colp[b + 1] = 0;
              dest_colp[b + 2] = 0;
            }
        }
      dest_mskp += dest_width;
      dest_colp += 3 * dest_width;
      dest_y_filled ++;
    }
  g_assert (dest_y_filled == dest_height);

  return dest_buf;
}


/*---- Dialog ---*/

static void
model_combobox_append (GtkListStore     *list_store,
                       GtkTreeIter      *iter,
                       gint              val,
                       gchar            *label,
                       const GdkPixdata *pixdata)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_from_pixdata (pixdata, TRUE, NULL);
  gtk_list_store_append (list_store, iter);
  gtk_list_store_set (list_store, iter,
                      GIMP_INT_STORE_VALUE, val,
                      GIMP_INT_STORE_LABEL, label,
                      GIMP_INT_STORE_PIXBUF, pixbuf, -1);
  g_object_unref (pixbuf);
}

static void
change_brush_callback (const gchar          *brush_name,
                       gdouble              opacity,
                       gint                 spacing,
                       GimpLayerModeEffects  paint_mode,
                       gint                 width,
                       gint                 height,
                       const guchar        *mask_data,
                       gboolean             dialog_closing,
                       gpointer             user_data)
{
  GtkTreeModel  *tree_model = user_data;
  GtkTreeIter    iter;
  gchar         *labelbuf;
  GdkPixdata    *pixdata_brush;
  GdkPixbuf     *pixbuf;
  gboolean       t;

  if (! dialog_closing)
    return;

  /* If you can't update brush with error message, like this
   *
   *  > assertion `GTK_LIST_STORE (tree_model)->stamp == iter->stamp' failed
   *  > assertion `VALID_ITER (iter, list_store)' failed
   *
   * You should check compile flags such as memory alignment.
   * Resign greater flags, or use same flags as glib.
   */

  g_assert (gtk_tree_model_get_iter_first (tree_model, &iter));
  /* skip Flat, Ring, Concave, and Gauss */
  for (;;)
    {
      ModelTypes model;
      gtk_tree_model_get (tree_model, &iter, 0, &model, -1);
      if (model == MODEL_BRUSH)
        break;
      g_return_if_fail (gtk_tree_model_iter_next (tree_model, &iter));
    }

  /* reset Brush info */

  brush_free (fblur_brush);
  fblur_brush = get_brush_data (brush_name);
  labelbuf = g_strdup_printf (_("Brush (%dx%d)"),
                              fblur_brush->width, fblur_brush->height);
  pixdata_brush = make_brush_thumbnail (fblur_brush, 17, 17, 1);
  pixbuf = gdk_pixbuf_from_pixdata (pixdata_brush, TRUE, NULL);
  g_free (pixdata_brush);

  gtk_list_store_set (GTK_LIST_STORE (tree_model), &iter,
                      1, labelbuf, 3, pixbuf, -1);
  g_free (labelbuf);
  g_object_unref (pixbuf);

  /* set sensitive */
  t = fblur_brush->is_color;
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_WHITEGLOW], t);
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_WHITEGLOW_LABEL], t);
}


/* make brush thumbnail in RGB (3 bytes per pixel) */
static GdkPixdata*
make_brush_thumbnail (BrushData *brush,
                      gint       width,
                      gint       height,
                      gint       padding)
{
  const gint bpp = 3;
  gint rowstride = bpp * width;
  gint length = rowstride * height;

  GdkPixdata *pixdata = g_malloc0 (sizeof (GdkPixdata) + length);
  guint8 *data = (guint8 *) pixdata + sizeof (GdkPixdata);

  gint w2 = width - padding - padding;
  gint h2 = height - padding - padding;
  g_assert (w2 > 0 && h2 > 0);

  gint xsize = brush->width;
  gint ysize = brush->height;
  guint8 *thumb = NULL;

  if (brush->width > w2 || brush->height > h2)
    {
      gdouble scale = MIN ((gdouble) w2 / brush->width,
                           (gdouble) h2 / brush->height) - 0.0000001;
      xsize = ceil (brush->width * scale);
      ysize = ceil (brush->height * scale);
      g_assert (xsize <= w2 && ysize <= h2);

      thumb = scale_brush (scale, brush);
    }

  gint xoff = (width - xsize) / 2;
  gint yoff = (height - ysize) / 2;
  guint8 *sp = (thumb) ? thumb : brush->data;
  guint8 *dp = &data[yoff * rowstride + xoff * bpp];

  /* A+RGB to RGB or GRAY to RGB */
  gint b, x, y;
  if (brush->is_color)
    {
      guint8 *cp = sp + xsize * ysize;
      gint color_rowstride = 3 * xsize;
      for (y = 0; y < ysize; y ++)
        {
          guint8 *bp = dp;
          for (b = 0, x = 0; x < xsize; x ++, b += bpp)
            {
              bp[b] = cp[b] * sp[x] / 255;
              bp[b + 1] = cp[b + 1] * sp[x] / 255;
              bp[b + 2] = cp[b + 2] * sp[x] / 255;
            }
          sp += xsize; /* mask_rowstride */
          cp += color_rowstride;
          dp += rowstride;
        }
    }
  else
    {
      for (y = 0; y < ysize; y ++)
        {
          guint8 *bp = dp;
          for (x = 0; x < xsize; x ++)
            {
              bp[0] = bp[1] = bp[2] = sp[x];
              bp += bpp;
            }
          sp += xsize;
          dp += rowstride;
        }
    }

  if (thumb)
    g_free (thumb);

  pixdata->magic = GDK_PIXBUF_MAGIC_NUMBER;
  pixdata->length = GDK_PIXDATA_HEADER_LENGTH + length;
  pixdata->pixdata_type = GDK_PIXDATA_COLOR_TYPE_RGB |
    GDK_PIXDATA_SAMPLE_WIDTH_8 | GDK_PIXDATA_ENCODING_RAW;
  pixdata->rowstride = width * bpp;
  pixdata->width = width;
  pixdata->height = height;
  pixdata->pixel_data = data;

  return pixdata;
}

static GtkWidget*
notebook_append_vbox (GtkNotebook *notebook,
                      GtkWidget   *tab_label)
{
  GtkWidget *scrlwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy
    (GTK_SCROLLED_WINDOW (scrlwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  GtkWidget *viewport = gtk_viewport_new (NULL, NULL);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_set_border_width (GTK_CONTAINER (viewport), 6);
  gtk_container_add (GTK_CONTAINER (scrlwin), viewport);

  GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (viewport), vbox);

  GtkWidget *vpaned = gtk_vpaned_new ();
  gtk_paned_add1 (GTK_PANED (vpaned), scrlwin);

  gtk_notebook_append_page (notebook, vpaned, tab_label);
  return vbox;
}

static gboolean
focus_blur_dialog (GimpDrawable *drawable)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *align;
  GtkWidget *radio;
  GtkWidget *table;
  GtkWidget *spinbutton;
  GtkWidget *toggle;
  GtkObject *adj;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *combo;
  GtkSizeGroup *size_group;
  GtkWidget *vpaned;
  GtkWidget *scrlwin;


  gimp_ui_init ("fblur", TRUE);


  /* ---------- dialog ---------- */

  GtkWidget *dlg = gimp_dialog_new (_("Focus Blur"), "focus_blur", NULL, 0,
                                    gimp_standard_help_func, NULL,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  fblur_widgets[FBLUR_WIDGET_DIALOG] = dlg;

  GtkWidget *main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), main_vbox,
                      TRUE, TRUE, 0);

  /* ---------- preview interface ---------- */

#if (FBLUR_PREVIEW == 2)
  GtkWidget *preview =
    gimp_drawable_preview_new (drawable, &fblur_vals.preview);
  gtk_box_pack_start (GTK_BOX (main_vbox), preview, FALSE, TRUE, 0);
  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (update_preview_init), drawable);
  g_signal_connect (GIMP_PREVIEW (preview)->area, "button_press_event",
                    G_CALLBACK (update_preview_event), NULL);
#endif
#if (FBLUR_PREVIEW == 1)
  FblurPreview *preview = fblur_preview_new (drawable);
  gtk_box_pack_start (GTK_BOX (main_vbox), preview->widget, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (dlg), "event",
                    G_CALLBACK (preview_dialog_event_hook), preview);
#endif


  /* notebook to select options */

  GtkWidget *notebook = gtk_notebook_new ();
  fblur_widgets[FBLUR_WIDGET_NOTEBOOK] = notebook;
  //gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_box_pack_start (GTK_BOX (main_vbox), notebook, TRUE, TRUE, 0);

  GtkWidget *nb0 = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (nb0), 6);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), nb0,
                            gtk_label_new_with_mnemonic (_("_Main")));

  GtkWidget *nb1 = notebook_append_vbox
    (GTK_NOTEBOOK (notebook), gtk_label_new_with_mnemonic (_("Mod_el")));

  GtkWidget *nb4 = notebook_append_vbox
    (GTK_NOTEBOOK (notebook), gtk_label_new_with_mnemonic (_("_Shine")));

  GtkWidget *nb2 = notebook_append_vbox
    (GTK_NOTEBOOK (notebook), gtk_label_new_with_mnemonic (_("_Distance")));

  GtkWidget *nb7 = notebook_append_vbox
    (GTK_NOTEBOOK (notebook), gtk_label_new_with_mnemonic (_("_Utility")));

  GtkWidget *nb9 = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (nb9), 6);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), nb9,
                            gtk_label_new_with_mnemonic (_("_About")));

  /* ---------- nb0: Basic paramaters ---------- */

  /* 0: model and radius */

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (nb0), vbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Model and Radius:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);

  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);

  /* maximum radius for blurring */

  spinbutton = gimp_spin_button_new (&adj, fblur_vals.radius,
                                     0.0, 200.0, 1.0, 10.0, 0.0, 1.0, 2);
  fblur_widgets[FBLUR_WIDGET_RADIUS] = spinbutton;
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, spinbutton);

  gimp_help_set_help_data (spinbutton,
                           _("It is a maximum radius(px) for blurring. Destined image will be made indistinctness as it has been larger, and nothing to do when it's zero."),
                           NULL);

  gtk_widget_set_sensitive (spinbutton, (fblur_vals.model != MODEL_BRUSH));
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.radius);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* model */

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);

  gchar *labelbuf;

  combo = g_object_new (GIMP_TYPE_INT_COMBO_BOX, NULL);
  fblur_widgets[FBLUR_WIDGET_MODEL] = combo;
  gtk_container_add (GTK_CONTAINER (align), combo);

  GtkTreeModel *tree_model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  GtkTreeIter   tree_iter;
  gtk_tree_model_get_iter_first (tree_model, &tree_iter);

  model_combobox_append (GTK_LIST_STORE (tree_model), &tree_iter,
                         MODEL_FLAT, _("Flat"), &pixdata_flat);

  model_combobox_append (GTK_LIST_STORE (tree_model), &tree_iter,
                         MODEL_RING, _("Ring"), &pixdata_ring);

  model_combobox_append (GTK_LIST_STORE (tree_model), &tree_iter,
                         MODEL_CONCAVE, _("Concave"), &pixdata_concave);

  model_combobox_append (GTK_LIST_STORE (tree_model), &tree_iter,
                         MODEL_CONCAVE, _("Gauss"), &pixdata_gauss);

  fblur_brush = get_brush_data ("");
  GdkPixdata *pixdata_brush = make_brush_thumbnail (fblur_brush, 17, 17, 1);
  labelbuf = g_strdup_printf (_("Brush (%dx%d)"),
                              fblur_brush->width, fblur_brush->height);
  model_combobox_append (GTK_LIST_STORE (tree_model), &tree_iter,
                         MODEL_BRUSH, labelbuf, pixdata_brush);
  g_free (pixdata_brush);
  g_free (labelbuf);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), fblur_vals.model);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (model_combobox_callback), &fblur_vals.model);

  gimp_help_set_help_data (combo,
                           _("Specify model for diffusion of color. it will mkae a image to indistinctness."),
                           NULL);
  PREVIEW_INVALIDATE (combo, "changed");


  /* 1: distance map */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (nb0), vbox, FALSE, FALSE, 0);

  /* switch */

  toggle = gtk_check_button_new_with_label (_("Distance Map:"));
  fblur_widgets[FBLUR_WIDGET_USE_MAP] = toggle;
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.use_map);
  gimp_help_set_help_data (toggle,
                           _("It uses a map image to make distance to objects (each pixels) in source image."),
                           NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (gimp_toggle_button_update),
                    &fblur_vals.use_map);
  PREVIEW_INVALIDATE (toggle, "toggled");

  /* select map */

  combo = gimp_drawable_combo_box_new (mapmenu_constraint, NULL);
  fblur_widgets[FBLUR_WIDGET_MAP_ID] = combo;
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              fblur_vals.map_id,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &fblur_vals.map_id);

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (align), combo);

  /* not work with GimpIntComboBox */
  gimp_help_set_help_data (combo, _("Map image to make distance."), NULL);
  PREVIEW_INVALIDATE (combo, "changed");

  /* sensitive for distance map */
  gtk_widget_set_sensitive (combo, fblur_vals.use_map);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", combo);


  /* 3: point of focus */

  table = gtk_table_new (1, 3, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 3);
  gtk_box_pack_start (GTK_BOX (nb0), table, FALSE, FALSE, 0);

  /* focus */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("Focus:"), SCALE_WIDTH, 0,
                              fblur_vals.focus, 0, COLORMAX, 1, 10, 0,
                              TRUE, 0, 0,
                              _("Specify focal distance in numerical value of color. It is compared with color in map image, or zero when it does not sat. Its difference have an effect on radius."),
                              NULL);
  spinbutton = GIMP_SCALE_ENTRY_SPINBUTTON (adj);
  fblur_widgets[FBLUR_WIDGET_FOCUS] = spinbutton;
  gtk_size_group_add_widget (size_group, spinbutton);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.focus);
  PREVIEW_INVALIDATE (adj, "value_changed");


  /* ---------- nb1: Model ---------- */

  /* softness */

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (nb1), hbox, FALSE, FALSE, 0);
  label = gtk_label_new (_("Softness:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.softness,
                                     0.0, 100.0, 1.0, 10.0, 0.0, 1.0, 1);
  fblur_widgets[FBLUR_WIDGET_SOFTNESS] = spinbutton;
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, spinbutton);

  gimp_help_set_help_data (spinbutton,
                           _("This value makes soft shape. It makes no difference as to apply gaussian blur, except that it can use distance map."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.softness);
  PREVIEW_INVALIDATE (adj, "value_changed");


  /* filling inside */

  hbox = gtk_hbox_new (FALSE, 3);
  fblur_widgets[FBLUR_WIDGET_FILL_BOX] = hbox;
  gtk_box_pack_start (GTK_BOX (nb1), hbox, FALSE, FALSE, 0);
  label = gtk_label_new (_("Filling inside:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.fill,
                                     0.0, 100.0, 1.0, 10.0, 0.0, 1.0, 1);
  fblur_widgets[FBLUR_WIDGET_FILL] = spinbutton;
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, spinbutton);

  gimp_help_set_help_data (spinbutton,
                           _("It is a opacity to fill inside of models Ring and Concave."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.fill);
  PREVIEW_INVALIDATE (adj, "value_changed");
  if (fblur_vals.model != MODEL_RING &&
      fblur_vals.model != MODEL_CONCAVE)
    gtk_widget_set_sensitive (hbox, FALSE);


  /* brush control */

  vbox = gtk_vbox_new (FALSE, 3);
  fblur_widgets[FBLUR_WIDGET_BSCALE_BOX] = vbox;
  gtk_box_pack_start (GTK_BOX (nb1), vbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  label = gtk_label_new (_("Brush and Scale:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.bscale,
                                     0.0, 100.0, 1.0, 10.0, 0.0, 1.0, 1);
  fblur_widgets[FBLUR_WIDGET_BSCALE] = spinbutton;
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, spinbutton);

  gimp_help_set_help_data (spinbutton,
                           _("This value makes brush size smaller."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.bscale);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* brush selector widget */
  button = gimp_brush_select_widget_new 
    (NULL, NULL, -1, -1, -1, (GimpRunBrushCallback) change_brush_callback, tree_model);
  fblur_widgets[FBLUR_WIDGET_BRUSH_SEL] = button;

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (align), button);

  if (fblur_vals.model != MODEL_BRUSH)
    gtk_widget_set_sensitive (vbox, FALSE);


  /* siwtch of model adjustment */

  toggle = gtk_check_button_new_with_label (_("Balance adjustment:"));
  fblur_widgets[FBLUR_WIDGET_USE_MODELADJ] = toggle;
  gtk_box_pack_start (GTK_BOX (nb1), toggle, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.use_modeladj);
  gimp_help_set_help_data (toggle,
                           _("If you want to make a slant of shadow or to get a center of brush, check this item to enable below parameters."),
                           NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (gimp_toggle_button_update),
                    &fblur_vals.use_modeladj);
  PREVIEW_INVALIDATE (toggle, "toggled");

  /* table */

  table = gtk_table_new (3, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 0);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);

  align = gtk_alignment_new (1.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), table);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (nb1), vbox, FALSE, FALSE, 0);

  /* sensitive for model adjustment */
  gtk_widget_set_sensitive (vbox, fblur_vals.use_modeladj);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", vbox);


  /* 0: difference */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.diff,
                                     0.0, 100.0, 1.0, 10.0, 0, 1, 1);
  fblur_widgets[FBLUR_WIDGET_DIFF] = spinbutton;
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                             _("Difference:"), 0.0, 0.5, spinbutton, 1, TRUE);
  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify difference in percentage of radius. This parameter slides center of bluring. It is usually 0."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.diff);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* 1: direction */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.direction,
                                     0.0, 12.0, 0.5, 3.0, 0, 1, 2);
  fblur_widgets[FBLUR_WIDGET_DIRECTION] = spinbutton;
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 1,
                             _("Direction:"), 0.0, 0.5, spinbutton, 1, TRUE);
  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify direction for difference. Angle is hour format (0 .. 12)."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.direction);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* reset button for difference and direction */
  button = gtk_button_new_with_mnemonic (_("_Reset"));
  align = gtk_alignment_new (0.0, 1.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), button);
  gtk_table_attach (GTK_TABLE (table), align, 2, 3, 0, 2,
                    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_size_group_add_widget (size_group, button);
  gimp_help_set_help_data (button, _("It resets difference and direction."), NULL);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_diff_reset), fblur_widgets);

  /* brush center to diff */
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Adjust to center:"));
  align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  button = gtk_button_new_with_mnemonic (_("Adjust"));
  gtk_size_group_add_widget (size_group, button);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gimp_help_set_help_data (button, _("It will compute a center of brush."), NULL);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_center_to_diff), fblur_widgets);


  /* ---------- nb2: Distance ---------- */

  /* 0: Which does color mean near, black or white. */

  GtkWidget *widget_near_white;
  GtkWidget *widget_near_black;

  radio = gimp_int_radio_group_new
    (FALSE, NULL, G_CALLBACK (gimp_radio_button_update),
     &fblur_vals.near, fblur_vals.near,
     _("White means near."), 0, &widget_near_white,
     _("Black means near."), 1, &widget_near_black,
     NULL);
  fblur_widgets[FBLUR_WIDGET_NEAR] = widget_near_white;

  gimp_help_set_help_data (widget_near_white, _("White in distance map means near. This is default, because easy to know by intuition. Near or far is related to distance weighting."), NULL);
  gimp_help_set_help_data (widget_near_black, _("Black in distance map means near, This switch for using generic images which are made by 3D renderer. Near or far is related to distance weighting."), NULL);
  PREVIEW_INVALIDATE (widget_near_white, "toggled");

  label = gtk_label_new (_("How to decide a distance:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (nb2), align, FALSE, FALSE, 0);

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (nb2), align, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (align), radio);

  /* 1: Distance weighting mode */

  GtkWidget *widget_dweight_none;
  GtkWidget *widget_dweight_obst;
  GtkWidget *widget_dweight_depth;

  radio = gimp_int_radio_group_new
    (FALSE, NULL, G_CALLBACK (gimp_radio_button_update),
     &fblur_vals.dist_mode, fblur_vals.dist_mode,
     _("Disable distance weighting."), DWEIGHT_NONE, &widget_dweight_none,
     _("Obstruct blurring in rear of pixels."), DWEIGHT_OBSTRUCTION, &widget_dweight_obst,
     _("Estimate all pixels in depth order."), DWEIGHT_DEPTH_ORDER, &widget_dweight_depth,
     NULL);
  fblur_widgets[FBLUR_WIDGET_DIST_MODE] = widget_dweight_none;

  gimp_help_set_help_data (widget_dweight_none, _("It doesn't use distance weighting. Usefull for when distance map is smooth gradient, or just aplying blur without a distance."), NULL);
  gimp_help_set_help_data (widget_dweight_obst, _("It bases amount of blurring at estimated pixel. Blurring of rear pixels that is brought from around of focused pixel is obstructed. This is enough to blur only background."), NULL);
  gimp_help_set_help_data (widget_dweight_depth, _("All pixels are estimated in depth order. Near pixels block rear pixels. And, rear pixels make edge foreground when they are focused. So that, lacks value is filled with rough blurring."), NULL);

  //PREVIEW_INVALIDATE (widget_dweight_none, "toggled");
  PREVIEW_INVALIDATE (widget_dweight_obst, "toggled");
  PREVIEW_INVALIDATE (widget_dweight_depth, "toggled");

  /* sensitive for distance weight mode */
  gtk_widget_set_sensitive (nb2, fblur_vals.use_map);
  g_object_set_data (G_OBJECT (fblur_widgets[FBLUR_WIDGET_MAP_ID]),
                     "set_sensitive", nb2);

  label = gtk_label_new (_("Distance weighting:"));
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (nb2), align, FALSE, FALSE, 0);

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (nb2), align, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (align), radio);


  /* ---------- nb4: Shine ---------- */

  /* check params */
  if (fblur_vals.shine_mode == SHINE_MAP)
    fblur_vals.shine_mode = SHINE_INTENSITY;

  gboolean is_rgb = gimp_drawable_is_rgb (drawable->drawable_id);
  if (! is_rgb && fblur_vals.shine_mode == SHINE_HSV)
    fblur_vals.shine_mode = SHINE_INTENSITY;

  /* switch of shine */

  toggle = gtk_check_button_new_with_label (_("Shine on lighting pixels:"));
  fblur_widgets[FBLUR_WIDGET_USE_SHINE] = toggle;
  gtk_box_pack_start (GTK_BOX (nb4), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.use_shine);
  gimp_help_set_help_data (toggle,
                           _("If you want to use a blur with shine, check this item to enable below parameters. It makes photo realistic blurring by keeping brightness on shining pixels."),
                           NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (gimp_toggle_button_update),
                    &fblur_vals.use_shine);
  PREVIEW_INVALIDATE (toggle, "toggled");

  /* table */

  table = gtk_table_new (5, 4, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 0);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacing (GTK_TABLE (table), 1, 8);
  gtk_table_set_row_spacing (GTK_TABLE (table), 3, 8);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (nb4), hbox, FALSE, FALSE, 0);

  /* sensitive for shine */
  gtk_widget_set_sensitive (table, fblur_vals.use_shine);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", table);
  GtkWidget *shine_sensitive = table;   /* copy */


  /* 0: light value */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.light,
                                     1, COLORMAX, 1, 8, 0, 1, 0);
  fblur_widgets[FBLUR_WIDGET_LIGHT] = spinbutton;
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                                     _("Light value:"), 0.0, 0.5,
                                     spinbutton, 1, TRUE);
  fblur_widgets[FBLUR_WIDGET_LIGHT_LABEL] = label;

  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify light value to decide shining pixels. It is a threshold from white in color value."),
                           NULL);
  PREVIEW_INVALIDATE (adj, "value_changed");
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.light);


  /* 1: saturation */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.saturation,
                                     0.0, 100.0, 1.0, 10.0, 0, 1, 1);
  fblur_widgets[FBLUR_WIDGET_SATURATION] = spinbutton;
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, 1,
                                     _("Saturation:"), 0.0, 0.5,
                                     spinbutton, 1, TRUE);
  fblur_widgets[FBLUR_WIDGET_SATURATION_LABEL] = label;

  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify rate in percentage for saturation to decides light. It is allowed to mix saturation by this value."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.saturation);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* sensitive for saturation */
  if (fblur_vals.shine_mode != SHINE_HSV)
    {
      gtk_widget_set_sensitive (spinbutton, FALSE);
      gtk_widget_set_sensitive (label, FALSE);
    }
  //g_object_set_data (G_OBJECT (widget_shine_hsv), "set_sensitive", spinbutton);


  /* 2: level */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.shine_level,
                                     0.0, 100.0, 1.0, 10.0, 0, 1, 1);
  fblur_widgets[FBLUR_WIDGET_LEVEL] = spinbutton;
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, 2,
                                     _("Level:"), 0.0, 0.5,
                                     spinbutton, 1, TRUE);
  fblur_widgets[FBLUR_WIDGET_LEVEL_LABEL] = label;

  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify rate to shine in percentage."), NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.shine_level);
  PREVIEW_INVALIDATE (adj, "value_changed");


  /* 3: curve */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.shine_curve,
                                     0.0, 8.0, 0.1, 1.0, 0, 1, 2);
  fblur_widgets[FBLUR_WIDGET_CURVE] = spinbutton;
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, 3,
                                     _("Curve:"), 0.0, 0.5,
                                     spinbutton, 1, TRUE);
  fblur_widgets[FBLUR_WIDGET_CURVE_LABEL] = label;

  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify value of curve to shine. The normal value (direct proportion) is 1.0 ."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.shine_curve);
  PREVIEW_INVALIDATE (adj, "value_changed");


  /* 4: white glow color brush */
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.whiteglow,
                                     0.0, 100.0, 1.0, 10.0, 0, 1, 1);
  fblur_widgets[FBLUR_WIDGET_WHITEGLOW] = spinbutton;
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, 4,
                                     _("White glow:"), 0.0, 0.5,
                                     spinbutton, 1, TRUE);
  fblur_widgets[FBLUR_WIDGET_WHITEGLOW_LABEL] = label;

  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("Specify white glowing lovel in percentage. It is used with color brush."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.whiteglow);
  PREVIEW_INVALIDATE (adj, "value_changed");

  gtk_widget_set_sensitive (spinbutton,
                            (fblur_vals.model == MODEL_BRUSH &&
                             fblur_brush && fblur_brush->is_color));
  gtk_widget_set_sensitive (label,
                            (fblur_vals.model == MODEL_BRUSH &&
                             fblur_brush && fblur_brush->is_color));


  /* reset button for level and curve */

  button = gtk_button_new_with_mnemonic (_("_Reset"));
  fblur_widgets[FBLUR_WIDGET_RESET_LEVEL_CURVE] = button;

  align = gtk_alignment_new (0.0, 1.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), button);
  gtk_table_attach (GTK_TABLE (table), align, 2, 4, 2, 4,
                    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_size_group_add_widget (size_group, button);
  gimp_help_set_help_data (button, _("It resets level and curve."), NULL);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_level_reset), fblur_widgets);


  /* mode */
  GtkWidget *widget_shine_mode;
  GtkWidget *widget_shine_hsv;
  GtkWidget *widget_shine_map;

  radio = gimp_int_radio_group_new
    (FALSE, NULL, G_CALLBACK (shine_radio_button_update),
     drawable, fblur_vals.shine_mode,
     _("Intensity"), SHINE_INTENSITY, &widget_shine_mode,
     _("HSV"), SHINE_HSV, &widget_shine_hsv,
     _("Map"), SHINE_MAP, &widget_shine_map,
     NULL);
  fblur_widgets[FBLUR_WIDGET_SHINE_MODE] = widget_shine_mode;

  gtk_table_attach (GTK_TABLE (table), radio, 3, 4, 0, 3,
                    GTK_SHRINK, GTK_FILL, 0, 0);
  gimp_help_set_help_data (widget_shine_mode,
                           _("It makes lighting pixels by intensity of image. This value is same as color value, 1 .. 255."),
                           NULL);
  gimp_help_set_help_data (widget_shine_hsv,
                           _("It computes light with HSV. it not works with gray image or when a value of saturation is 0%. Specified a light value is used as the value in HSV. (its range is same, 1 .. 255)"),
                           NULL);
  gimp_help_set_help_data (widget_shine_map,
                           _("It makes lighting pixels from selected image. An image must be in same size, gray scale and no alpha channel."),
                           NULL);
  //PREVIEW_INVALIDATE (widget_shine_mode, "toggled");
  PREVIEW_INVALIDATE (widget_shine_hsv, "toggled");
  PREVIEW_INVALIDATE (widget_shine_map, "toggled");

  /* sensitive for shine */
  gtk_widget_set_sensitive (widget_shine_hsv, is_rgb);


  /* box of erase white */
  vbox = gtk_vbox_new (FALSE, 3);
  fblur_widgets[FBLUR_WIDGET_ERASE_WHITE_BOX] = vbox;
  gtk_box_set_spacing (GTK_BOX (vbox), 1);
  gtk_box_pack_start (GTK_BOX (nb4), vbox, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (vbox, fblur_vals.use_shine);
  g_object_set_data (G_OBJECT (shine_sensitive), "set_sensitive", vbox);
  shine_sensitive = vbox;

  /* switch of erase white */
  toggle = gtk_check_button_new_with_label (_("Erase wide area:"));
  fblur_widgets[FBLUR_WIDGET_ERASE_WHITE] = toggle;
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.erase_white);
  gimp_help_set_help_data (toggle,
                           _("If yow want to erase a wide shining area as just a white object, check this item and adjust an area with below parameter."),
			     NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (gimp_toggle_button_update),
                    &fblur_vals.erase_white);
  PREVIEW_INVALIDATE (toggle, "toggled");

  /* threshold for erase white */
  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (align), hbox);

  label = gtk_label_new (_("Threshold:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.erase_white_val,
                                     0.0, 10.0, 0.1, 1.0, 0.0, 1.0, 2);
  fblur_widgets[FBLUR_WIDGET_ERASE_WHITE_VAL] = spinbutton;
  gtk_box_pack_end (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("This is threshold value about deciding area that is not added for shining. Continuous wide area that is over than this threshold will be excepted as just a white object."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.erase_white_val);
  PREVIEW_INVALIDATE (adj, "value_changed");

  /* sensitive for erase white */
  gtk_widget_set_sensitive (hbox, fblur_vals.erase_white);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", hbox);


  /* ---------- nb7: Utility ---------- */

  /* store params */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 1);
  gtk_box_pack_start (GTK_BOX (nb7), vbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Store current parameters:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);

  button = gtk_button_new_with_label (_("Store"));
  gtk_size_group_add_widget (size_group, button);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gimp_help_set_help_data (button,
                           _("This command just put values for non-interactive. So it does not make any copies."),
                           NULL);

  entry = gtk_entry_new ();
  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (align), entry);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_vals_store), entry);

  /* export shine */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 1);
  gtk_box_pack_start (GTK_BOX (nb7), vbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Export shine level as new image:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);

  button = gtk_button_new_with_label (_("Export"));
  gtk_size_group_add_widget (size_group, button);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gimp_help_set_help_data (button,
			   _("It makes a image, that is able to confirm shining pixels, or to be a base for applying other filter. The image is fit for shine map. You can import a processed image again. If it is not shown in select window, check size, mode, and alpha channel."),
			   NULL);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_shine_as_new), drawable);

  gtk_widget_set_sensitive (vbox, fblur_vals.use_shine);
  g_object_set_data (G_OBJECT (shine_sensitive), "set_sensitive", vbox);


  /* focus animation */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (nb7), vbox, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (vbox,
			    gimp_drawable_is_layer (drawable->drawable_id));

  toggle = gtk_check_button_new_with_label (_("Make focus changes:"));
  fblur_widgets[FBLUR_WIDGET_FOCUS_ANIM] = toggle;
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				fblur_vals.focus_anim);
  gimp_help_set_help_data (toggle,
			   _("It makes new layers as animation with changing focal distance. An original layer will be kept."),
			   NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (gimp_toggle_button_update),
		    &fblur_vals.focus_anim);

  /* focus animation: box */
  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 16, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);

  table = gtk_table_new (5, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 3);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 3);
  gtk_container_add (GTK_CONTAINER (align), table);
  gtk_widget_set_sensitive (table, fblur_vals.focus_anim);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", table);

  /* focus animation: start point */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("Start:"), /* SCALE_WIDTH - 16 */ 0, 0,
                              fblur_vals.focus_start, 0, COLORMAX, 1, 10, 0,
                              TRUE, 0, 0,
                              _("Start point."),
                              NULL);
  spinbutton = GIMP_SCALE_ENTRY_SPINBUTTON (adj);
  fblur_widgets[FBLUR_WIDGET_FOCUS_START] = spinbutton;
  gtk_size_group_add_widget (size_group, spinbutton);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.focus_start);

  /* focus animation: end point */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                              _("End:"), /* SCALE_WIDTH - 16 */ 0, 0,
                              fblur_vals.focus_end, 0, COLORMAX, 1, 10, 0,
                              TRUE, 0, 0,
                              _("End point."),
                              NULL);
  spinbutton = GIMP_SCALE_ENTRY_SPINBUTTON (adj);
  fblur_widgets[FBLUR_WIDGET_FOCUS_END] = spinbutton;
  gtk_size_group_add_widget (size_group, spinbutton);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.focus_end);

  /* focus animation: focus curve */
  label = gtk_label_new (_("Curve:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);

  gtk_table_attach (GTK_TABLE (table), align, 0, 2, 2, 3,
		    GTK_FILL, GTK_SHRINK, 0, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.focus_curve,
                                     0.0, 8.0, 0.1, 1.0, 0.0, 1.0, 2);
  fblur_widgets[FBLUR_WIDGET_FOCUS_CURVE] = spinbutton;
  gtk_table_attach (GTK_TABLE (table), spinbutton, 2, 3, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  //gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("It is value of curve to make high and low on movement."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &fblur_vals.focus_curve);

  /* focus animation: waiting time */
  label = gtk_label_new (_("Wait:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);

  gtk_table_attach (GTK_TABLE (table), align, 0, 2, 3, 4,
		    GTK_FILL, GTK_SHRINK, 0, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.focus_wait,
                                     0, 8000, 100, 1000, 0.0, 1.0, 0);
  fblur_widgets[FBLUR_WIDGET_FOCUS_WAIT] = spinbutton;
  gtk_table_attach (GTK_TABLE (table), spinbutton, 2, 3, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  //gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("It is a waiting time for one frame in milli-second. When a focal distance is not changed (it is in integer), this wait is accumulated."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.focus_wait);

  /* focus animation: frame number */
  label = gtk_label_new (_("Frames:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);

  gtk_table_attach (GTK_TABLE (table), align, 0, 2, 4, 5,
		    GTK_FILL, GTK_SHRINK, 0, 0);
  spinbutton = gimp_spin_button_new (&adj, fblur_vals.focus_div,
                                     2, 100, 1, 10, 0.0, 1.0, 0);
  fblur_widgets[FBLUR_WIDGET_FOCUS_DIV] = spinbutton;
  gtk_table_attach (GTK_TABLE (table), spinbutton, 2, 3, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);
  //gtk_size_group_add_widget (size_group, spinbutton);
  gimp_help_set_help_data (spinbutton,
                           _("It is a number of frames, divides changing fucal distance. When a focal distance is not changed (it is in integer), a number of frames is reduced."),
                           NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fblur_vals.focus_div);

  /* report type */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 1);
  gtk_box_pack_start (GTK_BOX (nb7), vbox, FALSE, FALSE, 0);

  toggle =
    gtk_check_button_new_with_label (_("Report processing time with popup:"));
  fblur_widgets[FBLUR_WIDGET_REPORT_TIME_WIN] = toggle;

  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.report == REPORT_TIME_WIN);
  gimp_help_set_help_data (toggle,
                           _("Reports processing time and other conditions. This way notes into message dialog."),
                           NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (report_button_update),
                    GINT_TO_POINTER (REPORT_TIME_WIN));

  toggle =
    gtk_check_button_new_with_label (_("Report processing time with console:"));
  fblur_widgets[FBLUR_WIDGET_REPORT_TIME_CON] = toggle;

  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                fblur_vals.report == REPORT_TIME_CON);
  gimp_help_set_help_data (toggle,
                           _("Reports processing time and other conditions. This way notes into standard out."),
                           NULL);
  g_signal_connect (toggle, "toggled", G_CALLBACK (report_button_update),
                    GINT_TO_POINTER (REPORT_TIME_CON));


  /* reset params */

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_end (GTK_BOX (nb7), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Reset all parameters:"));
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  button = gtk_button_new_with_mnemonic (_("_Reset"));
  gtk_size_group_add_widget (size_group, button);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_vals_init), fblur_widgets);

  /* ---------- nb9: about this plug-in ---------- */

  /* This infomation is prepared for user support.
   * A user might get only compiled binary without infomation.
   * And a binary might be redistributed over again.
   */

  GtkWidget *about = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (about), TRUE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (about), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (about), FALSE);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (about), 2);

  GtkTextBuffer *buffer =
    gtk_text_view_get_buffer (GTK_TEXT_VIEW (about));

  gtk_text_buffer_set_text (buffer, "", 0);
  GtkTextIter text_iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &text_iter, 0);

  gtk_text_buffer_create_tag (buffer, "TITLE",
                              "weight", PANGO_WEIGHT_BOLD,
                              "size", 12 * PANGO_SCALE, NULL);

  gtk_text_buffer_create_tag (buffer, "SIGNATURE",
                              "style", PANGO_STYLE_ITALIC,
                              "size", 8 * PANGO_SCALE,
                              "left_margin", 10, NULL);

  gtk_text_buffer_create_tag (buffer, "TEXT", "size", 10 * PANGO_SCALE, NULL);

  gtk_text_buffer_create_tag (buffer, "ITEM",
                              "weight", PANGO_WEIGHT_BOLD,
                              "size", 9 * PANGO_SCALE,
                              "pixels_above_lines", 10, NULL);

  gtk_text_buffer_create_tag (buffer, "DETAIL",
                              "size", 8 * PANGO_SCALE,
                              "left_margin", 10, NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Focus Blur plug-in ", -1, "TITLE", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "version " PLUG_IN_VERSION "\n", -1, "TEXT", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Copyright © 2002-2006, Kyoichiro Suda\n",
     -1, "SIGNATURE", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Homepage:\n", -1, "ITEM", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "http://sudakyo.hp.infoseek.co.jp\n", -1, "DETAIL", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Author:\n", -1, "ITEM", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter,
     "Kyoichiro Suda <sudakyo\100fat.coara.or.jp>\n", -1, "DETAIL", NULL);

#ifdef DISTRIBUTER
  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Distributer:\n", -1, "ITEM", NULL);
  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, DISTRIBUTER "\n", -1, "DETAIL", NULL);
#endif

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter, "Compile options:\n", -1, "ITEM", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter,
#if (FBLUR_PREVIEW == 1 || FBLUR_PREVIEW == 2)
#  if (FBLUR_PREVIEW == 1)
     "Preview support:\tFblur\n"
#  else
     "Preview support:\tGimp 2.2\n"
#  endif
#else
     "Preview support:\tNo\n"
#endif
     , -1, "DETAIL", NULL);

  gtk_text_buffer_insert_with_tags_by_name
    (buffer, &text_iter,
#ifdef ENABLE_NLS
     "NLS support:\tYes\n"
#else
     "NLS support:\tNo\n"
#endif
     , -1, "DETAIL", NULL);

  gtk_widget_modify_text (about, GTK_STATE_NORMAL,
                          &about->style->fg[GTK_STATE_NORMAL]);
  gtk_widget_modify_base (about, GTK_STATE_NORMAL,
                          &about->style->bg[GTK_STATE_NORMAL]);

  gtk_frame_set_shadow_type (GTK_FRAME (nb9), GTK_SHADOW_IN);

  vpaned = gtk_vpaned_new ();
  gtk_container_add (GTK_CONTAINER (nb9), vpaned);
  scrlwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrlwin),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_paned_add1 (GTK_PANED (vpaned), scrlwin);
  gtk_container_add (GTK_CONTAINER (scrlwin), about);


  /* ---------- ---------- */


  /* finish */
  gtk_widget_show_all (dlg);

  gboolean run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);
#if (FBLUR_PREVIEW == 2)
  update_preview_free ();
#endif
#if (FBLUR_PREVIEW == 1)
  fblur_preview_free (preview);
#endif

  gtk_widget_destroy (dlg);

  return run;
}

static void
dialog_vals_init (GtkButton  *button,
                  GtkWidget **widgets)
{
  GtkWidget *w;
  gboolean active;

  w = fblur_widgets[FBLUR_WIDGET_MODEL];
  gtk_combo_box_set_active (GTK_COMBO_BOX (w), fblur_init_vals.model);

  w = widgets[FBLUR_WIDGET_RADIUS];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.radius);

  w = widgets[FBLUR_WIDGET_SOFTNESS];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.softness);

  w = widgets[FBLUR_WIDGET_FILL];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.fill);

  w = widgets[FBLUR_WIDGET_BSCALE];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.bscale);

  w = fblur_widgets[FBLUR_WIDGET_USE_MODELADJ];
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                fblur_init_vals.use_modeladj);

  w = fblur_widgets[FBLUR_WIDGET_DIFF];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.diff);

  w = fblur_widgets[FBLUR_WIDGET_DIRECTION];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.direction);

  w = fblur_widgets[FBLUR_WIDGET_FOCUS];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.focus);

  w = fblur_widgets[FBLUR_WIDGET_USE_MAP];
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                fblur_init_vals.use_map);

  w = fblur_widgets[FBLUR_WIDGET_NEAR];
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (w),
                                   fblur_init_vals.near);

  w = fblur_widgets[FBLUR_WIDGET_DIST_MODE];
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (w),
                                   fblur_init_vals.dist_mode);

  w = fblur_widgets[FBLUR_WIDGET_USE_SHINE];
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                fblur_init_vals.use_shine);

  w = fblur_widgets[FBLUR_WIDGET_SHINE_MODE];
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (w),
                                   fblur_init_vals.shine_mode);

  w = fblur_widgets[FBLUR_WIDGET_LIGHT];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.light);

  w = fblur_widgets[FBLUR_WIDGET_SATURATION];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.saturation);

  w = fblur_widgets[FBLUR_WIDGET_LEVEL];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                             fblur_init_vals.shine_level);

  w = fblur_widgets[FBLUR_WIDGET_CURVE];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                             fblur_init_vals.shine_curve);

  w = fblur_widgets[FBLUR_WIDGET_WHITEGLOW];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.whiteglow);

  w = fblur_widgets[FBLUR_WIDGET_ERASE_WHITE];
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				fblur_init_vals.erase_white);

  w = fblur_widgets[FBLUR_WIDGET_ERASE_WHITE_VAL];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
			     fblur_init_vals.erase_white_val);

  w = fblur_widgets[FBLUR_WIDGET_FOCUS_ANIM];
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				fblur_init_vals.focus_anim);

  w = fblur_widgets[FBLUR_WIDGET_FOCUS_START];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.focus_start);

  w = fblur_widgets[FBLUR_WIDGET_FOCUS_END];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.focus_end);

  w = fblur_widgets[FBLUR_WIDGET_FOCUS_DIV];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), fblur_init_vals.focus_div);

  w = fblur_widgets[FBLUR_WIDGET_REPORT_TIME_WIN];
  active = fblur_init_vals.report == REPORT_TIME_WIN;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), active);

  w = fblur_widgets[FBLUR_WIDGET_REPORT_TIME_CON];
  active = fblur_init_vals.report == REPORT_TIME_CON;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), active);

  return;
}

static void
dialog_diff_reset (GtkButton  *button,
		   GtkWidget **widgets)
{
  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIFF]), fblur_init_vals.diff);

  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIRECTION]),
     fblur_init_vals.direction);
}

static void
dialog_level_reset (GtkButton  *button,
                    GtkWidget **widgets)
{
  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_LEVEL]),
     fblur_init_vals.shine_level);

  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_CURVE]),
     fblur_init_vals.shine_curve);
}

static void
dialog_vals_store (GtkButton *button,
                   GtkWidget *entry)
{
  gchar *text = g_strdup_printf
    ("%s image drawable %d %.2f %.1f %.1f %.1f %.1f %.2f %d %s %d %d %.1f %.1f %.2f %.2f %.1f",
     PLUG_IN_NAME,
     fblur_vals.model,
     fblur_vals.radius,
     fblur_vals.softness,
     (fblur_vals.model == MODEL_RING ||
      fblur_vals.model == MODEL_CONCAVE) ? fblur_vals.fill : 0.0,
     (fblur_vals.model == MODEL_BRUSH) ? fblur_vals.bscale : 0.0,
     fblur_vals.diff,
     fblur_vals.direction,
     fblur_vals.focus,
     fblur_vals.use_map ? "map" : "-1",
     fblur_vals.dist_mode,
     (fblur_vals.use_shine &&
      (fblur_vals.shine_mode == SHINE_INTENSITY ||
       fblur_vals.shine_mode == SHINE_HSV)) ? fblur_vals.light : -1,
     (fblur_vals.shine_mode == SHINE_HSV) ? fblur_vals.saturation : -1,
     fblur_vals.shine_level,
     fblur_vals.shine_curve,
     fblur_vals.erase_white ? fblur_vals.erase_white_val : 0.0,
     (fblur_vals.model == MODEL_BRUSH) ? fblur_vals.whiteglow : 0.0);

  gtk_entry_set_text (GTK_ENTRY (entry), text);
  g_free (text);
}


static void
dialog_center_to_diff (GtkButton  *button,
		       GtkWidget **widgets)
{
  if (fblur_vals.model != MODEL_BRUSH)
    goto dialog_center_to_diff_fail;

  g_assert (fblur_brush);

  guint8  *data   = fblur_brush->data;
  gint     width  = fblur_brush->width;
  gint     height = fblur_brush->height;

  if (width <= 1 && height <= 1)
    goto dialog_center_to_diff_fail;

  gdouble  sum, sumb, half;
  gdouble  hx = 0, hy = 0;
  gint     i, x, y;

  half = 0;
  for (i = 0; i < width * height; i ++)
    half += data[i];
  half /= 2;

  sum = sumb = 0;
  for (y = 0; y < height; y ++)
    for (x = 0; x < width; x ++)
      {
        gint o = y * width + x;
        sum += data[o];
        if (sum >= half)
          {
            hy = y - 0.5 * height + 1 - (sum - half) / (sum - sumb);
            y = height; /* break */
            break;
          }
        sumb = sum;
      }

  sum = sumb = 0;
  for (x = 0; x < width; x ++)
    for (y = 0; y < height; y ++)
      {
        gint o = y * width + x;
        sum += data[o];
        if (sum >= half)
          {
            hx = x - 0.5 * width + 1 - (sum - half) / (sum - sumb);
            x = width; /* break */
            break;
          }
        sumb = sum;
      }

  gdouble blen, glen, rad, hour;
  glen = hypot (hx, hy);
  if (glen < 0.0000001)
    goto dialog_center_to_diff_fail;

  blen = hypot (width - 1, height - 1) / 2;
  rad  = asin (hy / glen);
  hour = 3 - 3 * 2 / G_PI * rad;
  if (hx > 0)
    hour = 12 - hour;

  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIFF]), 100.0 * glen / blen);
  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIRECTION]), hour);

  return;

 dialog_center_to_diff_fail:
  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIFF]), 0);
  gtk_spin_button_set_value
    (GTK_SPIN_BUTTON (widgets[FBLUR_WIDGET_DIRECTION]), 0);
  return;
}


static void
dialog_shine_as_new (GtkButton    *button,
                     GimpDrawable *src_d)
{
  gint32 image_ID = shine_as_new (src_d);

  if (image_ID == -1)
    return;

  gimp_display_new (image_ID);
  gimp_displays_flush ();
}

static gint32
shine_as_new (GimpDrawable *src_d)
{
  ImageBuffer *srcimg;
  ShineBuffer *shine;

  gint32         image_ID, layer_ID;
  GimpDrawable  *drawable;
  GimpPixelRgn   pr;
  gpointer       p;
  guchar        *dlp, *dp;
  gint           x, y, n;
  gdouble        val;
  gint           col;

  srcimg = fblur_update_srcimg (src_d);
   if (! srcimg)
    return -1;

  shine  = fblur_update_shine (srcimg);
  if (! shine)
    return -1;

  image_ID = gimp_image_new (shine->w, shine->h, GIMP_GRAY);
  gimp_image_undo_disable (image_ID);

  layer_ID = gimp_layer_new (image_ID, _("Shine level"),
                             shine->w, shine->h, GIMP_GRAY_IMAGE,
                             100.0, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_ID, layer_ID, 0);

  drawable = gimp_drawable_get (layer_ID);
  gimp_pixel_rgn_init (&pr, drawable, 0, 0, shine->w, shine->h, TRUE, TRUE);

  for (p = gimp_pixel_rgns_register (1, &pr);
       p != NULL; p = gimp_pixel_rgns_process (p))
    {
      dlp = pr.data;
      for (y = pr.y; y < pr.y + pr.h; y ++)
        {
          dp = dlp;
          for (x = pr.x, n = 0; n < pr.w; x ++, n ++)
            {
              val = shine_get (shine, x, y);
              col = RINT (val * 255);
              dp[n] = CLAMP0255 (col);
            }
          dlp += pr.rowstride;
        }
    }

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (layer_ID, FALSE);
  gimp_drawable_update (layer_ID, 0, 0, shine->w, shine->h);
  gimp_drawable_detach (drawable);
  gimp_image_clean_all (image_ID);
  gimp_image_undo_enable (image_ID);

  return image_ID;
}


static gint
mapmenu_constraint (gint32   image_id,
                    gint32   drawable_id,
                    gpointer data)
{
  if (drawable_id == -1)
    return FALSE;

  return (gimp_drawable_is_rgb (drawable_id) ||
          gimp_drawable_is_gray (drawable_id));
}

static void
model_combobox_callback (GtkComboBox *combobox,
                         gpointer     user_data)
{
  gboolean t;

  fblur_vals.model = gtk_combo_box_get_active (combobox);

  t = fblur_vals.model != MODEL_BRUSH;
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_RADIUS], t);

  t = fblur_vals.model == MODEL_BRUSH && fblur_brush && fblur_brush->is_color;
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_WHITEGLOW], t);
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_WHITEGLOW_LABEL], t);

  t = fblur_vals.model == MODEL_RING || fblur_vals.model == MODEL_CONCAVE;
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_FILL_BOX], t);

  t = fblur_vals.model == MODEL_BRUSH;
  gtk_widget_set_sensitive (fblur_widgets[FBLUR_WIDGET_BSCALE_BOX], t);
}


static void
report_button_update (GtkToggleButton *toggle,
                      gpointer         user_data)
{
  GtkToggleButton *toggle2 = NULL;
  gint report = GPOINTER_TO_INT (user_data);

  if (report == REPORT_TIME_WIN)
    toggle2 = GTK_TOGGLE_BUTTON (fblur_widgets[FBLUR_WIDGET_REPORT_TIME_CON]);
  else if (report == REPORT_TIME_CON)
    toggle2 = GTK_TOGGLE_BUTTON (fblur_widgets[FBLUR_WIDGET_REPORT_TIME_WIN]);
  else
    g_assert_not_reached ();

  if (gtk_toggle_button_get_active (toggle))
    {
      if (gtk_toggle_button_get_active (toggle2))
        gtk_toggle_button_set_active (toggle2, FALSE);
      fblur_vals.report = report;
    }
  else
    fblur_vals.report = REPORT_NONE;
}


/*---- Preview ----*/

#if (FBLUR_PREVIEW == 1 || FBLUR_PREVIEW == 2)

static ImageBuffer*
img_dup_crop (ImageBuffer *img,
              guint        x,
              guint        y,
              guint        width,
              guint        height)
{
  ImageBuffer *crop = img_allocate (img->bpp, width, height);
  COLORBUF    *srcp;
  COLORBUF    *destp;
  guint        x2, y2, w2, h2;
  guint        x3, y3;
  guint        tmp;
  gint         croplen, nlines;

  crop->x = x;
  crop->y = y;
  crop->ch = img->ch;
  crop->is_rgb = img->is_rgb;
  crop->has_alpha = img->has_alpha;
  memset (crop->data, 0, crop->length);

  x2 = x - img->x;
  y2 = y - img->y;
  w2 = width;
  h2 = height;
  x3 = 0;
  y3 = 0;

  tmp = img->x + img->w - x;
  if (w2 > tmp)
    w2 = tmp;
  tmp = img->y + img->h - y;
  if (h2 > tmp)
    h2 = tmp;

  if (x < img->x)
    {
      tmp = img->x - x;
      x3  = tmp;
      w2 -= tmp;
      x2  = 0;
    }
  if (y < img->y)
    {
      tmp = img->y - y;
      y3  = tmp;
      h2 -= tmp;
      y2  = 0;
    }
  g_assert (w2 > 0 && h2 > 0);

  srcp  = &( img->data[ img->rowstride * y2 +  img->bpp * x2]);
  destp = &(crop->data[crop->rowstride * y3 + crop->bpp * x3]);

  croplen = w2 * img->bpp;
  for (nlines = h2; nlines; nlines --)
    {
      memcpy (destp, srcp, croplen);
      srcp += img->rowstride;
      destp += crop->rowstride;
    }

  return crop;
}

static void
focus_point (guint32    map_id,
             gint       x,
             gint       y,
             gboolean   invert)
{
  gint       w, h;
  gint       num_channels;
  guint8    *p;
  GtkWidget *widget;

  g_assert (fblur_vals.use_map);
  g_assert (fblur_vals.map_id != 1);

  w = gimp_drawable_width  (fblur_vals.map_id);
  h = gimp_drawable_height (fblur_vals.map_id);

  /* tile */
  while (x >= w)
    x -= w;
  while (x < 0)
    x += w;
  while (y >= h)
    y -= h;
  while (y < 0)
    y += h;

  p = gimp_drawable_get_pixel (map_id, x, y, &num_channels);
  if (gimp_drawable_is_rgb (map_id))
    p[0] = RINT (GIMP_RGB_INTENSITY (p[0], p[1], p[2]));

  if (invert)
    p[0] = COLORMAX - p[0];

  /* alpha is ignored */

  widget = fblur_widgets[FBLUR_WIDGET_FOCUS];
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), p[0]);

  g_free (p);
}

#endif

#if (FBLUR_PREVIEW == 2)

static void
update_preview_free (void)
{
  if (fblur_preview.idle)
    {
      g_source_remove (fblur_preview.idle);
      fblur_preview.idle = 0;
    }

  if (fblur_preview.idle_draw)
    {
      g_source_remove (fblur_preview.idle_draw);
      fblur_preview.idle = 0;
    }

  if (fblur_preview.dest)
    {
      img_free (fblur_preview.dest);
      fblur_preview.dest = NULL;
    }

  fblur_preview.com_line = 0;
  fblur_preview.draw_line = 0;
}

static void
update_preview_draw (gpointer data)
{
  g_assert (fblur_preview.dest);

  gint x, y;
  gimp_preview_get_position (fblur_preview.preview, &x, &y);

  if (x != fblur_preview.dest->x || y != fblur_preview.dest->y)
    {
      update_preview_free ();
      return;
    }

  if (fblur_preview.com_line == fblur_preview.draw_line)
    return;

  gimp_preview_draw_buffer (fblur_preview.preview, fblur_preview.dest->data,
                            fblur_preview.dest->rowstride);
  fblur_preview.draw_line = fblur_preview.com_line;
}

static void
update_preview_progress (gpointer data)
{
  g_assert (fblur_preview.com_line < fblur_preview.dest->h);

  gint y = fblur_preview.dest->y + fblur_preview.com_line;
  gint x2 = fblur_preview.dest->x + fblur_preview.dest->w;

  COLORBUF *dp = &(fblur_preview.dest->data
                   [fblur_preview.dest->rowstride * fblur_preview.com_line]);
  gint x = fblur_preview.dest->x;
  for (; x < x2; x ++)
    {
      focus_blur_pixel (dp, fblur_preview.src, x, y, fblur_preview.dtable,
                        fblur_preview.model, fblur_preview.focus,
                        fblur_preview.dist_mode, fblur_preview.map,
                        fblur_preview.shine, fblur_preview.use_color,
                        fblur_preview.whiteglow);
      dp += fblur_preview.dest->bpp;
    }

  fblur_preview.com_line ++;

  if (fblur_preview.com_line == fblur_preview.dest->h)
    {
      gimp_preview_draw_buffer (fblur_preview.preview,
                                fblur_preview.dest->data,
                                fblur_preview.dest->rowstride);
      update_preview_free ();
    }
}

static void
update_preview_init (GimpPreview  *preview,
                     GimpDrawable *drawable)
{
  update_preview_free ();

  if ((fblur_vals.model != MODEL_BRUSH && fblur_vals.radius <= 0.0) ||
      (fblur_vals.model == MODEL_BRUSH && fblur_vals.bscale <= 0.0) ||
      ((! fblur_vals.use_map) && fblur_vals.focus <= 0))
    return;

  fblur_preview.preview = preview;
  fblur_preview.model = fblur_vals.model;

  gdouble diff_x, diff_y;
  if (fblur_vals.use_modeladj)
    hour_to_coordinates (&diff_x, &diff_y, fblur_vals.direction);
  else
    diff_x = diff_y = 0;

  fblur_preview.src       = fblur_update_srcimg (drawable);
  fblur_preview.map       = fblur_update_map (FALSE);
  fblur_preview.dtable    = dtable_update (FALSE);
  fblur_preview.shine     = fblur_update_shine (fblur_preview.src);
  fblur_preview.focus     = fblur_vals.focus;
  fblur_preview.dist_mode = (fblur_preview.map
                             ) ? fblur_vals.dist_mode : DWEIGHT_NONE;
  fblur_preview.use_color = (fblur_preview.src->is_rgb &&
                             fblur_preview.dtable->model == MODEL_BRUSH &&
                             fblur_preview.dtable->color &&
                             fblur_preview.shine);
  fblur_preview.whiteglow = (fblur_vals.whiteglow / 100) - 1.0;

  gint x, y;
  gint w, h;
  gimp_preview_get_position (fblur_preview.preview, &x, &y);
  gimp_preview_get_size (fblur_preview.preview, &w, &h);

  fblur_preview.dest = img_dup_crop (fblur_preview.src, x, y, w, h);

  fblur_preview.idle = g_idle_add_full
    (G_PRIORITY_LOW + 10, (GSourceFunc) update_preview_progress, NULL, NULL);

  fblur_preview.idle_draw = g_timeout_add_full
    (G_PRIORITY_LOW, 250, (GSourceFunc) update_preview_draw, NULL, NULL);
}

static void
update_preview_event    (GtkWidget      *area,
                         GdkEvent       *event,
                         gpointer        data)
{
  GdkEventButton *b = (GdkEventButton *) event;
  gint x, y;

  if (b->button != 2)
    return;

  if (! fblur_vals.use_map)
    return;
  g_return_if_fail (fblur_vals.map_id != -1);

  x = GIMP_PREVIEW_AREA(area)->offset_x + event->button.x;
  y = GIMP_PREVIEW_AREA(area)->offset_y + event->button.y;

  focus_point (fblur_vals.map_id, x, y, fblur_vals.near);
}

#endif

#if (FBLUR_PREVIEW == 1)

/* Image manipulation */

static ImageBuffer*
img_duplicate (ImageBuffer *img)
{
  gsize len = sizeof (ImageBuffer) - 1 + img->length;
  ImageBuffer *cp = g_memdup (img, len);

  return cp;
}

static ImageBuffer*
img_dup_noalpha (ImageBuffer *img,
                 CheckValues *check)
{
  g_assert (img->has_alpha);

  gint bpp = img->ch;
  ImageBuffer *noa = img_allocate (bpp, img->w, img->h);

  noa->x = img->x;
  noa->y = img->y;
  noa->is_rgb = img->is_rgb;

  COLORBUF *srcp  = img->data;
  COLORBUF *destp = noa->data;

  guint x, y, b;
  for (y = 0; y < img->h; y ++)
    {
      for (x = 0; x < img->w; x ++)
        {
          gint bgsqr = ((x ^ y) >> check->shift) & 1;

          gint ab = (gint) srcp[bpp];
          gint bb = COLORMAX - ab;
          gint bc = bb * check->c[bgsqr];
          for (b = 0; b < bpp; b ++)
            {
              gint v = (ab * srcp[b] + bc + COLORMAX/2) / COLORMAX;
              destp[b] = CLAMP (v, 0, COLORMAX);
            }

          srcp += img->bpp;
          destp += bpp;
        }
    }

  return noa;
}

static ImageBuffer*
make_inactive_img (ImageBuffer *img,
                   GtkWidget   *widget)
{
  COLORBUF bg[4];
  g_assert (sizeof bg[0] == 1);
  bg[0] = widget->style->bg[GTK_STATE_INSENSITIVE].red >> 8;
  bg[1] = widget->style->bg[GTK_STATE_INSENSITIVE].green >> 8;
  bg[2] = widget->style->bg[GTK_STATE_INSENSITIVE].blue >> 8;

  guint pixel = img->w * img->h;
  gboolean has_alpha = img->has_alpha;
  gint bpp = img->bpp - (has_alpha ? 1 : 0);

  ImageBuffer *inactive = img_allocate (bpp, img->w, img->h);
  inactive->x = img->x;
  inactive->y = img->y;
  inactive->is_rgb = img->is_rgb;

  COLORBUF *srcp = img->data;
  COLORBUF *destp = inactive->data;

  guint p, b;

  if (has_alpha)
    {
      for (p = 0; p < pixel; p ++)
        {
          gint ab = (gint) srcp[bpp] / 2;
          gint bb = COLORMAX - ab;
          for (b = 0; b < bpp; b ++)
            {
              gint v = (ab * srcp[b] + bb * bg[b] + COLORMAX/2) / COLORMAX;
              destp[b] = CLAMP (v, 0, COLORMAX);
            }

          srcp += img->bpp;
          destp += bpp;
        }
    }
  else
    {
      for (p = 0; p < pixel; p ++)
        {
          for (b = 0; b < bpp; b ++)
            destp[b] = (srcp[b] + bg[b]) / 2;
          srcp += bpp;
          destp += bpp;
        }
    }

  return inactive;
}

static guint
thumbnail_progress_update (FblurPreviewNavigate *navi)
{
  g_assert (navi->img);
  g_assert (navi->img->data);

  ImageBuffer   *src    = navi->parent->img;
  ImageBuffer   *dest   = navi->img;
  CheckValues   *check  = navi->parent->check;

  guint          width  = dest->w;
  guint          height = dest->h;
  gdouble        scale_x = (gdouble) width / (gdouble) src->w;
  gdouble        scale_y = (gdouble) height / (gdouble) src->h;

  gint           bpp    = src->bpp;
  gint           ch     = src->ch;
  gboolean       has_alpha = src->has_alpha;

  guint          l      = navi->com_line;
  gdouble        run_y  = l * scale_y;
  guint          fill_y = floor (run_y);
  COLORBUF      *ip     = &(src->data[src->rowstride * l]);
  COLORBUF      *dp     = &(dest->data[dest->rowstride * fill_y]);

  guint          len    = bpp * width;
  gdouble        val[len];
  gdouble       *vp     = val;
  guint          b;
  gdouble        pre_y, across_y, val_y;

  gdouble        run_x = 0;
  guint          fill_x = 0;
  guint          x;

  for (b = 0; b < bpp; b ++)
    val[b] = 0;

  for (x = 0; x < src->w; x ++, ip += src->bpp)
    {
      gdouble pre_x, across_x, val_x;

      pre_x = run_x;
      run_x += scale_x;
      across_x = run_x - fill_x;

      if (across_x <= 1)
        val_x = scale_x;
      else
        val_x = (fill_x + 1) - pre_x;

      if (has_alpha)
        {
          gdouble a = val_x * ip[ch] / COLORMAX;
          for (b = 0; b < ch; b ++)
            vp[b] += a * ip[b];
          vp[b] += a;
        }
      else
        {
          for (b = 0; b < ch; b ++)
            vp[b] += val_x * ip[b];
        }

      if (across_x < 1)
        continue;

      for (;;)
        {
          fill_x ++;
          vp += bpp;
          across_x --;
          g_assert (across_x >= 0);
          if (fill_x >= width)
            break;

          if (across_x >= 1)
            {
              if (has_alpha)
                {
                  gdouble a = ip[ch] / COLORMAX;
                  for (b = 0; b < ch; b ++)
                    vp[b] = a * ip[b];
                  vp[b] = a;
                }
              else
                {
                  for (b = 0; b < ch; b ++)
                    vp[b] = ip[b];
                }
            }
          else if (across_x > 0)
            {
              if (has_alpha)
                {
                  gdouble a = across_x * ip[ch] / COLORMAX;
                  for (b = 0; b < ch; b ++)
                    vp[b] = a * ip[b];
                  vp[b] = a;
                }
              else
                {
                  for (b = 0; b < ch; b ++)
                    vp[b] = across_x * ip[b];
                }
              break;

            }
          else
            {
              for (b = 0; b < bpp; b ++)
                vp[b] = 0;
              break;

            }
        }
    }

  pre_y = run_y;
  run_y += scale_y;
  across_y = run_y - fill_y;

  if (across_y <= 1)
    val_y = scale_y;
  else
    val_y = (fill_y + 1) - pre_y;

  for (b = 0; b < len; b ++)
    navi->sum[b] += val_y * val[b];

  if (across_y < 1)
    return 0;

  if (has_alpha)
    {
      gdouble *sp = navi->sum;
      for (x = 0; x < width; x ++)
        {
          gint bgsqr = ((x ^ fill_y) >> check->shift) & 1;
          gdouble bb = 1 - sp[ch];
          gdouble bc = bb * check->c[bgsqr];
          gint v;

          for (b = 0; b < ch; b ++)
            {
              v = RINT (sp[b] + bc);
              dp[b] = CLAMP (v, 0, COLORMAX);
              sp[b] = 0;
            }
          sp[b] = 0;
          sp += bpp;
          dp += ch;
        }
    }
  else
    {
      gint v;
      for (b = 0; b < len; b ++)
        {
          v = RINT (navi->sum[b]);
          dp[b] = CLAMP (v, 0, COLORMAX);
          navi->sum[b] = 0;
        }
      dp += len;
    }
  fill_y ++;
  /*
     if (fill_y >= height)
     return fill_y;
   */

  for (;;)
    {
      across_y --;
      g_assert (across_y >= 0);

      if (across_y >= 1)
        {
          if (has_alpha)
            {
              gdouble *vp = val;
              for (x = 0; x < width; x ++)
                {
                  gdouble bb = 1 - vp[ch];
                  gint bgsqr = ((x ^ fill_y) >> check->shift) & 1;
                  gdouble bc = bb * check->c[bgsqr];
                  gint v;
                  for (b = 0; b < ch; b ++)
                    {
                      v = RINT (vp[b] + bc);
                      dp[b] = CLAMP (v, 0, COLORMAX);
                    }
                  vp += bpp;
                  dp += ch;
                }
            }
          else
            {
              gint v;
              for (b = 0; b < len; b ++)
                {
                  v = RINT (vp[b]);
                  dp[b] = CLAMP (v, 0, COLORMAX);
                }
              dp += len;
            }
          fill_y ++;
          if (fill_y >= height)
            return fill_y;

        }
      else if (across_y > 0)
        {
          for (b = 0; b < len; b ++)
            navi->sum[b] = across_y * val[b];
          break;

        }
      else
        {
          for (b = 0; b < len; b ++)
            navi->sum[b] = 0;
          break;

        }
    }

  return fill_y;
}

static void
thumbnail_progress_callback (gpointer data)
{
  FblurPreviewNavigate *navi = (FblurPreviewNavigate *) data;
  thumbnail_progress_update (navi);

  navi->com_line ++;
  if (navi->com_line == navi->parent->img->h)
    {
      g_source_remove (navi->idle);
      navi->idle = 0;
      g_free (navi->sum);
      navi->sum = NULL;

      gtk_widget_queue_draw (navi->widget);

      return;
    }
}

static void
preview_progress_draw (gpointer data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) data;
  gint y, w, lines;

  lines = view->com_line - view->draw_line;

  g_assert (view->draw_line < view->img->h);
  g_assert (lines >= 0);

  if (! lines)
    return;

  y = view->draw_line;
  w = view->img->w;

  if (view->dblsize)
    {
      y     <<= 1;
      w     <<= 1;
      lines <<= 1;
    }

  gtk_widget_queue_draw_area (view->widget, 0, y, w, lines);

  if (view->parent->navigate->active)
    gtk_widget_queue_draw (view->parent->indicater);

  view->draw_line = view->com_line;
}

static void
preview_progress_update (gpointer data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) data;

  ImageBuffer   *src    = view->parent->img;
  ImageBuffer   *dest   = view->img;
  CheckValues   *check  = view->parent->check;
  g_assert (src);
  g_assert (dest);
  g_assert (view->com_line < src->h);
  g_assert (view->com_line < dest->h);

  gint bpp = src->bpp;
  gint has_alpha = src->has_alpha;
  gint ch = bpp - (has_alpha ? 1 : 0);
  gint x1 = dest->x;
  gint w = dest->w;

  gint y = view->com_line;
  gint y1 = dest->y + y;
  COLORBUF *dp = &(dest->data[dest->rowstride * y]);

  gint x, b;

  if (has_alpha)
    {
      for (x = 0; x < w; x ++)
        {
          focus_blur_pixel (dp, src, x1 + x, y1, view->dtable, view->model,
                            view->focus, view->dist_mode, view->map,
                            view->shine, view->use_color, view->whiteglow);

          gint bgsqr = ((x ^ y) >> check->shift) & 1;
          gint ab = (gint) dp[ch];
          gint bb = COLORMAX - ab;
          gint bc = bb * check->c[bgsqr];

          for (b = 0; b < ch; b ++)
            {
              *dp = (ab * *dp + bc + COLORMAX/2) / COLORMAX;
              dp ++;
            }
        }
    }
  else
    {
      for (x = 0; x < w; x ++)
        {
          focus_blur_pixel (dp, src, x1 + x, y1, view->dtable, view->model,
                            view->focus, view->dist_mode, view->map,
                            view->shine, view->use_color, view->whiteglow);
          dp += ch;
        }
    }

  view->com_line ++;

  if (view->com_line < dest->h)
    return;

  preview_progress_draw (view);
  preview_progress_free (view);
}

static void
preview_progress_init (FblurPreviewViewport *view)
{
  g_assert (view->img);

  preview_progress_free (view);

  if ((fblur_vals.model != MODEL_BRUSH && fblur_vals.radius <= 0.0) ||
      (fblur_vals.model == MODEL_BRUSH && fblur_vals.bscale <= 0.0) ||
      ((! fblur_vals.use_map) && fblur_vals.focus <= 0))
    {
      preview_revert (view);
      return;
    }

  view->map       = fblur_update_map (FALSE);
  view->dtable    = dtable_update (FALSE);
  view->model     = fblur_vals.model;
  view->shine     = fblur_update_shine (view->parent->img);
  view->focus     = fblur_vals.focus;
  view->dist_mode = (view->map) ? fblur_vals.dist_mode : DWEIGHT_NONE;
  view->use_color = (view->parent->img->is_rgb &&
                     view->dtable->model == MODEL_BRUSH &&
                     view->dtable->color && view->shine);
  view->whiteglow = (fblur_vals.whiteglow / 100) - 1.0;

  view->idle = g_idle_add_full
    (G_PRIORITY_LOW + 10, (GSourceFunc) preview_progress_update, view, NULL);

  view->idle_draw = g_timeout_add_full
    (G_PRIORITY_LOW, 250, (GSourceFunc) preview_progress_draw, view, NULL);
}

static void
preview_progress_free (FblurPreviewViewport *view)
{
  if (view->idle)
    {
      g_source_remove (view->idle);
      view->idle = 0;
    }

  if (view->idle_draw)
    {
      g_source_remove (view->idle_draw);
      view->idle_draw = 0;
    }

  view->com_line = 0;
  view->draw_line = 0;

  if (view->parent->navigate->active)
    gtk_widget_queue_draw (view->parent->indicater);
}

static void
fblur_preview_invalidate (FblurPreviewViewport *view)
{
  if (! view->active)
    return;

  if (! fblur_vals.preview)
    return;

  preview_progress_free (view);

  view->idle = g_timeout_add_full (G_PRIORITY_LOW - 10, 500,
                                   (GSourceFunc) preview_progress_init,
                                   view, NULL);
}

static ImageBuffer*
make_thumbnail_img_raw (ImageBuffer *img, guint width, guint height)
{
  ImageBuffer *t = img_allocate (img->bpp, width, height);
  t->ch          = img->ch;
  t->is_rgb      = img->is_rgb;
  t->has_alpha   = img->has_alpha;

  COLORBUF *dp   = t->data;

  guint step_x   = (img->w << 14) / width;
  guint step_y   = (img->h << 14) / height;
  guint step_table[width];

  guint x, y, b;
  guint pick_y = 1<<13;
  guint pick_x = 1<<13;

  for (x = 0; x < width; x ++)
    {
      guint sx = pick_x >> 14;
      step_table[x] = img->bpp * sx;
      pick_x += step_x;
    }

  for (y = 0; y < height; y ++)
    {
      guint sy = pick_y >> 14;
      COLORBUF *lp = &img->data[sy * img->rowstride];
      for (x = 0; x < width; x ++)
        {
          COLORBUF *ip = &(lp[step_table[x]]);
          for (b = 0; b < img->bpp; b ++)
            *dp ++ = ip[b];
        }
      pick_y += step_y;
    }

  return t;
}

/* Preview interface */

static CheckValues*
check_values_new (void)
{
  CheckValues *check = g_new (CheckValues, 1);
  const guchar gimp_check_shift =
    (GIMP_CHECK_SIZE == 1<<2) ? 2 :
    (GIMP_CHECK_SIZE == 1<<3) ? 3 :
    (GIMP_CHECK_SIZE == 1<<4) ? 4 : 3;

  g_assert (check);

  /* preset to constant value */
  check->shift = gimp_check_shift;
  check->c[0]  = RINT (GIMP_CHECK_LIGHT * 255);
  check->c[1]  = RINT (GIMP_CHECK_DARK  * 255);

  /* sync to user preference */
  switch (gimp_check_size ())
    {
    case GIMP_CHECK_SIZE_SMALL_CHECKS:
      check->shift = 2;
      break;
    case GIMP_CHECK_SIZE_MEDIUM_CHECKS:
      check->shift = 3;
      break;
    case GIMP_CHECK_SIZE_LARGE_CHECKS:
      check->shift = 4;
    }

  gimp_checks_get_shades (gimp_check_type (), &(check->c[0]), &(check->c[1]));

  return check;
}

static FblurPreviewNavigate*
preview_navigate_new (guint width,
                      guint height)
{
  FblurPreviewNavigate  *navi;
  GtkWidget             *draw;
  guint                  max_width, max_height;
  gdouble                scale;
  gint                   w2, h2, cw, ch;

  max_width  = MIN (PREVIEW_SIZE * 4 / 3, width);
  max_height = MIN (PREVIEW_SIZE, height);

  /* do not scale small image */
  if (width <= max_width && height <= max_height)
    {
      scale = 1;
      w2 = width;
      h2 = height;
    }
  else
    {
      scale = MIN ((gdouble) max_width / width,
                   (gdouble) max_height / height) - 0.0000001;
      w2 = ceil (width  * scale);
      h2 = ceil (height * scale);
    }

  g_assert (w2 <= (PREVIEW_SIZE * 4 / 3) && h2 <= PREVIEW_SIZE);

  draw = gtk_drawing_area_new ();
  gtk_widget_set_size_request (draw, w2, h2);
  gtk_widget_add_events (draw,
                         GDK_BUTTON_MOTION_MASK |
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  navi = g_new (FblurPreviewNavigate, 1);
  navi->parent   = NULL;
  navi->widget   = draw;
  navi->active   = FALSE;
  navi->status   = PREVIEW_STANDBY;
  navi->width    = w2;
  navi->height   = h2;
  navi->grip_x   = 0;
  navi->grip_y   = 0;
  navi->old_x    = -1;
  navi->old_y    = -1;
  navi->cursor_x = 0;
  navi->cursor_y = 0;

  cw = floor (MIN (PREVIEW_SIZE, width) * scale);
  cw = CLAMP (cw, 1, w2);
  ch = floor (MIN (PREVIEW_SIZE, height) * scale);
  ch = CLAMP (ch, 1, h2);

  navi->cursor_w  = cw;
  navi->cursor_h  = ch;
  navi->cursor_on = FALSE;

  navi->cursor_gc = NULL;
  navi->dash_offset = 0;
  navi->dash_interval = 0;

  navi->idle     = 0;
  navi->com_line = 0;
  navi->run_y    = 0;
  navi->sum      = NULL;
  navi->img      = NULL;

  g_signal_connect (G_OBJECT (draw), "event",
                    G_CALLBACK (preview_navigate_event_callback), navi);

  return navi;
}

static void
preview_navigate_free (FblurPreviewNavigate *navi)
{
  if (navi->cursor_gc)
    g_object_unref (navi->cursor_gc);

  if (navi->dash_interval)
    g_source_remove (navi->dash_interval);

  if (navi->idle)
    g_source_remove (navi->idle);

  if (navi->sum)
    g_free (navi->sum);

  if (navi->img)
    img_free (navi->img);

  g_free (navi);
}


static FblurPreviewViewport*
preview_viewport_new (guint width,
                      guint height)
{
  guint w = MIN (PREVIEW_SIZE, width);
  guint h = MIN (PREVIEW_SIZE, height);

  GtkWidget *draw = gtk_drawing_area_new ();
  gtk_widget_set_size_request (draw, w, h);

  FblurPreviewViewport *view = g_new (FblurPreviewViewport, 1);
  view->widget    = draw;
  view->menu      = NULL;
  view->picker    = NULL;
  view->active    = FALSE;
  view->status    = PREVIEW_STANDBY;
  view->grip_x    = 0; 
  view->grip_y    = 0; 
  view->pick_x    = 0;
  view->pick_y    = 0;
  view->pre_time  = 0;
  view->dblsize   = FALSE;
  view->idle      = 0;
  view->idle_draw = 0;
  view->com_line  = 0;
  view->draw_line = 0;
  view->map       = NULL;
  view->shine     = NULL;
  view->img       = NULL;

  gtk_widget_add_events (draw,
                         GDK_BUTTON_MOTION_MASK |
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                         GDK_ENTER_NOTIFY_MASK);

  g_signal_connect (draw, "event",
                    G_CALLBACK (preview_viewport_event_callback), view);

  /* pop-up menu for preview */
  GtkWidget *menu = gtk_menu_new ();
  GtkWidget *item1 = gtk_menu_item_new_with_mnemonic (_("_Refresh"));
  GtkWidget *item2 = gtk_menu_item_new_with_mnemonic (_("Pre_view"));
  GtkWidget *item3 = gtk_check_menu_item_new_with_mnemonic (_("Au_to"));
  GtkWidget* item4 = gtk_menu_item_new_with_mnemonic (_("S_croll"));
  GtkWidget *item5 = gtk_menu_item_new_with_mnemonic (_("P_ickup distance"));
  GtkWidget *sep = gtk_separator_menu_item_new ();
  GtkWidget *item6 = gtk_check_menu_item_new_with_mnemonic (_("Double si_ze"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item1);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item2);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item3);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item4);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item5);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item6);
  g_signal_connect (item1, "activate",
                    G_CALLBACK (preview_menu_revert_callback), view);
  g_signal_connect (item2, "activate",
                    G_CALLBACK (preview_menu_render_callback), view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item3),
                                  fblur_vals.preview);
  g_signal_connect (item3, "activate",
                    G_CALLBACK (preview_menu_auto_callback), view);
  g_signal_connect (item4, "activate",
                    G_CALLBACK (preview_menu_scroll_callback), view);
  gtk_widget_set_sensitive (item4, (width > PREVIEW_SIZE ||
                                    height > PREVIEW_SIZE));
  g_signal_connect (item5, "activate",
                    G_CALLBACK (preview_menu_pickup_callback), view);
  g_signal_connect (item6, "activate",
                    G_CALLBACK (preview_menu_resize_callback), view);
  gtk_widget_show_all (menu);
  view->picker = item5;
  view->menu = menu;

  return view;
}

static void
preview_viewport_free (FblurPreviewViewport *view)
{
  if (view->idle)
    g_source_remove (view->idle);

  if (view->idle_draw)
    g_source_remove (view->idle_draw);

  /* view->dtable is copy */
  /* view->map    is copy */
  /* view->shine  is copy */

  if (view->img)
    img_free (view->img);

  g_free (view);
}

static void
preview_img_set (FblurPreview   *preview,
                 ImageBuffer    *src)
{
  FblurPreviewNavigate  *navi = preview->navigate;
  FblurPreviewViewport  *view = preview->viewport;

  if (preview->img)
    img_free (preview->img);
  preview->img = src;

  if (navi->img)
    img_free (navi->img);

  if (view->img)
    img_free (view->img);

  if (src->w <= navi->width &&
      src->h <= navi->height)
    {
      if (navi->dash_interval)
        {
          g_source_remove (navi->dash_interval);
          navi->dash_interval = 0;
        }

      ImageBuffer *copy = img_duplicate (src);
      navi->img = make_inactive_img (copy, navi->widget);
      navi->active = FALSE;

      if (copy->has_alpha)
        {
          ImageBuffer *orig = copy;
          copy = img_dup_noalpha (orig, preview->check);
          img_free (orig);
        }
      view->img = copy;
      view->active = TRUE;
    }
  else
    {
      if (navi->idle)
        g_source_remove (navi->idle);

      navi->com_line = 0;
      navi->run_y    = 0;
      if (navi->sum)
        g_free (navi->sum);

      gint x, len;
      len = navi->width * src->bpp;
      navi->sum   = g_new (gdouble, len);
      for (x = 0; x < len; x ++)
        navi->sum[x] = 0;

      ImageBuffer *thumb;
      thumb = make_thumbnail_img_raw (src, navi->width, navi->height);

      if (thumb->has_alpha)
        {
          ImageBuffer *orig = thumb;
          thumb = img_dup_noalpha (orig, preview->check);
          img_free (orig);
        }

      navi->img = thumb;
      navi->active = TRUE;
      view->active = TRUE;

      navi->idle = g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) thumbnail_progress_callback, navi, NULL);
    }
}

static FblurPreview*
fblur_preview_new (GimpDrawable *drawable)
{
  FblurPreview          *preview = g_new (FblurPreview, 1);
  FblurPreviewNavigate  *navi;
  FblurPreviewViewport  *view;
  ImageBuffer           *src;
  gint                   x, y, width, height;

  gimp_drawable_mask_intersect (drawable->drawable_id,
                                &x, &y, &width, &height);
  navi = preview_navigate_new (width, height);
  view = preview_viewport_new (width, height);

  preview->active       = TRUE;
  preview->status       = PREVIEW_STANDBY;
  preview->drawable     = drawable;
  preview->img          = NULL;
  preview->check        = check_values_new ();
  preview->navigate     = navi;
  preview->viewport     = view;
  navi->parent          = preview;
  view->parent          = preview;

  src = fblur_update_srcimg (preview->drawable);
  preview_img_set (preview, src);

  GtkWidget *hbox = gtk_hbox_new (FALSE, 4);
  GtkWidget *frame1 = gtk_frame_new (NULL);
  GtkWidget *frame2 = gtk_frame_new (NULL);
  GtkWidget *align1 = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  GtkWidget *align2 = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  GtkWidget *align3 = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);

  GtkWidget *draw = gtk_drawing_area_new ();
  gtk_widget_set_size_request (draw, 13, 21);
  g_signal_connect (draw, "expose_event",
                    G_CALLBACK (indicater_expose_callback), preview);
  gtk_container_add (GTK_CONTAINER (align3), draw);
  preview->indicater = draw;

  gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_IN);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame1), navi->widget);
  gtk_container_add (GTK_CONTAINER (frame2), view->widget);
  gtk_container_add (GTK_CONTAINER (align1), frame1);
  gtk_container_add (GTK_CONTAINER (align2), frame2);
  gtk_box_pack_end (GTK_BOX (hbox), align1, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), align3, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), align2, FALSE, TRUE, 0);

  GtkWidget *ebox = gtk_event_box_new ();
  preview->widget = ebox;
  gtk_container_add (GTK_CONTAINER (ebox), hbox);
  g_signal_connect (ebox, "scroll_event",
                    G_CALLBACK (preview_viewport_event_callback), view);

  /* this place is not correct */
  dragcursor = gdk_cursor_new (GDK_FLEUR);
  crosshair  = gdk_cursor_new (GDK_CROSSHAIR);

  return preview;
}

static void
fblur_preview_free (FblurPreview *preview)
{
  /* preview->drawable is copy */
  /* preview->img is copy */

  g_free (preview->check);

  preview_navigate_free (preview->navigate);

  preview_viewport_free (preview->viewport);

  g_free (preview);

  /* this place is not correct */
  if (dragcursor)
    {
      gdk_cursor_unref (dragcursor);
      dragcursor = NULL;
    }
  if (crosshair)
    {
      gdk_cursor_unref (crosshair);
      crosshair = NULL;
    }
}

static void
preview_draw_img_area (GtkWidget    *widget,
                       ImageBuffer  *img,
                       GdkRectangle *area)
{
  gint      width, height;
  gint      tmp;
  COLORBUF *p;

  g_assert (img);
  g_assert (! img->has_alpha);

  width = area->width;
  height = area->height;

  tmp = img->w - area->x;
  if (width > tmp)
    width = tmp;
  tmp = img->h - area->y;
  if (height > tmp)
    height = tmp;

  tmp = img->rowstride * area->y + img->bpp * area->x;
  p = &(img->data[tmp]);

  if (img->is_rgb)
    gdk_draw_rgb_image (widget->window, widget->style->white_gc,
                        area->x, area->y, width, height,
                        GDK_RGB_DITHER_MAX, p, img->rowstride);
  else
    gdk_draw_gray_image (widget->window, widget->style->white_gc,
                         area->x, area->y, width, height,
                         GDK_RGB_DITHER_MAX, p, img->rowstride);
}

static void
preview_draw_img_area_dbl (GtkWidget    *widget,
                           ImageBuffer  *img,
                           GdkRectangle *area)
{
  g_assert (img);
  g_assert (! img->has_alpha);

  gint x2 = area->x & ~1;
  gint y2 = area->y & ~1;
  gint w2 = (area->width + (area->x - x2) + 1) & ~1;
  gint h2 = (area->height + (area->y - y2) + 1) & ~1;

  gint x0 = x2 >> 1;
  gint y0 = y2 >> 1;
  gint w0 = w2 >> 1;
  gint h0 = h2 >> 1;

  if (x0 + w0 > img->w)
    {
      w0 = img->w - x0;
      w2 = w0 << 1;
    }
  if (y0 + h0 > img->h)
    {
      h0 = img->h - y0;
      h2 = h0 << 1;
    }

  gint x1 = x0 + w0;
  gint y1 = y0 + h0;

  gint bpp2 = img->bpp << 1;
  gint len = w2 * img->bpp;
  guint8 *buf = g_alloca (len);
  gint y, x;

  for (y = y0; y < y1; y ++)
    {
      COLORBUF *p0 = &img->data[y * img->rowstride + x0 * img->bpp];

      if (img->is_rgb)
        {

          guint8 *p1 = buf;
          guint8 *p2 = p1 + img->bpp;
          for (x = x0; x < x1; x ++)
            {
              p1[0] = p2[0] = p0[0];
              p1[1] = p2[1] = p0[1];
              p1[2] = p2[2] = p0[2];
              p1 += bpp2;
              p2 += bpp2;
              p0 += img->bpp;
            }

          gint y2 = y << 1;
          gdk_draw_rgb_image (widget->window, widget->style->white_gc,
                              x2, y2, w2, 1, GDK_RGB_DITHER_MAX, buf, len);
          y2 ++;
          gdk_draw_rgb_image (widget->window, widget->style->white_gc,
                              x2, y2, w2, 1, GDK_RGB_DITHER_MAX, buf, len);
        }
      else
        {
          guint8 *p1 = buf;
          for (x = x0; x < x1; x ++)
            {
              p1[0] = p1[1] = *p0 ++;
              p1 += 2;
            }

          gint y2 = y << 1;
          gdk_draw_gray_image (widget->window, widget->style->white_gc,
                               x2, y2, w2, 1, GDK_RGB_DITHER_MAX, buf, len);
          y2 ++;
          gdk_draw_gray_image (widget->window, widget->style->white_gc,
                               x2, y2, w2, 1, GDK_RGB_DITHER_MAX, buf, len);
        }
    }
  //g_free (buf);
}

static void
preview_dash_interval (gpointer data)
{
  FblurPreviewNavigate *navi = data;

  if (navi->cursor_on)
    {
      preview_cursor_draw (navi);

      navi->dash_offset += (dash_offset_max - 1);
      navi->dash_offset %= dash_offset_max;
      gdk_gc_set_dashes (navi->cursor_gc, navi->dash_offset,
                         dash_list, sizeof dash_list);

      preview_cursor_draw (navi);
    }
}

static void
preview_cursor_draw (FblurPreviewNavigate *navi)
{
  GdkDrawable *drawable = navi->widget->window;

  g_assert (navi->cursor_gc);

  gdk_draw_rectangle (drawable, navi->cursor_gc, FALSE,
                      navi->cursor_x, navi->cursor_y,
                      navi->cursor_w, navi->cursor_h);
}

static gboolean
preview_cursor_set (FblurPreviewNavigate *navi,
                    gint                  new_x,
                    gint                  new_y)
{
  gint max_x = navi->img->w - navi->cursor_w;
  gint max_y = navi->img->h - navi->cursor_h;

  new_x = CLAMP (new_x, 0, max_x);
  new_y = CLAMP (new_y, 0, max_y);

  if (navi->cursor_on)
    {
      if (new_x == navi->cursor_x &&
          new_y == navi->cursor_y)
        return FALSE;

      preview_cursor_draw (navi);
    }
  else
    {
      g_assert (navi->dash_interval == 0);
      navi->dash_interval =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 150,
                            (GSourceFunc) preview_dash_interval, navi, NULL);

      navi->cursor_on = TRUE;
    }

  navi->cursor_x = new_x;
  navi->cursor_y = new_y;
  preview_cursor_draw (navi);
  return TRUE;
}

static void
preview_cursor_off (FblurPreviewNavigate *navi)
{
  if (navi->cursor_on)
    {
      preview_cursor_draw (navi);
      navi->cursor_on = FALSE;

      g_assert (navi->dash_interval);
      g_source_remove (navi->dash_interval);
      navi->dash_interval = 0;
    }

}

static void
preview_cursor_update (FblurPreviewViewport *view)
{
  g_assert (view->img);

  FblurPreview          *p      = view->parent;
  ImageBuffer           *src    = p->img;
  g_assert (src);

  FblurPreviewNavigate  *navi   = p->navigate;
  ImageBuffer           *thumb  = navi->img;
  g_assert (thumb);

  if (! navi->cursor_on)
    return;

  gint off_x = view->img->x - src->x;
  gint off_y = view->img->y - src->y;

  gdouble scale_x = (gdouble) thumb->w / src->w;
  gdouble scale_y = (gdouble) thumb->h / src->h;
  gint new_x = RINT (scale_x * off_x);
  gint new_y = RINT (scale_y * off_y);

  preview_cursor_set (navi, new_x, new_y);
}

static void
preview_revert (FblurPreviewViewport *view)
{
  g_assert (view->img);

  ImageBuffer *src   = view->parent->img;
  ImageBuffer *orig  = view->img;
  CheckValues *check = view->parent->check;

  preview_progress_free (view);

  ImageBuffer *disp = NULL;

  if (src->w <= PREVIEW_SIZE && src->h <= PREVIEW_SIZE)
    {

      if (src->has_alpha)
        disp = img_dup_noalpha (src, check);
      else
        disp = img_duplicate (src);

    }
  else
    {
      disp = img_dup_crop (src, orig->x, orig->y, orig->w, orig->h);

      if (disp->has_alpha)
        {
          ImageBuffer *tmp = disp;
          disp = img_dup_noalpha (tmp, check);
          img_free (tmp);
        }
    }

  img_free (orig);

  view->active = TRUE;
  view->img = disp;

  gtk_widget_queue_draw (view->widget);
}

static void
preview_cursor_show (FblurPreviewNavigate *navi)
{
  FblurPreview          *p      = navi->parent;
  ImageBuffer           *src    = p->img;
  FblurPreviewViewport  *view   = p->viewport;
  CheckValues           *check  = p->check;

  preview_progress_free (view);

  guint show_x = RINT ((gdouble) navi->cursor_x * src->w / navi->img->w);
  guint show_y = RINT ((gdouble) navi->cursor_y * src->h / navi->img->h);
  guint show_w = MIN (PREVIEW_SIZE, src->w);
  guint show_h = MIN (PREVIEW_SIZE, src->h);
  show_x = MIN (show_x, src->w - show_w);
  show_y = MIN (show_y, src->h - show_h);

  show_x += src->x;
  show_y += src->y;

  ImageBuffer *disp = NULL;

  if (src->w <= PREVIEW_SIZE && src->h <= PREVIEW_SIZE)
    {

      if (src->has_alpha)
        disp = img_dup_noalpha (src, check);
      else
        disp = img_duplicate (src);

    }
  else
    {
      disp = img_dup_crop (src, show_x, show_y, show_w, show_h);

      if (disp->has_alpha)
        {
          ImageBuffer *orig = disp;
          disp = img_dup_noalpha (orig, check);
          img_free (orig);
        }
    }

  if (view->img)
    if (view->img != src && view->img != navi->img)
      img_free (view->img);

  view->active = TRUE;
  view->img = disp;

  gtk_widget_queue_draw (view->widget);

  fblur_preview_invalidate (view);
}

static gboolean
preview_navigate_event_callback (GtkWidget *widget,
                                 GdkEvent  *event,
                                 gpointer   data)
{
  FblurPreviewNavigate  *navi = (FblurPreviewNavigate *) data;
  FblurPreviewViewport  *view;
  FblurPreview          *preview;

  g_return_val_if_fail (navi->img, FALSE);

  if (event->type == GDK_MAP)
    {
      if (! navi->cursor_gc &&
          navi->active)
        {
          GdkDrawable   *drawable = widget->window;
          GdkGCValues    values;
          GdkColor       white    = widget->style->white;

          values.foreground = white;
          values.function   = GDK_XOR;
          values.line_width = 2;
          values.line_style = GDK_LINE_ON_OFF_DASH;

          navi->cursor_gc = gdk_gc_new_with_values
            (drawable, &values, GDK_GC_FOREGROUND | GDK_GC_FUNCTION
             | GDK_GC_LINE_STYLE | GDK_GC_LINE_WIDTH);

          gdk_gc_set_dashes (navi->cursor_gc, navi->dash_offset,
                             dash_list, sizeof dash_list);

          gint cx, cy;
          cx = (navi->img->w - navi->cursor_w) / 2;
          cy = (navi->img->h - navi->cursor_h) / 2;
          if (preview_cursor_set (navi, cx, cy))
            preview_cursor_show (navi);
        }
    }

  preview = navi->parent;
  if (! preview->active)
    return FALSE;

  if (event->type == GDK_EXPOSE)
    {
      GdkEventExpose *e = (GdkEventExpose *) event;

      preview_draw_img_area (widget, navi->img, &e->area);
      if (navi->cursor_on)
        preview_cursor_draw (navi);

      return TRUE;
    }

  if (! navi->active)
    return FALSE;

  view = preview->viewport;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      {
        GdkEventButton *b = (GdkEventButton *) event;
        guint mod_key = b->state & gtk_accelerator_get_default_mod_mask ();

        if (navi->status == PREVIEW_CANCEL)
          {
            if (b->state &
                (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK
                 | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK))
              break;

            navi->status = PREVIEW_STANDBY;
          }

        gdk_pointer_ungrab (b->time);

        if (b->button == 1 && navi->status == PREVIEW_STANDBY)
          {

            if (mod_key == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
              {
                if (! fblur_vals.use_map)
                  break;

                preview_navigate_pickup (navi, b->x, b->y);
                break;
              }

            if (mod_key == GDK_SHIFT_MASK)
              {
                view->pre_time = b->time;
              } /* ignore previous cursor */

            else if (navi->cursor_on &&
                     b->x >= navi->cursor_x &&
                     b->x < navi->cursor_x + navi->cursor_w &&
                     b->y >= navi->cursor_y &&
                     b->y < navi->cursor_y + navi->cursor_h)
              {

                navi->status = PREVIEW_CURSOR_DRAG;
                gdk_pointer_grab (b->window, FALSE, GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK, NULL, dragcursor,
                                  b->time);

                navi->old_x = navi->cursor_x;
                navi->old_y = navi->cursor_y;

                navi->grip_x = b->x - navi->cursor_x;
                navi->grip_y = b->y - navi->cursor_y;

                break;
              }

            navi->status = PREVIEW_CURSOR_SET;

            if (navi->cursor_on)
              {
                navi->old_x = navi->cursor_x;
                navi->old_y = navi->cursor_y;
              }
            else
              {
                navi->old_x = -1;
                navi->old_y = -1;
              }

            navi->grip_x = navi->cursor_w / 2;
            navi->grip_y = navi->cursor_h / 2;

            gint new_x = b->x - navi->grip_x;
            gint new_y = b->y - navi->grip_y;
            preview_cursor_set (navi, new_x, new_y);

            break;
          }

        if (b->button == 2 && navi->status == PREVIEW_STANDBY)
          {
            if (! navi->cursor_on)
              break;

            navi->status = PREVIEW_PREVIEW;

            break;
          }

        if (b->button == 3)
          {

            if (navi->status == PREVIEW_STANDBY)
              {
                if (navi->cursor_on)
                  {
                    navi->status = PREVIEW_CURSOR_OFF;
                  }
                break;
              }

            if (navi->status == PREVIEW_CURSOR_SET ||
                navi->status == PREVIEW_CURSOR_DRAG)
              {

                navi->status = PREVIEW_CANCEL;

                if (navi->old_x < 0 || navi->old_y < 0)
                  {
                    preview_cursor_off (navi);
                    break;
                  }

                preview_cursor_set (navi, navi->old_x, navi->old_y);
                break;
              }

            navi->status = PREVIEW_CANCEL;
          }
      }
      break;

    case GDK_MOTION_NOTIFY:
      {
        GdkEventMotion *m = (GdkEventMotion *) event;
        guint mod_key = m->state & gtk_accelerator_get_default_mod_mask ();
        gint new_x, new_y;

        if (navi->status == PREVIEW_CURSOR_SET)
          {
            gdk_pointer_grab (m->window, FALSE, GDK_POINTER_MOTION_MASK |
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
                              NULL, dragcursor, m->time);
            navi->status = PREVIEW_CURSOR_DRAG;
          }

        if (navi->status == PREVIEW_CURSOR_DRAG)
          {

            if (m->x < 0 || m->x >= navi->img->w ||
                m->y < 0 || m->y >= navi->img->h)
              {
                preview_cursor_off (navi);
                break;
              }

            new_x = m->x - navi->grip_x;
            new_y = m->y - navi->grip_y;
            if (! preview_cursor_set (navi, new_x, new_y))
              break; /* not changed */

            if (mod_key == GDK_SHIFT_MASK)
              {
                if (view->pre_time < m->time)
                  {
                    preview_cursor_show (navi);
                    view->pre_time = m->time + 20;
                  }
              }

            break;
          }

      }
      break;

    case GDK_BUTTON_RELEASE:
      {
        GdkEventButton *b = (GdkEventButton *) event;
        guint mod_key = b->state & gtk_accelerator_get_default_mod_mask ();

        gdk_pointer_ungrab (b->time);

        if (navi->status == PREVIEW_CANCEL)
          {
            navi->status = PREVIEW_STANDBY;
            break;
          }

        if (b->button == 1 && (navi->status == PREVIEW_CURSOR_SET ||
                               navi->status == PREVIEW_CURSOR_DRAG))
          {
            navi->status = PREVIEW_STANDBY;

            if (! navi->cursor_on)
              break;

            if (navi->old_x == navi->cursor_x &&
                navi->old_y == navi->cursor_y)
              break;

            if (mod_key != GDK_SHIFT_MASK)
              preview_cursor_show (navi);

            break;
          }

        if (b->button == 2 && navi->status == PREVIEW_PREVIEW)
          {
            navi->status = PREVIEW_STANDBY;

            if (! (b->x >= 0 && b->x < navi->img->w &&
                  b->y >= 0 && b->y < navi->img->h))
              break;

            if (! navi->cursor_on)
              break;

            if (view->active)
              preview_progress_init (view);

            break;
          }

        if (b->button == 3 && navi->status == PREVIEW_CURSOR_OFF)
          {
            navi->status = PREVIEW_STANDBY;

            if (! (b->x >= 0 && b->x < navi->img->w &&
                  b->y >= 0 && b->y < navi->img->h))
              break;

            preview_cursor_off (navi);
            break;
          }

      }
      break;

    case GDK_SCROLL:
      {
        GdkEventScroll *s = (GdkEventScroll *) event;
        gint mov_x, mov_y, amo_x, amo_y;
        gint new_x, new_y;

        if (! navi->cursor_on)
          break;

        if (navi->status != PREVIEW_STANDBY)
          break;

        mov_x = 0;
        mov_y = 0;
        if (s->state & GDK_SHIFT_MASK)
          {
            amo_x = navi->cursor_w;
            amo_y = navi->cursor_h;
          }
        else
          {
            amo_x = MAX (1, navi->cursor_w / 3);
            amo_y = MAX (1, navi->cursor_h / 3);
          }

        switch (s->direction)
          {
          case GDK_SCROLL_UP:
            mov_y = -amo_y;
            break;
          case GDK_SCROLL_DOWN:
            mov_y = amo_x;
            break;
          case GDK_SCROLL_LEFT:
            mov_x = -amo_x;
            break;
          case GDK_SCROLL_RIGHT:
            mov_x = amo_y;
            break;
          default:
            break;
          }

        if (s->state & GDK_CONTROL_MASK)
          SWAP (mov_x, mov_y);

        new_x = navi->cursor_x + mov_x;
        new_y = navi->cursor_y + mov_y;

        if (preview_cursor_set (navi, new_x, new_y))
          preview_cursor_show (navi);
      }
      break;

    case GDK_2BUTTON_PRESS:
      {
        if (view->active)
          preview_progress_init (view);
      }
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

static gboolean
preview_viewport_event_callback (GtkWidget *widget,
                                 GdkEvent  *event,
                                 gpointer   data)
{
  FblurPreviewViewport  *view = (FblurPreviewViewport *) data;
  FblurPreviewNavigate  *navi;
  FblurPreview          *preview;
  guint                  mod_key;

  if (! view->img)
    return FALSE;

  preview = view->parent;

  if (! preview->active)
    return FALSE;

  if (event->type == GDK_EXPOSE)
    {
      GdkEventExpose *e = (GdkEventExpose *) event;

      if (view->dblsize)
        preview_draw_img_area_dbl (widget, view->img, &(e->area));
      else
        preview_draw_img_area (widget, view->img, &(e->area));

      return TRUE;
    }

  if (! view->active)
    return FALSE;

  navi    = preview->navigate;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:

      if (view->status == PREVIEW_CANCEL)
        break;

      {
        GdkEventButton *b = (GdkEventButton *) event;
        mod_key = b->state & gtk_accelerator_get_default_mod_mask ();

        if (b->button == 1)
          {
            if (view->status == PREVIEW_CURSOR_DRAG)
              break;

            if (mod_key == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
              {
                if (! fblur_vals.use_map)
                  break;

                preview_viewport_pickup (view, b->x, b->y);
                break;
              }

            if (mod_key == GDK_CONTROL_MASK)
              {

                if (view->menu)
                  {
                    gtk_widget_set_sensitive (view->picker, fblur_vals.use_map);
                    view->pick_x = b->x;
                    view->pick_y = b->y;

                    gtk_menu_popup (GTK_MENU (view->menu), NULL, NULL, NULL,
                                    NULL, 0, b->time);
                    break;
                  }

                view->status = PREVIEW_CURSOR_SET;
                break;
              }

            if (mod_key == GDK_SHIFT_MASK)
              {
                view->status = PREVIEW_CURSOR_DRAG;
                gdk_pointer_grab (b->window, FALSE, GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK, NULL, dragcursor,
                                  b->time);

                view->grip_x = b->x;
                view->grip_y = b->y;
                view->pre_time = b->time;
                break;
              }

            view->grip_x = b->x;
            view->grip_y = b->y;
            view->status = PREVIEW_PREVIEW;
            /* Tablet often makes motion */
            view->pre_time = b->time + 150;
            break;
          }

        if (b->button == 2)
          {
            if (! navi->active)
              return TRUE;

            view->status = PREVIEW_CURSOR_DRAG;
            gdk_pointer_grab (b->window, FALSE, GDK_POINTER_MOTION_MASK |
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
                              NULL, dragcursor, b->time);

            view->grip_x = b->x;
            view->grip_y = b->y;
            view->pre_time = b->time;
            break;
          }

        if (b->button == 3)
          {
            if (view->status != PREVIEW_STANDBY)
              {
                if (view->status == PREVIEW_CURSOR_DRAG)
                  {
                    gdk_pointer_ungrab (b->time);
                    fblur_preview_invalidate (view);
                  }
                view->status = PREVIEW_CANCEL;
                break;
              }

            if (view->menu)
              {
                gtk_widget_set_sensitive (view->picker, fblur_vals.use_map);
                if (fblur_vals.use_map)
                  {
                    view->pick_x = b->x;
                    view->pick_y = b->y;
                  }

                gtk_menu_popup (GTK_MENU (view->menu), NULL, NULL, NULL, NULL,
                                0, b->time);
                break;
              }

            view->status = PREVIEW_CURSOR_SET;
            break;
          }
      }
      break;

    case GDK_MOTION_NOTIFY:
      {
        GdkEventMotion *m = (GdkEventMotion *) event;

        if (view->status == PREVIEW_PREVIEW)
          {
            if (m->time < view->pre_time)
              return TRUE;
            if (! navi->active)
              return TRUE;
            view->status = PREVIEW_CURSOR_DRAG;
            gdk_pointer_grab (m->window, FALSE, GDK_POINTER_MOTION_MASK |
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
                              NULL, dragcursor, m->time);
          }

        if (view->status == PREVIEW_CURSOR_DRAG)
          {
            ImageBuffer *src;
            gint old_x, old_y, mov_x, mov_y, new_x, new_y;
            gint min_x, min_y, max_x, max_y;

            /* If you want more smooth scrolling on your PC,
               remove below three lines. */
            if (m->time < view->pre_time)
              return TRUE;
            view->pre_time = m->time + 20;

            old_x = view->img->x;
            old_y = view->img->y;

            if (view->dblsize)
              {
                mov_x = (view->grip_x - m->x) / 2;
                mov_y = (view->grip_y - m->y) / 2;
                view->grip_x -= (mov_x << 1);
                view->grip_y -= (mov_y << 1);
              }
            else
              {
                mov_x = view->grip_x - m->x;
                mov_y = view->grip_y - m->y;
                view->grip_x -= mov_x;
                view->grip_y -= mov_y;
              }
            new_x = old_x + mov_x;
            new_y = old_y + mov_y;

            src = preview->img;
            min_x = src->x;
            min_y = src->y;
            max_x = src->x + src->w - view->img->w;
            max_y = src->y + src->h - view->img->h;
            new_x = CLAMP (new_x, min_x, max_x);
            new_y = CLAMP (new_y, min_y, max_y);

            if (new_x == old_x && new_y == old_y)
              return TRUE;

            view->img->x = new_x;
            view->img->y = new_y;

            preview_revert (view);
            preview_cursor_update (view);
          }
      }
      break;

    case GDK_BUTTON_RELEASE:
      {
        GdkEventButton *b = (GdkEventButton *) event;

        gdk_pointer_ungrab (b->time);

        if (view->status == PREVIEW_CANCEL)
          {
            if (! (b->state &
                   (((b->button == 1) ? 0 : GDK_BUTTON1_MASK) |
                    ((b->button == 2) ? 0 : GDK_BUTTON2_MASK) |
                    ((b->button == 3) ? 0 : GDK_BUTTON3_MASK) |
                    ((b->button == 4) ? 0 : GDK_BUTTON4_MASK) |
                    ((b->button == 5) ? 0 : GDK_BUTTON5_MASK))))
              view->status = PREVIEW_STANDBY;
            break;
          }

        if (view->status == PREVIEW_CURSOR_DRAG)
          {
            gdk_pointer_ungrab (b->time);
            view->status = PREVIEW_STANDBY;
            fblur_preview_invalidate (view);
            break;
          }

        if (! (b->x >= 0 && b->x < view->widget->allocation.width &&
               b->y >= 0 && b->y < view->widget->allocation.height))
          break;

        if (view->status == PREVIEW_PREVIEW)
          {
            view->status = PREVIEW_STANDBY;
            preview_progress_init (view);
            break;
          }

        if (view->status == PREVIEW_CURSOR_SET)
          {
            view->status = PREVIEW_STANDBY;
            preview_revert (view);
            break;
          }
      }
      break;

    case GDK_SCROLL:
      {
        /* scroll event is permited while mouse grabing.
         * keybord scrolling is diverted to this event. */

        GdkEventScroll *s = (GdkEventScroll *) event;
        ImageBuffer *src;
        gint mov_x, mov_y, mov_a;
        gint old_x, old_y, new_x, new_y;
        gint min_x, min_y, max_x, max_y;

        mov_x = 0;
        mov_y = 0;
        mov_a = (s->state & GDK_SHIFT_MASK) ? 8 : 1;
        switch (s->direction)
          {
          case GDK_SCROLL_UP:
            mov_y = -mov_a;
            break;
          case GDK_SCROLL_DOWN:
            mov_y = mov_a;
            break;
          case GDK_SCROLL_LEFT:
            mov_x = -mov_a;
            break;
          case GDK_SCROLL_RIGHT:
            mov_x = mov_a;
            break;
          default:
            break;
          }
        if (s->state & GDK_CONTROL_MASK)
          SWAP (mov_x, mov_y);

        old_x = view->img->x;
        old_y = view->img->y;
        new_x = old_x + mov_x;
        new_y = old_y + mov_y;

        src = preview->img;
        min_x = src->x;
        min_y = src->y;
        max_x = src->x + src->w - view->img->w;
        max_y = src->y + src->h - view->img->h;
        new_x = CLAMP (new_x, min_x, max_x);
        new_y = CLAMP (new_y, min_y, max_y);

        if (new_x == old_x && new_y == old_y)
          return TRUE;

        view->img->x = new_x;
        view->img->y = new_y;
        preview_revert (view);

        /* update preview */
        if (! gdk_pointer_is_grabbed ())
          fblur_preview_invalidate (view);

        preview_cursor_update (view);
      }
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

static gboolean
indicater_expose_callback (GtkWidget      *widget,
                           GdkEventExpose *event,
                           gpointer        data)
{
  FblurPreview *p = (FblurPreview *) data;

  if (! p->active)
    return FALSE;

  static GdkPoint points0[] =
    { {0, 9}, {9, 0}, {11, 0}, {11, 19}, {9, 19}, {0, 10} };
  static GdkPoint points1[] =
    { {0, 9}, {9, 0}, {11, 0}, {10, 1}, {9, 1}, {1, 9} };
  static GdkPoint points2[] =
    { {0, 10}, {9, 19}, {11, 19}, {10, 18}, {9, 18}, {1, 10} };
  static GdkPoint points3[] = { {10, 1}, {10, 18}, {11, 19}, {11, 0} };
  static GdkPoint points4[] = { {2, 9}, {9, 2}, {9, 17}, {2, 10} };
  static const gint npoints0 = sizeof points0 / sizeof points0[0];
  static const gint npoints1 = sizeof points1 / sizeof points1[0];
  static const gint npoints2 = sizeof points2 / sizeof points2[0];
  static const gint npoints3 = sizeof points3 / sizeof points3[0];
  static const gint npoints4 = sizeof points4 / sizeof points4[0];

  static GdkGC *color[4] = { NULL, NULL, NULL, NULL };
  if (! color[0])
    {
      color[0] = color[1] = color[2] = color[3] =
        widget->style->bg_gc[GTK_STATE_INSENSITIVE];

      GdkColormap *colormap = gdk_gc_get_colormap (widget->style->white_gc);
      GdkGCValues v;
      GdkColor c;

      c.red = 0xffff;
      c.green = 0x4444;
      c.blue = 0x1111;
      gdk_colormap_alloc_color (colormap, &c, FALSE, TRUE);
      v.foreground.pixel = c.pixel;
      color[0] =
        gdk_gc_new_with_values (widget->window, &v, GDK_GC_FOREGROUND);

      c.red = 0xdddd;
      c.green = 0x3333;
      c.blue = 0x1111;
      gdk_colormap_alloc_color (colormap, &c, FALSE, TRUE);
      v.foreground.pixel = c.pixel;
      color[1] =
        gdk_gc_new_with_values (widget->window, &v, GDK_GC_FOREGROUND);

      c.red = 0x5555;
      c.green = 0x1111;
      c.blue = 0x0000;
      gdk_colormap_alloc_color (colormap, &c, FALSE, TRUE);
      v.foreground.pixel = c.pixel;
      color[2] =
        gdk_gc_new_with_values (widget->window, &v, GDK_GC_FOREGROUND);
    }

  if (! p->navigate->active)
    {
      gdk_draw_lines (widget->window,
                      widget->style->dark_gc[GTK_STATE_NORMAL], points0,
                      npoints0);
      gdk_draw_polygon (widget->window, color[3], TRUE, points4, npoints4);
      gdk_draw_lines (widget->window, widget->style->mid_gc[GTK_STATE_NORMAL],
                      points4, npoints4);
      return TRUE;
    }

  static gint f = 0;
  if (p->viewport->idle)
    {
      f ^= 1;
      gdk_draw_polygon (widget->window, color[f], TRUE, points0, npoints0);
      gdk_draw_lines (widget->window,
                      widget->style->dark_gc[GTK_STATE_NORMAL], points1,
                      npoints1);
      gdk_draw_lines (widget->window, widget->style->mid_gc[GTK_STATE_NORMAL],
                      points2, npoints2);
      gdk_draw_lines (widget->window,
                      widget->style->light_gc[GTK_STATE_NORMAL], points3,
                      npoints3);
      return TRUE;
    }
  f = 0;

  gdk_draw_polygon (widget->window, color[2], TRUE, points0, npoints0);
  gdk_draw_lines (widget->window, widget->style->dark_gc[GTK_STATE_NORMAL],
                  points1, npoints1);
  gdk_draw_lines (widget->window, widget->style->mid_gc[GTK_STATE_NORMAL],
                  points2, npoints2);
  gdk_draw_lines (widget->window, widget->style->light_gc[GTK_STATE_NORMAL],
                  points3, npoints3);
  return TRUE;
}

static void
preview_mouse_cursor_set (FblurPreview      *preview,
                          PreviewStatusType  status)
{
  FblurPreviewNavigate *navi;
  FblurPreviewViewport *view;

  g_assert (preview);
  g_return_if_fail (preview->active); /* check before */

  if (status == preview->status)
    return;

  navi = preview->navigate;
  view = preview->viewport;

  if (! navi->active)
    navi = NULL;
  if (! view->active)
    view = NULL;

  g_return_if_fail (navi || view);

  if (status == PREVIEW_CURSOR_DRAG)
    {
      if (navi)
        gdk_window_set_cursor (navi->widget->window, dragcursor);
      if (view)
        gdk_window_set_cursor (view->widget->window, dragcursor);
      preview->status = PREVIEW_CURSOR_DRAG;
    }

  else if (status ==  PREVIEW_PICKUP)
    {
      if (navi)
        gdk_window_set_cursor (navi->widget->window, crosshair);
      if (view)
        gdk_window_set_cursor (view->widget->window, crosshair);
      preview->status = PREVIEW_PICKUP;
    }

  else if (status == PREVIEW_STANDBY)
    {
      if (navi)
        gdk_window_set_cursor (navi->widget->window, NULL);
      if (view)
        gdk_window_set_cursor (view->widget->window, NULL);
      preview->status = PREVIEW_STANDBY;
    }
  else
    g_assert_not_reached ();
 }

static gboolean
preview_dialog_event_hook (GtkWidget    *widget,
                           GdkEvent     *event,
                           gpointer      data)
{
  FblurPreview          *preview = (FblurPreview *) data;
  FblurPreviewViewport  *view    = preview->viewport;
  GdkEventKey           *key;
  guint                  mod_key;

  if (! preview->active ||
      ! preview->viewport->active ||
      ! preview->viewport->img)
    return FALSE;


  /* scroll */
  if (view->status == PREVIEW_CURSOR_DRAG)
    {
      if (event->type != GDK_KEY_PRESS)
        return FALSE;

      key = (GdkEventKey *) event;
      mod_key = key->state;

      /* dragging */
      if (mod_key & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK |
                     GDK_BUTTON4_MASK | GDK_BUTTON5_MASK))
        return FALSE;

      /* ignore modifier press */
      if (key->keyval == GDK_Control_L ||
          key->keyval == GDK_Control_R ||
          key->keyval == GDK_Shift_L   ||
          key->keyval == GDK_Shift_R   ||
          key->keyval == GDK_Alt_L     ||
          key->keyval == GDK_Alt_R)
        return FALSE;

      mod_key &= gtk_accelerator_get_default_mod_mask ();

      /* scroll with keyboard */
      if (key->keyval == GDK_Left     ||
          key->keyval == GDK_Up       ||
          key->keyval == GDK_Right    ||
          key->keyval == GDK_Down     ||
          key->keyval == GDK_KP_Left  ||
          key->keyval == GDK_KP_Up    ||
          key->keyval == GDK_KP_Right ||
          key->keyval == GDK_KP_Down)
        {
          GdkEventScroll e;
          e.type        = GDK_SCROLL;
          e.window      = view->widget->window;
          e.time        = key->time;
          e.state       = key->state;

          if (key->keyval == GDK_Left ||
              key->keyval == GDK_KP_Left)
            e.direction = GDK_SCROLL_LEFT;
          else if (key->keyval == GDK_Up ||
                   key->keyval == GDK_KP_Up)
            e.direction = GDK_SCROLL_UP;
          else if (key->keyval == GDK_Right ||
                   key->keyval == GDK_KP_Right)
            e.direction = GDK_SCROLL_RIGHT;
          else if (key->keyval == GDK_Down ||
                   key->keyval == GDK_KP_Down)
            e.direction = GDK_SCROLL_DOWN;

          gdk_event_put ((GdkEvent *) &e);
          return TRUE;
        }

      gdk_pointer_ungrab (key->time);
      view->status = PREVIEW_STANDBY;

      if (key->keyval == GDK_v &&
          mod_key == GDK_MOD1_MASK)
        {
          preview_menu_render_callback (NULL, view);
          return TRUE;
        }

      fblur_preview_invalidate (view);
      return TRUE;
    }

  /* update mouse cursor */
  switch (event->type)
    {
    case GDK_KEY_PRESS:
      key = (GdkEventKey *) event;
      mod_key = key->state;
      mod_key &= gtk_accelerator_get_default_mod_mask ();

      if (view->menu &&
          key->keyval == GDK_space && mod_key != 0)
        {
          preview_mouse_cursor_set (preview, PREVIEW_STANDBY);
          gtk_widget_set_sensitive (view->picker, FALSE);
          gtk_menu_popup (GTK_MENU (view->menu),
                          NULL, NULL, NULL, NULL, 0, key->time);
          return TRUE;
        }

      if (key->keyval == GDK_r &&
          mod_key == GDK_MOD1_MASK)
        {
          preview_menu_revert_callback (NULL, view);
          return TRUE;
        }

      if (key->keyval == GDK_v &&
          mod_key == GDK_MOD1_MASK)
        {
          preview_menu_render_callback (NULL, view);
          return TRUE;
        }

      if (key->keyval == GDK_c &&
          mod_key == GDK_MOD1_MASK)
        {
          preview_menu_scroll_callback (NULL, view);
          return TRUE;
        }

      if (key->keyval == GDK_Control_L ||
          key->keyval == GDK_Control_R)
        {
          mod_key |= GDK_CONTROL_MASK;
          goto update_mouse_cursor;
        }

      if (key->keyval == GDK_Shift_L ||
          key->keyval == GDK_Shift_R)
        {
          mod_key |= GDK_SHIFT_MASK;
          goto update_mouse_cursor;
        }

      /* for notebook. I want always to use this key binding */
      if (key->keyval == GDK_Prior &&
          mod_key == GDK_CONTROL_MASK)
        {
          gtk_notebook_prev_page
            (GTK_NOTEBOOK (fblur_widgets[FBLUR_WIDGET_NOTEBOOK]));
          return TRUE;
        }

      if (key->keyval == GDK_Next &&
          mod_key == GDK_CONTROL_MASK)
        {
          gtk_notebook_next_page
            (GTK_NOTEBOOK (fblur_widgets[FBLUR_WIDGET_NOTEBOOK]));
          return TRUE;
        }

      return FALSE;

    case GDK_KEY_RELEASE:
      key = (GdkEventKey *) event;
      mod_key = key->state;
      mod_key &= gtk_accelerator_get_default_mod_mask ();

      if (key->keyval == GDK_Control_L ||
          key->keyval == GDK_Control_R)
        {
          mod_key &= ~GDK_CONTROL_MASK;
          goto update_mouse_cursor;
        }

      if (key->keyval == GDK_Shift_L ||
          key->keyval == GDK_Shift_R)
        {
          mod_key &= ~GDK_SHIFT_MASK;
          goto update_mouse_cursor;
        }

      return FALSE;

    case GDK_ENTER_NOTIFY:

      if (! gtk_window_is_active
          (GTK_WINDOW (fblur_widgets[FBLUR_WIDGET_DIALOG])))
        return FALSE;

      mod_key = event->crossing.state;
      mod_key &= gtk_accelerator_get_default_mod_mask ();

      goto update_mouse_cursor;

    default:
      return FALSE;
    }

 update_mouse_cursor:

  if (mod_key == GDK_SHIFT_MASK)
    preview_mouse_cursor_set (preview, PREVIEW_CURSOR_DRAG);
  else if (mod_key == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    preview_mouse_cursor_set (preview, PREVIEW_PICKUP);
  else
    preview_mouse_cursor_set (preview, PREVIEW_STANDBY);

  return FALSE; /* must not eat */
}

static void
preview_menu_revert_callback (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;
  preview_revert (view);
}

static void
preview_menu_render_callback (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;
  preview_progress_init (view);
}

static void
preview_menu_auto_callback (GtkCheckMenuItem *checkmenuitem,
                            gpointer          user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;

  g_assert (checkmenuitem);
  fblur_vals.preview = gtk_check_menu_item_get_active (checkmenuitem);

  preview_progress_free (view);

  if (fblur_vals.preview)
    preview_progress_init (view);

  else
    preview_revert (view);
}

static void
preview_menu_scroll_callback (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;

  view->status = PREVIEW_CURSOR_DRAG;
  gdk_pointer_grab (view->widget->window, FALSE, GDK_POINTER_MOTION_MASK |
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK, NULL, dragcursor,
                    GDK_CURRENT_TIME);

 gtk_widget_get_pointer (view->widget, &(view->grip_x), &(view->grip_y));
 view->pre_time = 0;
}

static void
preview_menu_pickup_callback (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;
  gint x, y;

  x = view->pick_x;
  y = view->pick_y;

  g_return_if_fail (fblur_vals.use_map);
  g_return_if_fail (fblur_vals.map_id != 1);

  if (view->dblsize)
    {
      x >>= 1;
      y >>= 1;
    }

  if (view->img)
    {
      x += view->img->x;
      y += view->img->y;
    }

  focus_point (fblur_vals.map_id, x, y, fblur_vals.near);
}

static void
preview_menu_resize_callback (GtkCheckMenuItem *checkmenuitem,
                              gpointer          user_data)
{
  FblurPreviewViewport *view = (FblurPreviewViewport *) user_data;
  gboolean dblsize;

  g_assert (checkmenuitem);
  dblsize = gtk_check_menu_item_get_active (checkmenuitem);

  preview_viewport_resize (view, dblsize);
}

static void
preview_viewport_resize (FblurPreviewViewport *view,
                         gboolean              dblsize)
{
  view->dblsize = dblsize;

  gint width = view->widget->allocation.width;
  gint height = view->widget->allocation.height;

  if (dblsize)
    {
      gint req_w = view->img->w << 1;
      gint req_h = view->img->h << 1;
      if (width == req_w && height == req_h)
        return;
      gtk_widget_set_size_request (view->widget, req_w, req_h);
    }
  else
    {
      gint req_w = view->img->w;
      gint req_h = view->img->h;
      if (width == req_w && height == req_h)
        return;
      gtk_widget_set_size_request (view->widget, req_w, req_h);
    }
}

static void
preview_viewport_pickup (FblurPreviewViewport   *view,
                         gint                    x,
                         gint                    y)
{
  g_assert (view);
  g_assert (view->img);

  g_return_if_fail (fblur_vals.use_map);
  g_return_if_fail (fblur_vals.map_id != 1);

  if (view->dblsize)
    {
      x >>= 1;
      y >>= 1;
    }

  if (view->img)
    {
      x += view->img->x;
      y += view->img->y;
    }

  focus_point (fblur_vals.map_id, x, y, fblur_vals.near);
}

static void
preview_navigate_pickup (FblurPreviewNavigate   *navi,
                         gint                    x,
                         gint                    y)
{
  ImageBuffer *src, *thumb;

  g_assert (navi);
  g_assert (navi->img);

  g_return_if_fail (fblur_vals.use_map);
  g_return_if_fail (fblur_vals.map_id != 1);

  src   = navi->parent->img;
  thumb = navi->img;

  x = RINT (x * (gdouble) (src->w - 1) / (thumb->w - 1));
  y = RINT (y * (gdouble) (src->h - 1) / (thumb->h - 1));

  x += src->x;
  y += src->y;

  focus_point (fblur_vals.map_id, x, y, fblur_vals.near);
}

#endif /* FBLUR_PREVIEW */
