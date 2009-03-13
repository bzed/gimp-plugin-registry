/*
	DustCleaner is a GIMP plugin to detect and remove the dust spots 
	in digital image.
	Copyright (C) 2006-2007  Frank Tao<solotim.cn@gmail.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
	02110-1301, USA        	
*/

/*
 * plugin UI 
 * Author: Frank.Tao<solotim.cn@gmail.com>
 * Date: 2007.04.12
 * Version: 0.1
 */
 
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "dustcleaner_gimp_plugin.h"

static void setToDetectionMode(GtkObject *ob, TPara *para);
static void setToRecoveryMode(GtkObject *ob, TPara *para);
static void setToRecoveryFromMaskMode(GtkObject *ob, TPara *para);

gboolean show_dustcleaner_dialog (GimpDrawable *drawable, TPara *para)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *hbox8;
  GtkWidget *vbox2;
  GtkWidget *rbt_detection;
  GSList *rbt_detection_group = NULL;
  GtkWidget *rbt_recovery;
  GtkWidget *rbt_file_recovery;
  GtkWidget *vbox7;
  GtkWidget *frame2;
  GtkWidget *alignment2;
  GtkWidget *table1;
  GtkWidget *label5;
  GtkWidget *label6;
  GtkWidget *label7;
  GtkWidget *label8;
  GtkWidget *label9;
  GtkWidget *hs_segm_s;
  GtkWidget *hs_width;
  GtkWidget *hs_height;
  GtkWidget *hs_ed;
  GtkWidget *hs_scan_s;
  GtkWidget *hs_radius;
  GtkWidget *hs_strength;
  GtkObject *spbt_scan_s_adj;
  GtkWidget *spbt_scan_s;
  GtkObject *spbt_height_adj;
  GtkWidget *spbt_height;
  GtkObject *spbt_ed_adj;
  GtkWidget *spbt_ed;
  GtkObject *spbt_segm_s_adj;
  GtkWidget *spbt_segm_s;
  GtkObject *spbt_width_adj;
  GtkWidget *spbt_width;
  GtkWidget *label3;
  GtkWidget *frame3;
  GtkWidget *alignment3;
  GtkWidget *table2;
  GtkWidget *label10;
  GtkWidget *label11;
  GtkObject *spbt_radius_adj;
  GtkWidget *spbt_radius;
  GtkObject *spbt_strength_adj;
  GtkWidget *spbt_strength;
  GtkWidget *label4;
  GtkWidget *hbox6;
  GtkWidget *label2;
  GtkWidget *entry1;
  GtkWidget *button1;
  GtkWidget *preview;

  gimp_ui_init ("dustcleaner", FALSE);

  dialog1 = gimp_dialog_new ("Dust Cleaner", "dustcleaner",
                            NULL, 0,
                            gimp_standard_help_func, "plug-in-dustcleaner",

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);
                            
  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, FALSE, TRUE, 0);

  hbox8 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox8);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox8, TRUE, TRUE, 0);

	/*============= preview =============*/
  preview = gimp_drawable_preview_new (drawable, NULL);
  gtk_widget_show (preview);
  gtk_box_pack_start (GTK_BOX (hbox8), preview, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (hbox8), vbox2, TRUE, TRUE, 0);

  rbt_detection = gtk_radio_button_new_with_mnemonic (NULL, "Detection");
  gtk_widget_show (rbt_detection);
  gtk_box_pack_start (GTK_BOX (vbox2), rbt_detection, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rbt_detection), rbt_detection_group);
  rbt_detection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rbt_detection));

  rbt_recovery = gtk_radio_button_new_with_mnemonic (NULL, "Recovery Instantly");
  gtk_widget_show (rbt_recovery);
  gtk_box_pack_start (GTK_BOX (vbox2), rbt_recovery, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rbt_recovery), rbt_detection_group);
  rbt_detection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rbt_recovery));

  rbt_file_recovery = gtk_radio_button_new_with_mnemonic (NULL, "Recovery with Dust Spot Mask File");
  //gtk_widget_show (rbt_file_recovery);
  gtk_box_pack_start (GTK_BOX (vbox2), rbt_file_recovery, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rbt_file_recovery), rbt_detection_group);
  rbt_detection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rbt_file_recovery));

  vbox7 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox7);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox7, TRUE, TRUE, 0);

  frame2 = gtk_frame_new (NULL);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (vbox7), frame2, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame2), 1);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);

  alignment2 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment2);
  gtk_container_add (GTK_CONTAINER (frame2), alignment2);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment2), 0, 0, 12, 0);

  table1 = gtk_table_new (5, 3, FALSE);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (alignment2), table1);

  label5 = gtk_label_new ("Scan Sensitivity:");
  gtk_widget_show (label5);
  gtk_table_attach (GTK_TABLE (table1), label5, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label5), 0, 0.5);

  label6 = gtk_label_new ("Segmentation Sensitivity:");
  gtk_widget_show (label6);
  gtk_table_attach (GTK_TABLE (table1), label6, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label6), 0, 0.5);

  label7 = gtk_label_new ("Min Dust Width(px):");
  gtk_widget_show (label7);
  gtk_table_attach (GTK_TABLE (table1), label7, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label7), 0, 0.5);

  label8 = gtk_label_new ("Min Dust Height(px):");
  gtk_widget_show (label8);
  gtk_table_attach (GTK_TABLE (table1), label8, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label8), 0, 0.5);

  label9 = gtk_label_new ("Erosion/Dilation(px):");
  gtk_widget_show (label9);
  gtk_table_attach (GTK_TABLE (table1), label9, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label9), 0, 0.5);

  spbt_height_adj = gtk_adjustment_new (8, 1, 200, 1, 1, 0);
  spbt_height = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_height_adj), 1, 0);
  gtk_widget_show (spbt_height);
  gtk_table_attach (GTK_TABLE (table1), spbt_height, 2, 3, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spbt_ed_adj = gtk_adjustment_new (0, -10, 10, 1, 1, 0);
  spbt_ed = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_ed_adj), 1, 0);
  gtk_widget_show (spbt_ed);
  gtk_table_attach (GTK_TABLE (table1), spbt_ed, 2, 3, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spbt_segm_s_adj = gtk_adjustment_new (50, 1, 100, 1, 1, 0);
  spbt_segm_s = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_segm_s_adj), 1, 0);
  gtk_widget_show (spbt_segm_s);
  gtk_table_attach (GTK_TABLE (table1), spbt_segm_s, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spbt_width_adj = gtk_adjustment_new (8, 1, 200, 1, 1, 0);
  spbt_width = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_width_adj), 1, 0);
  gtk_widget_show (spbt_width);
  gtk_table_attach (GTK_TABLE (table1), spbt_width, 2, 3, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
                    
  spbt_scan_s_adj = gtk_adjustment_new (60, 1, 100, 1, 1, 0);
  spbt_scan_s = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_scan_s_adj), 1, 0);
  gtk_widget_show (spbt_scan_s);
  gtk_table_attach (GTK_TABLE (table1), spbt_scan_s, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hs_width = gtk_hscale_new (GTK_ADJUSTMENT (spbt_width_adj));
  gtk_widget_show (hs_width);
  gtk_table_attach (GTK_TABLE (table1), hs_width, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_scale_set_digits (GTK_SCALE (hs_width), 0);                    

  hs_height = gtk_hscale_new (GTK_ADJUSTMENT (spbt_height_adj));
  gtk_widget_show (hs_height);
  gtk_table_attach (GTK_TABLE (table1), hs_height, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_height), 0);   
 
  hs_ed = gtk_hscale_new (GTK_ADJUSTMENT (spbt_ed_adj));
  gtk_widget_show (hs_ed);
  gtk_table_attach (GTK_TABLE (table1), hs_ed, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_ed), 0);                       

  hs_segm_s = gtk_hscale_new (GTK_ADJUSTMENT (spbt_segm_s_adj));
  gtk_widget_show (hs_segm_s);
  gtk_table_attach (GTK_TABLE (table1), hs_segm_s, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_segm_s), 0);   
 
  hs_scan_s = gtk_hscale_new (GTK_ADJUSTMENT (spbt_scan_s_adj));
  gtk_widget_show (hs_scan_s);
  gtk_table_attach (GTK_TABLE (table1), hs_scan_s, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_scan_s), 0);   
 
  label3 = gtk_label_new ("<b>Detection</b>");
  gtk_widget_show (label3);
  gtk_frame_set_label_widget (GTK_FRAME (frame2), label3);
  gtk_label_set_use_markup (GTK_LABEL (label3), TRUE);

  frame3 = gtk_frame_new (NULL);
  gtk_widget_show (frame3);
  gtk_box_pack_start (GTK_BOX (vbox7), frame3, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame3), 1);
  gtk_frame_set_shadow_type (GTK_FRAME (frame3), GTK_SHADOW_IN);

  alignment3 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment3);
  gtk_container_add (GTK_CONTAINER (frame3), alignment3);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment3), 0, 0, 12, 0);

  table2 = gtk_table_new (5, 3, FALSE);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (alignment3), table2);

  label10 = gtk_label_new ("Kernel Radius(px):");
  gtk_widget_show (label10);
  gtk_table_attach (GTK_TABLE (table2), label10, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label10), 0, 0.5);

  label11 = gtk_label_new ("Strength:");
  gtk_widget_show (label11);
  gtk_table_attach (GTK_TABLE (table2), label11, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label11), 0, 0.5);



  spbt_radius_adj = gtk_adjustment_new (0, 0, 500, 1, 1, 0);
  spbt_radius = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_radius_adj), 1, 0);
  gtk_widget_show (spbt_radius);
  gtk_table_attach (GTK_TABLE (table2), spbt_radius, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spbt_strength_adj = gtk_adjustment_new (0, -10, 10, 1, 1, 0);
  spbt_strength = gtk_spin_button_new (GTK_ADJUSTMENT (spbt_strength_adj), 1, 0);
  gtk_widget_show (spbt_strength);
  gtk_table_attach (GTK_TABLE (table2), spbt_strength, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
                    
  hs_radius = gtk_hscale_new (GTK_ADJUSTMENT (spbt_radius_adj));
  gtk_widget_show (hs_radius);
  gtk_table_attach (GTK_TABLE (table2), hs_radius, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_radius), 0);                     

  hs_strength = gtk_hscale_new (GTK_ADJUSTMENT (spbt_strength_adj));
  gtk_widget_show (hs_strength);
  gtk_table_attach (GTK_TABLE (table2), hs_strength, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_scale_set_digits (GTK_SCALE (hs_strength), 0); 
 
  label4 = gtk_label_new ("<b>Recovery</b>");
  gtk_widget_show (label4);
  gtk_frame_set_label_widget (GTK_FRAME (frame3), label4);
  gtk_label_set_use_markup (GTK_LABEL (label4), TRUE);

  hbox6 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox6);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox6, TRUE, TRUE, 0);

  label2 = gtk_label_new ("Dust Spot Mask File:");
  //gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox6), label2, FALSE, FALSE, 0);

  entry1 = gtk_entry_new ();
  //gtk_widget_show (entry1);
  gtk_box_pack_start (GTK_BOX (hbox6), entry1, TRUE, TRUE, 0);
  gtk_entry_set_invisible_char (GTK_ENTRY (entry1), 9679);

  button1 = gtk_button_new_with_mnemonic ("Browse...");
  //gtk_widget_show (button1);
  gtk_box_pack_start (GTK_BOX (hbox6), button1, FALSE, FALSE, 0);


/*================ Initialization ==================*/
	if (para != NULL) {
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_scan_s_adj), para->sensitivity);
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_segm_s_adj), para->segmentationSensitivity);
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_width_adj), para->dustSize.width);
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_height_adj), para->dustSize.height);
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_ed_adj), para->margin);
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_strength_adj), para->recoveryStrength);		
		gtk_adjustment_set_value (GTK_ADJUSTMENT(spbt_radius_adj), para->kernelSize);
		if (para->op == DUST_DETECTION)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(rbt_detection), TRUE);
		else if (para->op == DUST_RECOVERY_INSTANTLY)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(rbt_recovery), TRUE);
		else if (para->op == DUST_RECOVERY_BY_MASK)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(rbt_file_recovery), TRUE);		
		else
			printf("ERROR! in dustcleaner_UI.c Line 340 \n");
	}



/*=========== ACTION ==========*/

/**/     
  g_signal_connect_swapped (preview, "invalidated",
                            G_CALLBACK (process_it),
                            drawable);
  g_signal_connect_swapped (spbt_scan_s_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_segm_s_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_width_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_height_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_ed_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_radius_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (spbt_strength_adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);                                                                                                                                                                        
  g_signal_connect_swapped (rbt_detection, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);                              
  g_signal_connect_swapped (rbt_recovery, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);  

  process_it (drawable, GIMP_PREVIEW (preview));

  g_signal_connect (spbt_scan_s_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->sensitivity));
  g_signal_connect (spbt_segm_s_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->segmentationSensitivity));
  g_signal_connect (spbt_width_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->dustSize.width));
  g_signal_connect (spbt_height_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->dustSize.height));
  g_signal_connect (spbt_ed_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->margin));
  g_signal_connect (spbt_radius_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->kernelSize));
  g_signal_connect (spbt_strength_adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &(para->recoveryStrength));
  g_signal_connect (rbt_detection, "toggled",
                            G_CALLBACK (setToDetectionMode),
                            para);   
  g_signal_connect (rbt_recovery, "toggled",
                            G_CALLBACK (setToRecoveryMode),
                            para);        
                    
	gtk_widget_show (dialog1);	
	gboolean run = (gimp_dialog_run (GIMP_DIALOG (dialog1)) == GTK_RESPONSE_OK);
	gtk_widget_destroy (dialog1);

  	return run;
}

static void setToDetectionMode(GtkObject *ob, TPara *para)
{
	para->op = DUST_DETECTION;
	return;
}

static void setToRecoveryMode(GtkObject *ob, TPara *para)
{
	para->op = DUST_RECOVERY_INSTANTLY;
	return;
}

static void setToRecoveryFromMaskMode(GtkObject *ob, TPara *para)
{
	para->op = DUST_RECOVERY_BY_MASK;
	return;
}

