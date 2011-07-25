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

( define ( script-fu-qproof img drawable mode tgl_bpc tgl_real_overprint tgl_preserve tgl_overprint tgl_keep_img )
		 (let* ( ( dirty 0 ) ( buffer 0 ) ( layer 0 ) ( cmykimg -1 ) ( proofimg -1 ) ( visible (car ( gimp-drawable-get-visible drawable ) ) ) )
		   (set! dirty (car ( gimp-image-is-dirty img ) ) )
		   ( gimp-image-undo-freeze img )
		   (if (= tgl_real_overprint TRUE ) ( gimp-drawable-set-visible drawable FALSE ) )

		   (set! buffer (car ( gimp-edit-named-copy-visible img "script-fu-qproof/temp" ) ) )
		   (set! layer (car ( gimp-edit-named-paste drawable buffer TRUE ) ) )
		   ( gimp-buffer-delete buffer )
		   ( gimp-floating-sel-to-layer layer )
		   (set! cmykimg (car ( plug-in-separate-full 1 img layer "" TRUE "" -1 tgl_bpc tgl_preserve tgl_overprint TRUE ) ) )

		   ( gimp-image-set-active-layer img drawable )
		   (if (= tgl_real_overprint TRUE ) ( gimp-drawable-set-visible drawable visible ) () )

		   ( gimp-image-remove-layer img layer )
		   ( gimp-image-undo-thaw img )
		   (if (= dirty TRUE ) () ( gimp-image-clean-all img ) )

		   (if (= cmykimg -1 )
			   ()
			 (begin
			  (if (= tgl_real_overprint TRUE )
				  ( black-overprinting img cmykimg drawable )
				()
				)
			  (set! proofimg (car( plug-in-separate-proof 1 cmykimg (car ( gimp-image-get-active-layer cmykimg ) ) "" "" mode TRUE ) ) )
			  (if (= tgl_keep_img TRUE ) ( gimp-display-new cmykimg ) ( gimp-image-delete cmykimg ) )
			  )
			 )
		   (if (= proofimg -1 ) () (begin ( gimp-display-new proofimg ) ( gimp-image-clean-all proofimg ) ) )
		   )
		 ( gimp-displays-flush )
		 )

( script-fu-register
 "script-fu-qproof"
 "<Image>/Image/Separate/QProof"
 "Genarate the softproofing image quickly"
 "Yoshinori Yamakawa"
 "Yoshinori Yamakawa"
 "2007-2009"
 "RGB*"
 SF-IMAGE     "Image"     0
 SF-DRAWABLE  "Drawable"  0
 SF-OPTION "Mode" `( "Normal" "Simulate black ink" "Simulate media white" )
 SF-TOGGLE "Use BPC algorithm for the separation" TRUE
 SF-TOGGLE "Real black overprinting by the active layer" FALSE
 SF-TOGGLE "Preserve pure black" FALSE
 SF-TOGGLE "Overprint pure black" FALSE
 SF-TOGGLE "Preserve separated image" FALSE )
