#!/usr/bin/env python

"""software_license
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
"""

# Based on Batch resize code by Carol Spears

# pdb.python_fu_contact_sheet("jpg","",True,"con",".jpg","","A4",300)

import os
import os.path
from gimpfu import *
from math import ceil

#these constants are the borders given in mm

mmLEFT_PAGE_BORDER = 5
mmRIGHT_PAGE_BORDER = 5
mmTOP_PAGE_BORDER = 5
mmBOTTOM_PAGE_BORDER = 5
mmTHUMB_MARGIN = 2
mmFONT_SIZE = 4


def Log(text):
    #f=file("/tmp/gimp.log","a+")
    #f.write(text+"\n")
    #f.close()
    return

def get_images(original_type, original_location):
    images = []
    
    for filename in os.listdir(original_location):

        basename, ext = os.path.splitext(filename)
        if ((len(ext)>0) and (ext in original_type)):
            imagefile = os.path.join(original_location, filename)
            original_image = {'base_name':basename,'image_file':imagefile}
            if os.path.isfile(imagefile):
                images.append(original_image)
                Log(str(original_image))
    return sorted(images)



def save_png(image, drawable, new_filelocation, use_comment):
    compression = 9
    interlace, bkgd = False, False
    gama, offs, phys = False, False, False
    time, svtrans = True, False
    pdb.file_png_save2(image, drawable, new_filelocation, new_filelocation, interlace, compression, bkgd, gama, offs, phys, time, use_comment, svtrans) 

def save_jpeg(image, name, comment=""):
    jpeg_save_defaults = (0.85, 0.0, 1, 0, "", 1, 0, 0, 0)
    args = list(jpeg_save_defaults)
    args[4] = comment

    pdb.file_jpeg_save(image, image.active_layer, name, name, *args)



def generate_thumb(filename,Thumb_width,Thumb_height):
    Log(filename)
    img = pdb.gimp_file_load(filename,filename)
    #now resize the loaded image proportionally
    if (img.width>img.height):
        #landscape so scale height proportionally
        ratio = img.width/float(img.height)
        new = (Thumb_width,int(Thumb_width/ratio))
        #if the new height exceed max thumb height then scale to thumb height
        if (new[1]>Thumb_height):
            new = (int(Thumb_height*ratio),Thumb_height)
    else:
        #portrate so scale width proportionally
        ratio = img.width/float(img.height)
        new = (int(Thumb_height*ratio),Thumb_height)
        if (new[0]>Thumb_width):
            new = (Thumb_width,int(Thumb_width/ratio))
    #now resize the image
    pdb.gimp_image_scale(img,new[0],new[1])
    return img

def CalcFontSize(text,Font,Size,CalcTextHeight,max_width):
    #this procedure calculates the text size to fit within the width param, the text is reduced until the width is small enough
    txtw,txtH,txte,txtd = pdb.gimp_text_get_extents_fontname(text,Size,PIXELS,Font)
    if (txtw<=max_width):
        return Size
    while ((txtw>max_width) and (Size>0)):
        Size = Size -1
        txtw,txtH,txte,txtd = pdb.gimp_text_get_extents_fontname(text,Size,PIXELS,Font)
    return Size
    
def Contact_Sheet(file_type, location, inc_filename, contact_name, contact_type, contact_location, contact_size, dpi,orient,num_col,num_rows):
    images = get_images(file_type,location)
    num_images = len(images)
    #print "Number of img files "  + str(len(images))
    #Log(str(images))
    #now make  a new drawing canvas of the correct size
    width,height = (209.9,297)
    if (contact_size =="A4"):
        width,height = (209.9,297)
    elif (contact_size == "Jumbo"):
        width,height = (102,152)
    elif (contact_size == "6x8"):
        width,height = (152,203)
    elif (contact_size == "8x10"): 
        width,height = (203,254)
    #Log(str(width))
    #Log(str(height))
    width = int((width/25.4)*int(dpi))
    height = int((height/25.4)*int(dpi))
    #calculate the required size for the thumbs based on the number of images per sheet
    #convert sizes to px
    LEFT_PAGE_BORDER = int((mmLEFT_PAGE_BORDER/25.4)*float(dpi))
    RIGHT_PAGE_BORDER = int((mmRIGHT_PAGE_BORDER/25.4)*float(dpi))
    TOP_PAGE_BORDER = int((mmTOP_PAGE_BORDER/25.4)*float(dpi))
    BOTTOM_PAGE_BORDER = int((mmBOTTOM_PAGE_BORDER/25.4)*float(dpi))
    THUMB_MARGIN = int((mmTHUMB_MARGIN/25.4)*float(dpi))
    FONT_SIZE = int((mmFONT_SIZE/25.4)*float(dpi))
    ThumbsPerSheet = num_col*num_rows
    img_no = 1
    for sheetcount in range(int(ceil(len(images)/float(ThumbsPerSheet)))):
    
        if (orient=="land"):
            sheetimg = gimp.Image(height,width,RGB)
            bklayer = gimp.Layer(sheetimg,"Background",height,width,RGB_IMAGE,100,NORMAL_MODE)
            sheetimg.disable_undo()
            sheetimg.add_layer(bklayer,0)
        else:
            sheetimg = gimp.Image(width,height,RGB)
            bklayer = gimp.Layer(sheetimg,"Background",width,height,RGB_IMAGE,100,NORMAL_MODE)
            sheetimg.disable_undo()
            sheetimg.add_layer(bklayer,0)
        #Log(str(sheetimg))
        #Log(str(sheetimg.resolution))
        #set the image resolution
        sheetimg.resolution = (float(dpi), float(dpi))
        #now calculate sizes
        Canvas_width = sheetimg.width - LEFT_PAGE_BORDER - RIGHT_PAGE_BORDER
        
        Canvas_height = sheetimg.height - TOP_PAGE_BORDER - BOTTOM_PAGE_BORDER
        #print ("Canvas width %d height %d" % ( Canvas_width,Canvas_height))
        
        #Log(str(sheetimg.resolution))
        #now fill with white
        bklayer.fill(WHITE_FILL)
        bklayer.flush()
        sheetdsp = gimp.Display(sheetimg)
        #print "sheet display" + str(sheetdsp)
        gimp.displays_flush()        
        
        txtw,CalcTextHeight,txte,txtd = pdb.gimp_text_get_extents_fontname("Sheet %03d of %03d" % (sheetcount+1,int(ceil(len(images)/float(ThumbsPerSheet)))),FONT_SIZE,PIXELS,"Arial")
        txtfloat = pdb.gimp_text_fontname(sheetimg, sheetimg.active_layer, LEFT_PAGE_BORDER, TOP_PAGE_BORDER-CalcTextHeight, "Sheet %03d of %03d"  % (sheetcount+1,int(ceil(len(images)/float(ThumbsPerSheet)))), -1, False, FONT_SIZE, PIXELS, "Arial")
        pdb.gimp_floating_sel_anchor(txtfloat)
        
        CalcTextHeight =0
        txtw,txth,txte,txtd = (0,0,0,0)
        if (inc_filename == True):
            txtw,CalcTextHeight,txte,txtd = pdb.gimp_text_get_extents_fontname(images[0]['base_name'],FONT_SIZE,PIXELS,"Arial")
        
            
        #print "CalcText Height %d " %(CalcTextHeight)
        Thumb_width = (Canvas_width/num_col)-2*THUMB_MARGIN
        Thumb_height = Canvas_height/num_rows-2*THUMB_MARGIN - CalcTextHeight
        

        
        files = images[sheetcount*ThumbsPerSheet:(sheetcount+1)*ThumbsPerSheet]
        #now for each of the image files generate a thumbnail
        rcount = 0
        ccount = 0
        #generate thumb 
        for file in files:
            thumbimg=generate_thumb(file['image_file'],Thumb_width,Thumb_height)
            cpy = pdb.gimp_edit_copy(thumbimg.active_layer)
            gimp.delete(thumbimg)
            #now paste the new thumb into contact sheet
            newselect = pdb.gimp_edit_paste(sheetimg.active_layer,True)
            #print str(newselect)
            #print str(newselect.offsets)
            #positition in top left corner 
            newselect.translate(-newselect.offsets[0],-newselect.offsets[1])
            #now position in correct position
            xpos = LEFT_PAGE_BORDER + ccount * (Thumb_width + (2 * THUMB_MARGIN)) + THUMB_MARGIN
            ypos = TOP_PAGE_BORDER + rcount * (Thumb_height + (2 * THUMB_MARGIN)+ CalcTextHeight) + THUMB_MARGIN 
            newselect.translate(xpos,ypos)
            pdb.gimp_floating_sel_anchor(newselect)
            
            if (inc_filename == True):
                Size = CalcFontSize(file['base_name'],"Arial",FONT_SIZE,CalcTextHeight,Thumb_width)
                txtfloat = pdb.gimp_text_fontname(sheetimg, sheetimg.active_layer, xpos-THUMB_MARGIN, ypos+Thumb_height+THUMB_MARGIN, file['base_name'], -1, False, Size, PIXELS, "Arial")
                pdb.gimp_floating_sel_anchor(txtfloat)
                
            ccount = ccount + 1
            if (ccount>= num_col):
                ccount = 0
                rcount = rcount + 1
            gimp.displays_flush()

        
        contact_filename = contact_name + "_%03d" % (sheetcount) + contact_type
        contact_full_filename = os.path.join(contact_location, contact_filename)
        #print "File to save " + contact_full_filename
        if (contact_type == ".jpg"):
            #save_jpeg(sheetimg,contact_full_filename,"Created with the GIMP and Robin Gilham's Contact sheet plugin")
            save_jpeg(sheetimg,contact_full_filename,"")
        else:
            save_png(sheetimg,pdb.gimp_image_get_active_drawable(sheetimg),contact_full_filename,False)
        gimp.delete(sheetimg)
        pdb.gimp_display_delete(sheetdsp) 




register(
        "python_fu_contact_sheet",
        "Generates a contact sheet(s) for a directory of images.\nIf you find this script useful or any bugs I would love to hear from you robin.gilham@gmail.com\nHeck you could even consider a donation",
        "Generates contact sheet(s) with a configurable number of thumbnails for all files located in a directory",
        "Robin Gilham",
        "Licensed under the GPL v2",
        "2008",
        "<Toolbox>/Xtns/Batch/Contact Sheet",
        "",
        [
        (PF_RADIO, "file_type", "File type:", ".jpg .jpeg .JPG", (("jpg", ".jpg .jpeg .JPG"), ("png", ".png .PNG"), ("tiff", ".tiff .tif .TIF .TIFF"))),
        (PF_DIRNAME, "location", "Generate contact sheet of all files in this directory", ""),
        (PF_BOOL, "inc_filename", "Include filename on contact sheet", True),
        (PF_STRING, "contact_name",  "Contact sheet base name:", "contact_sheet"),
        (PF_RADIO, "contact_type", "Contact sheet image type:", ".jpg", (("jpg", ".jpg"), ("png", ".png"))),
        (PF_DIRNAME, "contact_location", "Where the contact sheet should be saved", ""),
        (PF_RADIO, "contact_size", "Contact page sheet size:", "A4", (("A4 (20.9x29.7 cm)", "A4"), ("Jumbo (10.2x15.2 cm)", "Jumbo"), ("6x8 (15.2x20.3 cm)", "6x8"),("8x10 (20.3x25.4 cm)", "8x10"))),
        (PF_RADIO, "dpi", "Contact sheet resolution", "72", (("72 dpi", "72"), ("300 dpi","300"))),
        (PF_RADIO, "orient", "Orientation:", "land", (("portrate", "port"), ("landscape","land"))),
        (PF_SPINNER, "num_col", "Number of images per row", 4, (1,32,1)),
        (PF_SPINNER, "num_rows", "Number of rows", 4, (1,32,1))
        ],
        [],
        Contact_Sheet)


main()
