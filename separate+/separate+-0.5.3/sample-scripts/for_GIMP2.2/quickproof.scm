( define ( black-overprinting srcimg destimg srclayer )
		 (let* ( ( destlayer 0 ) ( offset 0 ) ( mask 0 ) ( klayer 0 ) )
		   (set! offset ( gimp-drawable-offsets srclayer ) )
		   (set! destlayer (car ( gimp-layer-new-from-drawable srclayer destimg ) ) )
		   (set! mask (car ( gimp-layer-create-mask destlayer ADD-ALPHA-MASK ) ) )
		   ( gimp-drawable-delete destlayer )
		   (set! klayer (aref (cadr ( gimp-image-get-layers destimg ) ) 3 ) )
		   ( gimp-channel-combine-masks (car ( gimp-layer-get-mask klayer ) ) mask CHANNEL-OP-ADD (car offset ) (cadr offset ) )
		   )
		 )

( define ( script-fu-quick-proof img drawable filename1 filename2 filename3 toggle1 toggle2 toggle3 option1 option2 toggle4 toggle5 toggle6 )
		 (let* ( ( dirty 0 ) ( layer 0 ) ( cmykimg -1 ) ( proofimg -1 ) ( visible (car ( gimp-drawable-get-visible drawable ) ) ) )
		   (set! dirty (car ( gimp-image-is-dirty img ) ) )
		   ( gimp-image-undo-freeze img )
		   (if (= toggle1 TRUE ) ( gimp-drawable-set-visible drawable FALSE ) )

		   ( gimp-edit-copy-visible img )
		   (set! layer (car ( gimp-edit-paste drawable TRUE ) ) )
		   ( gimp-floating-sel-to-layer layer )
		   (set! cmykimg (car ( plug-in-separate-full 1 img layer filename1 filename2 toggle2 toggle3 option1 toggle4 toggle5 ) ) )

		   ( gimp-image-set-active-layer img drawable )
		   (if (= toggle1 TRUE ) ( gimp-drawable-set-visible drawable visible ) () )

		   ( gimp-image-remove-layer img layer )
		   ( gimp-image-undo-thaw img )
		   (if (= dirty TRUE ) () ( gimp-image-clean-all img ) )

		   (if (= cmykimg -1 )
			   ()
			 (begin
			  (if (= toggle1 TRUE )
				  ( black-overprinting img cmykimg drawable )
				()
				)
			  (set! proofimg (car( plug-in-separate-proof 1 cmykimg (car ( gimp-image-get-active-layer cmykimg ) ) filename3 filename2 option2 ) ) )
			  (if (= toggle6 TRUE ) ( gimp-display-new cmykimg ) ( gimp-image-delete cmykimg ) )
			  )
			 )
		   (if (= proofimg -1 ) () (begin ( gimp-display-new proofimg ) ( gimp-image-clean-all proofimg ) ) )
		   )
		 ( gimp-displays-flush )
		 )

( script-fu-register
 "script-fu-quick-proof"
 "<Image>/Image/Separate/Quickproof"
 "Separate and Proof automaticaly"
 "Yoshinori Yamakawa"
 "Yoshinori Yamakawa"
 "2007"
 "RGB*"
 SF-IMAGE     "Image"     0
 SF-DRAWABLE  "Drawable"  0
 SF-FILENAME "Source colorspace" ""
 SF-FILENAME "Dest/Print colorspace" ""
 SF-FILENAME "Monitor colorspace" ""
 SF-TOGGLE "Real black overprinting by the active layer" FALSE
 SF-TOGGLE "Preserve pure black" FALSE
 SF-TOGGLE "Overprint pure black" FALSE
 SF-OPTION "Rendering intent" '( "Perceptual" "Rel. colorimetric" "Saturation" "Abs. colorimetric" )
 SF-OPTION "Proof mode" '( "Normal" "Black ink simulation" "Media white simulation" )
 SF-TOGGLE "Use BPC algorithm for the separation" TRUE
 SF-TOGGLE "Use embedded source profile" TRUE
 SF-TOGGLE "Preserve separated image" FALSE )
