; The GIMP -- an image manipulation program
; Copyright (C) 1995 Spencer Kimball and Peter Mattis
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 3 of the License, or
; (at your option) any later version.
; 
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
; 
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
; http://www.gnu.org/licenses/gpl-3.0.html
;
; Copyright (C) 2008 elsamuko <elsamuko@web.de>
;


(define (elsamuko-color-tint aimg adraw color opacity)
  (let* ((img (car (gimp-drawable-get-image adraw)))
         (owidth (car (gimp-image-width img)))
         (oheight (car (gimp-image-height img)))
         (tint-layer (car (gimp-layer-copy adraw FALSE)))
         (tint-layer-mask 0)
         (red   (/ (car   color) 255))
         (blue  (/ (cadr  color) 255))
         (green (/ (caddr color) 255))
         )
    
    ; init
    (gimp-context-push)
    (gimp-image-undo-group-start img)
    (if (= (car (gimp-drawable-is-gray adraw )) TRUE)
        (gimp-image-convert-rgb img)
        )
    
    ;add tint layer and filter color
    (gimp-drawable-set-name tint-layer "Tint")
    (gimp-image-add-layer img tint-layer -1)
    (plug-in-colors-channel-mixer 1 img tint-layer TRUE
                                  red blue green ;R
                                  0 0 0 ;G
                                  0 0 0 ;B
                                  )
    
    ;add mask
    (set! tint-layer-mask (car (gimp-layer-create-mask tint-layer ADD-COPY-MASK)))
    (gimp-layer-add-mask tint-layer tint-layer-mask)

    ;colorize tint layer
    (gimp-context-set-foreground color)
    (gimp-selection-all img)
    (gimp-edit-bucket-fill tint-layer FG-BUCKET-FILL NORMAL-MODE 100 0 FALSE 0 0)
    
    ;set modes
    (gimp-layer-set-mode tint-layer SCREEN-MODE)
    (gimp-layer-set-opacity tint-layer opacity)
    
    ; tidy up
    (gimp-image-undo-group-end img)
    (gimp-displays-flush)
    (gimp-context-pop)
    )
  )

(script-fu-register "elsamuko-color-tint"
                    _"_Color Tint"
                    "Add color tint layer.
Latest version can be downloaded from http://registry.gimp.org/"
                    "elsamuko <elsamuko@web.de>"
                    "elsamuko"
                    "16/04/10"
                    "*"
                    SF-IMAGE       "Input image"          0
                    SF-DRAWABLE    "Input drawable"       0
                    SF-COLOR       "Color"              '(0   0  255)
                    SF-ADJUSTMENT _"Opacity"            '(100 0 100 5 10 0 0)
                    )

(script-fu-menu-register "elsamuko-color-tint" _"<Image>/Colors")
