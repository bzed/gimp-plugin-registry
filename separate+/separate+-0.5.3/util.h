#ifndef SEPARATE_UTIL_H
#define SEPARATE_UTIL_H

enum separate_channel {sep_C,sep_M,sep_Y,sep_K};

gboolean separate_is_CMYK(gint32 image_id);

char *separate_filename_change_extension(char *root,char *newext);
char *separate_filename_add_suffix(char *root,char *suffix);

gint32 separate_create_RGB(gchar *filename,
	guint width, guint height, gint32 *layers);
gint32 separate_create_planes_grey(gchar *filename,
	guint width, guint height, gint32 *layers);
gint32 separate_create_planes_CMYK(gchar *filename,
	guint width, guint height, gint32 *layers,guchar *primaries);
gint32 separate_create_planes_Duotone(gchar *filename,
	guint width, guint height, gint32 *layers);

void separate_init_settings( struct SeparateContext *sc, gboolean get_last_values );
void separate_store_settings( struct SeparateContext *sc, enum separate_function func );

GimpDrawable *separate_find_channel(gint32 image_id,enum separate_channel channel);

#endif
