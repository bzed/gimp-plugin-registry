;; Periodic Screenshot Saver, (C) 2007 Muthiah Annamalai
;;
;; Functionally this script tries to save the Images of screen
;; at periodic intervals, in file names like file1.png , file2.png etc.
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.
;; 
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


(define (minute-delay)
  ;;gives some type of minute delay
  (set! tmp 0)
  (set! tmpMAX 100)

  (while (< tmp tmpMAX) (set! tmp (+ 1 tmp)))
  0
)

(define (script-fu-periodic-screenshot nprfx fpath filefmt nshots interval offset progress)
  (set! itr 1)
  (set! t1 (realtime))
  (set! noninteractive 1)
  
  ;;FIXME
  ;;add some code to report 0-length file names or file paths.
  ;;

  ;;delay the initial offset (seconds)
  (while (<= (- (realtime) t1) offset) (minute-delay))

  (while (<= itr nshots)
   (set! fname (string-append fpath "/"  nprfx (number->string itr) filefmt))

   ;;delay before the screenshot
   (set! t1 (realtime))
   (while (<= (- (realtime) t1) interval) (minute-delay))

   ;; get screenshot
   (set! img (car (plug-in-screenshot noninteractive TRUE 0)) )
   (set! img_d (car (gimp-image-active-drawable img)) )
   
   ;; save it
   (gimp-file-save noninteractive img img_d  fname fname)

   ;;show the dialog if user wants it
   (if (= progress TRUE)
       (gimp-progress-update (/ itr nshots) ))
   (set! itr (+ itr 1))
  )
)

(script-fu-register "script-fu-periodic-screenshot"
		    "<Toolbox>/Xtns/Script-Fu/Utils/Periodic Screenshots"
		    "Saves Periodic screenshots (Whole screen) at given intervals of time.!"
		    "Muthiah Annamalai,"
		    "Credits: Muthiah Annamalai, 2007"
		    "2007"
		    ""
		    SF-STRING "FileName (Mandatory)" ""
		    SF-DIRNAME "Folder to store files (Mandatory)" ""
		    SF-STRING "Format / Extension" ".png"
		    SF-VALUE  "Number of screenshots" "5"
		    SF-VALUE  "Interval before each screenshot (in seconds)" "30"
		    SF-VALUE  "Interval before the starting process (in seconds)" "0"
		    SF-TOGGLE "Use Progress Bar" 0
)

;; EOF
