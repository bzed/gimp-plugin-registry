/**********************************************************************
  iccbutton.h
  Copyright(C) 2007-2010 Y.Yamakawa
**********************************************************************/

#ifndef __ICC_BUTTON_H__
#define __ICC_BUTTON_H__

#include <gtk/gtkbutton.h>
#include <lcms.h>

/* Utils (non-class functions) */
cmsHPROFILE  lcms_open_profile            (gchar       *filename);
gchar       *_icc_button_get_profile_desc (cmsHPROFILE  profile);

/* Stock IDs */
#define ICC_BUTTON_STOCK_INPUT_CLASS       "icc-button-input-class"
#define ICC_BUTTON_STOCK_DISPLAY_CLASS     "icc-button-display-class"
#define ICC_BUTTON_STOCK_RGB_OUTPUT_CLASS  "icc-button-rgb-output-class"
#define ICC_BUTTON_STOCK_CMYK_OUTPUT_CLASS "icc-button-cmyk-output-class"
#define ICC_BUTTON_STOCK_LINK_CLASS        "icc-button-link-class"
#define ICC_BUTTON_STOCK_ABSTRACT_CLASS    "icc-button-abstract-class"
#define ICC_BUTTON_STOCK_COLORSPACE_CLASS  "icc-button-colorspace-class"
#define ICC_BUTTON_STOCK_NAMEDCOLOR_CLASS  "icc-button-namedcolor-class"


G_BEGIN_DECLS

#define ICC_BUTTON_TYPE              ( icc_button_get_type() )
#define ICC_BUTTON( obj )            ( G_TYPE_CHECK_INSTANCE_CAST( ( obj ), ICC_BUTTON_TYPE, IccButton ) )
#define ICC_BUTTON_CLASS( klass )    ( G_TYPE_CHECK_CLASS_CAST( ( klass ), ICC_BUTTON_TYPE, IccButtonClass ) )
#define IS_ICC_BUTTON( obj )         ( G_TYPE_CHECK_INSTANCE_TYPE( ( obj ), ICC_BUTTON_TYPE ) )
#define IS_ICC_BUTTON_CLASS( klass ) ( G_TYPE_CHECK_INSTANCE_TYPE( ( klass ), ICC_BUTTON_TYPE ) )

#define ICC_PROFILE_DESC_MAX 2048

enum {
  ICC_BUTTON_CLASS_INPUT    = 1 << 0,
  ICC_BUTTON_CLASS_OUTPUT   = 1 << 1,
  ICC_BUTTON_CLASS_DISPLAY  = 1 << 2,
  ICC_BUTTON_CLASS_LINK     = 1 << 3,
  ICC_BUTTON_CLASS_ABSTRACT = 1 << 4,
  ICC_BUTTON_CLASS_ALL      = 0xffff
};

enum {
  ICC_BUTTON_COLORSPACE_XYZ  = 1 << 0,
  ICC_BUTTON_COLORSPACE_LAB  = 1 << 1,
  ICC_BUTTON_COLORSPACE_GRAY = 1 << 2,
  ICC_BUTTON_COLORSPACE_RGB  = 1 << 3,
  ICC_BUTTON_COLORSPACE_CMY  = 1 << 4,
  ICC_BUTTON_COLORSPACE_CMYK = 1 << 5,
  ICC_BUTTON_COLORSPACE_ALL  = 0xffff
};

typedef struct _IccButton {
  GtkButton button;
  GtkWidget *hbox;
  GtkWidget *icon;
  GtkWidget *label;

  GtkWidget *dialog;
  GtkWidget *scrolledWindow;
  GtkWidget *treeView;
  GtkWidget *entry;

  GtkWidget *popupMenu;
  GPtrArray *menuItems;
  gint nEntries; /* マスクで指定した条件に一致するプロファイルの総数 */
  gint menuWidth;
  gint menuHeight;
  glong last_updated;

  gchar *title; /* title of the dialog */
  gchar *path;
  icProfileClassSignature class;
  icColorSpaceSignature pcs;
  icColorSpaceSignature colorspace;
  guint16 classMask;
  guint16 pcsMask;
  guint16 colorspaceMask;
  gint maxEntries; /* ポップアップに表示するプロファイルの最大数 */
  gboolean enable_empty;
} IccButton;

typedef struct _IccButtonClass {
  GtkButtonClass parent_class;
  void ( *changed )( IccButton *button );
} IccButtonClass;


GType      icc_button_get_type          (void);
GtkWidget *icc_button_new               (void);

void       icc_button_set_title         (IccButton   *button,
                                         const gchar *newTitle);
gchar     *icc_button_get_title         (IccButton   *button);
void       icc_button_set_mask          (IccButton   *button,
                                         guint16      classMask,
                                         guint16      pcsMask,
                                         guint16      colorSpaceMask);
void       icc_button_get_mask          (IccButton   *button,
                                         guint16     *classMask,
                                         guint16     *pcsMask,
                                         guint16     *colorSpaceMask);
gboolean   icc_button_set_filename      (IccButton   *button,
                                         const gchar *filename,
                                         gboolean     add_history);
gchar     *icc_button_get_filename      (IccButton   *button);
gboolean   icc_button_set_max_entries   (IccButton   *button,
                                         gint         n);
gint       icc_button_get_max_entries   (IccButton   *button);
void       icc_button_set_enable_empty  (IccButton   *button,
                                         gboolean     enabled);
gboolean   icc_button_get_enable_empty  (IccButton   *button);
gboolean   icc_button_is_empty          (IccButton   *button);

/* Profile info of current selection */
const gchar *icc_button_get_profile_desc  (IccButton *button);
icProfileClassSignature
           icc_button_get_class         (IccButton *button);
icColorSpaceSignature
           icc_button_get_pcs           (IccButton *button);
icColorSpaceSignature
           icc_button_get_colorspace    (IccButton *button);

G_END_DECLS

#endif /* __ICC_BUTTON_H__ */
