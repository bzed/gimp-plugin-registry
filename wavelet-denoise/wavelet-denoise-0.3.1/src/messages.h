/* 
 * Wavelet denoise GIMP plugin
 * 
 * messages.h
 * Copyright 2008 by Marco Rossini
 * 
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 */

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

/* TRANSLATORS: This is the name of the plugin as it appears in menus, the
   interface window title and such */
#define PLUGIN_NAME _("Wavelet denoise")

#define PLUGIN_HELP _("This plugin allows the separate denoising of image channels in multiple color spaces using wavelets.")

/* Tool-tips */
#define TT_THR_AMOUNT_COLOUR _("Adjusts the threshold for denoising of the selected channel in a range from 0.0 (none) to 10.0. The threshold is the value below which everything is considered noise.")
#define TT_THR_AMOUNT_GRAY _("Adjusts the threshold for denoising in a range from 0.0 (none) to 10.0. The threshold is the value below which everything is considered noise.")
#define TT_SELECT _("Select an image channel to edit its denoising settings.")
#define TT_PREVIEW_ALL _("Display all channels of the image (final image).")
#define TT_PREVIEW_SEL_GRAY _("Display only the selected channel in grayscale mode.")
#define TT_PREVIEW_SEL_COLOUR _("Display only the selected channel in color mode.")
#define TT_MODEL_YCBCR _("The YCbCr color model has one luminance channel (Y) which contains most of the detail information of an image (such as brightness and contrast) and two chroma channels (Cb = blueness, Cr = reddness) that hold the color information. Note that this choice drastically affects the result.")
#define TT_MODEL_RGB _("The RGB color model separates an image into channels of red, green, and blue. This is the default color model in GIMP. Note that this choice drastically affects the result.")
#define TT_MODEL_LAB _("CIELAB (L*a*b*) is a color model in which chrominance is separated from lightness and color distances are perceptually uniform. Note that this choice drastically affects the result.")
#define TT_THR_DETAIL_COLOUR _("This adjusts the softness of the thresholding (soft as opposed to hard thresholding). The higher the softness the more noise remains in the image. Default is 0.0.")
#define TT_THR_DETAIL_GRAY TT_THR_DETAIL_COLOUR
#define TT_RESET_PREVIEW _("Resets the settings for the selected channel while the button is pressed.")
#define TT_RESET_CHANNEL_GRAY _("Resets to the default values.")
#define TT_RESET_CHANNEL_COLOUR _("Resets the current channel to the default values.")
#define TT_RESET_ALL _("Resets all channels to the default values.")

#endif /* __MESSAGES_H__ */
