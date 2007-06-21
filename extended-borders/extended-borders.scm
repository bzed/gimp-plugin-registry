;;; extended-border.scm - Gimp script to add configurable borders
;;; Copyright (C) 2004  Stig E Sandø
;;; 
;;; Based on photo-border.scm, (C) 2004  Pavel Antokolsky
;;; 
;;; THIS SOFTWARE IS IN THE PUBLIC DOMAIN.
;;;
;;; Permission to use, copy, modify, and distribute this software and its
;;; documentation for any purpose and without fee is hereby granted, without any
;;; conditions or restrictions.
;;; 
;;; THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
;;
;; History
;; -------
;; Version 1.0 by Pavel Antokolsky aka Zigmar (antokol@isdn.net.il)
;;    aka photo-border.scm
;;
;; Version 1.1 Stig E Sandø (fufie@boblycat.org)
;;    aka copyright-border.scm
;;             - Improved error-checking and undo works
;;             - Addded support for copyright-notice
;;             - Moved script to S-Fu/Decor
;;
;; Version 1.2 Stig E Sandø (fufie@boblycat.org)
;;    aka copyright-border.scm
;;             - Added blur-effect on copyright-notice
;;               as suggested by Eugene
;;
;; Version 1.4 Stig E Sandø (fufie@boblycat.org)
;;    aka extended-border.scm
;;             - Added support for titles
;;             - Added positioning/alignment of copyright+title
;;             - Made user-friendly toggle of copyright/title as
;;               suggested by Pavel.
;;
;; Version 1.5 Stig E Sandø (fufie@boblycat.org)
;;    aka extended-border.scm
;;             - Fixed a bug because layer was cleared before added to
;;               image.  This was needed in Gimp 2.2

(define (script-fu-extended-border-script original-image original-drawable
					  use-inner-border inner-border-width inner-border-colour
					  use-outer-border outer-border-width outer-border-colour
					  
					  use-title title-text title-font-name title-font-size
					  title-colour title-align blur-title
					  
					  use-copyright copyright-text copyright-font-name copyright-font-size
					  copyright-colour copyright-align blur-copyright
					  
					  work-on-copy flatten-image)

  ;; reset right away
  (if (= use-inner-border FALSE)
      (set! inner-border-width 0))
  
  ;; reset right away
  (if (= use-outer-border FALSE)
      (set! outer-border-width 0))
  
  ;; reset right away
  (if (< (string-length title-text) 1)
      (set! use-title FALSE))
  
  ;; reset right away
  (if (< (string-length copyright-text) 1)
      (set! use-copyright FALSE))
  
  
  ;; get hold of the other variables we need
  (let* ((image-type (car (gimp-drawable-type-with-alpha original-drawable)))
	 (image (cond ((= work-on-copy TRUE)
		       (car (gimp-image-duplicate original-image)))
		      (#t
		       original-image)))
	 
	 (old-image-width    (car (gimp-image-width image)))
	 (old-image-height   (car (gimp-image-height image)))
	 (old-foreground     (car (gimp-palette-get-foreground)))
	 (total-border-width (+ inner-border-width outer-border-width))
	 (new-image-width    (+ old-image-width  (* 2 total-border-width)))
	 (new-image-height   (+ old-image-height (* 2 total-border-width)))
	 (border-layer '())
	 
	 (+blur-factor+ 1.5)    ;; constant, tweak if you want it changed
	 (+border-opacity+ 100) ;; constant, tweak if you want it changed
	 )
     
      
      ;; Start undo, make it simple.
      (gimp-image-undo-group-start image)

      ;;Add space for the border ("canvas resize")
      (gimp-image-resize image new-image-width new-image-height
			 total-border-width total-border-width)

      ;; Create new layer for the border
      (set! border-layer (car (gimp-layer-new image 
					      new-image-width 
					      new-image-height 
					      image-type
					      "Extended-border" ;;; Layername
					      +border-opacity+  ;; Opacity
					      NORMAL ;; layer combo mode
					      )))

      ;; Add it to image
      (gimp-image-add-layer image border-layer -1)

      ;; Clear newly created layer
      (gimp-edit-clear border-layer)
            
      ;; Outer border fill, if it is bigger than zero
      (cond ((> outer-border-width 0)
	     (gimp-rect-select image 
			       0 0 
			       new-image-width new-image-height 
			       REPLACE 
			       0 0)
	     (gimp-rect-select image 
			       outer-border-width outer-border-width 
			       (- new-image-width  (* outer-border-width 2))
			       (- new-image-height (* outer-border-width 2)) 
			       SUB 
			       0 0) 
	     (gimp-palette-set-foreground outer-border-colour)
	     (gimp-edit-fill border-layer 0) 
	     ))
      
      ;; Inner border fill if width is > 0
      (cond ((> inner-border-width 0)
	     (gimp-rect-select image 
			       outer-border-width outer-border-width 
			       (- new-image-width  (* outer-border-width 2))
			       (- new-image-height (* outer-border-width 2)) 
			       REPLACE 
			       0 0) 
	     (gimp-rect-select image
			       total-border-width total-border-width 
			       old-image-width old-image-height
			       SUB 
			       0 0) 
	     (gimp-palette-set-foreground inner-border-colour)
	     (gimp-edit-fill border-layer 0) 

	     ))


      ;; add the title if it is wanted
      
      (cond ((= use-title TRUE)

	     (gimp-palette-set-foreground title-colour)

	     ;; need to get extent of written-out text
	     (let* ((title-text-size   (gimp-text-get-extents-fontname title-text title-font-size
								       PIXELS title-font-name))
		    (title-text-width  (car title-text-size))
		    (title-text-height (cadr title-text-size))
		    
		    ;; ("BOTTOM-CENTRE ob" "BOTTOM-LEFT ob" "TOP-CENTRE ob"
		    ;;  "TOP-LEFT ob" "TOP-RIGHT ob" "BOTTOM-RIGHT ob")
		    (align-vertical   (nth title-align '("bottom" "bottom" "top" "top" "top" "bottom")))
		    (align-horizontal (nth title-align '("left" "centre" "centre" "left" "right" "right")))

		    (x-coord (cond ((equal? align-horizontal "left")
				    outer-border-width)
				   ((equal? align-horizontal "right")
				    (- new-image-width title-text-width outer-border-width))
				   ((equal? align-horizontal "centre")
				    (- (/ (+ outer-border-width new-image-width) 2)
				       (/ title-text-width 2)))
				   (#t ;; default to right
				    (- new-image-width title-text-width outer-border-width))))
				       
		    (y-coord (cond ((equal? align-vertical "top") 0)
				   ((equal? align-vertical "bottom")
				    (- new-image-height title-text-height))
				   (#t ;; default to bottom
				    (- new-image-height title-text-height))))
				    
		    (written-title '())
		    )

	       
	       (set! written-title (gimp-text-fontname image
						      -1  ;; new layer
						      x-coord
						      y-coord
						      title-text
						      -1 
						      TRUE ;; anti-alias
						      title-font-size 
						      PIXELS ;; size is in pixels
						      title-font-name))

	       ;; if we're to blur anything, this is the time
	       (if (= blur-title TRUE)
		   (plug-in-gauss-rle TRUE image (car written-title) +blur-factor+ TRUE TRUE))
	       
	       )))


      ;; add the copyright if it is wanted
      
      (cond ((= use-copyright TRUE)
	     
	     (gimp-palette-set-foreground copyright-colour)
	     
	     ;; need to get extent of written-out text
	     (let* ((copyright-text-size   (gimp-text-get-extents-fontname copyright-text copyright-font-size
									   PIXELS copyright-font-name))
		    (copyright-text-width  (car copyright-text-size))
		    (copyright-text-height (cadr copyright-text-size))
		    
		    ;; ("BOTTOM-RIGHT ob" "BOTTOM-LEFT ob" "BOTTOM-CENTRE ob"
		    ;;  "BOTTOM-RIGHT pic" "BOTTOM-LEFT pic" "BOTTOM-CENTRE pic"
		    ;;  "TOP-LEFT ob" "TOP-RIGHT ob" "TOP-CENTRE ob")
		    (align-vertical   (nth copyright-align '("ob-bottom" "ob-bottom" "ob-bottom"
							     "pic-bottom" "pic-bottom" "pic-bottom"
							     "ob-top" "ob-top" "ob-top")))
		    (align-horizontal (nth copyright-align '("ob-right" "ob-left" "ob-centre"
							     "pic-right" "pic-left" "pic-centre"
							     "ob-left" "ob-right" "ob-centre")))
		    
		    (x-coord (cond ((equal? align-horizontal "ob-left")
				    outer-border-width)
				   ((equal? align-horizontal "ob-right")
				    (- new-image-width copyright-text-width outer-border-width))
				   ((equal? align-horizontal "ob-centre")
				    (- (/ (+ outer-border-width outer-border-width new-image-width) 2)
				       (/ copyright-text-width 2)))
				   ((equal? align-horizontal "pic-left")
				    total-border-width)
				   ((equal? align-horizontal "pic-right")
				    (- new-image-width copyright-text-width total-border-width))
				   ((equal? align-horizontal "pic-centre")
				    (- (/ (+ total-border-width total-border-width new-image-width) 2)
				       (/ copyright-text-width 2)))
				   (#t ;; default to ob-right
				    (- new-image-width copyright-text-width outer-border-width))))
				       
		    (y-coord (cond ((equal? align-vertical "ob-top")
				    0)
				   ((equal? align-vertical "ob-bottom")
				    (- new-image-height copyright-text-height))
				   ((equal? align-vertical "pic-top")
				    total-border-width)
				   ((equal? align-vertical "pic-bottom")
				    (- new-image-height total-border-width copyright-text-height))
				   (#t ;; default to bottom
				    (- new-image-height copyright-text-height))))
				    
		    (written-copyright '())
		    )

	       
	       (set! written-copyright (gimp-text-fontname image
							   -1  ;; new layer
							   x-coord
							   y-coord
							   copyright-text
							   -1 
							   TRUE ;; anti-alias
							   copyright-font-size 
							   PIXELS ;; size is in pixels
							   copyright-font-name))
	       
	       ;; if we're to blur anything, this is the time
	       (if (= blur-copyright TRUE)
		   (plug-in-gauss-rle TRUE image (car written-copyright) +blur-factor+ TRUE TRUE))
	       
	       )))


      ;; flatten
      (if (= flatten-image TRUE) (gimp-image-flatten image))

      ;; Finalisation
      (gimp-selection-none image)
      (gimp-palette-set-foreground old-foreground)

      ;; End UNDO
      (gimp-image-undo-group-end image)

      ;; Display our work
      (if (= work-on-copy TRUE) (gimp-display-new image))
      (gimp-displays-flush)
      ))

;; Default values are for really big pictures, scale down if you work with
;; small pictures.  Unfortunately these defaults can't figure out the size
;; of your image due to limitations in gimp.

(script-fu-register "script-fu-extended-border-script"
		    _"<Image>/Script-Fu/Decor/Extended Border script..."
		    "Add a very configurable border around an image"
		    "Frank Ufie (fufie@boblycat.org) [Based on photo-border by: Zigmar (antokol@isdn.net.il)]"
		    "Fufie (was: Zigmar)"
		    "05/11/2004"
		    ""
		    SF-IMAGE "Input Image" 0
		    SF-DRAWABLE "Input Drawable" 0
		    
		    SF-TOGGLE "Use inner border" TRUE
		    SF-ADJUSTMENT _"Inner border width" '(8 0 2000 1 10 0 1)
		    SF-COLOR _"Inner border colour" '(255 255 255)
		    
		    SF-TOGGLE "Use outer border" TRUE
		    SF-ADJUSTMENT _"Outer border width" '(60 0 2000 1 10 0 1)
		    SF-COLOR _"Outer border colour" '(0 0 0)

		    SF-TOGGLE "Add title" FALSE
		    SF-STRING _"Title" ""
		    SF-FONT   "Font" "Serif"
		    SF-ADJUSTMENT _"Font Size (pixels)" '(50 2 1000 1 10 0 1)
		    SF-COLOR _"Title colour" '(255 255 255)
		    SF-OPTION "Title position" '("BOTTOM LEFT in outer border"
						 "BOTTOM CENTRE in outer border"
						 "TOP CENTRE in outer border"
						 "TOP LEFT in outer border" 
						 "TOP RIGHT in outer border"
						 "BOTTOM RIGHT in outer border")
		    SF-TOGGLE "Blur title" TRUE
		    
		    
		    SF-TOGGLE "Add copyright notice" TRUE
		    SF-STRING _"Copyright" "\302\251 f ufie, 2005"
		    SF-FONT _"Font" "Utopia"
		    SF-ADJUSTMENT _"Font Size (pixels)" '(30 2 1000 1 10 0 1)
		    SF-COLOR _"Copyright colour" '(255 255 255)
		    SF-OPTION "Copyright position" '("BOTTOM RIGHT in outer border"
						     "BOTTOM LEFT in outer border"
						     "BOTTOM CENTRE in outer border"
						     "BOTTOM RIGHT in picture"
						     "BOTTOM LEFT in picture"
						     "BOTTOM CENTRE in picture"
						     "TOP LEFT in outer border"
						     "TOP RIGHT in outer border"
						     "TOP CENTRE in outer border")
		    SF-TOGGLE "Blur copyright" TRUE

		    
		    SF-TOGGLE "Work on copy" TRUE
		    SF-TOGGLE "Flatten image" TRUE
		    )
