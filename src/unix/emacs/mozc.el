;; Copyright 2010-2014, Google Inc.
;; All rights reserved.
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;
;;     * Redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above
;; copyright notice, this list of conditions and the following disclaimer
;; in the documentation and/or other materials provided with the
;; distribution.
;;     * Neither the name of Google Inc. nor the names of its
;; contributors may be used to endorse or promote products derived from
;; this software without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

;; Keywords: mule, multilingual, input method

;;; Commentary:

;; mozc-mode is a minor mode to input Japanese text using Mozc server.
;; mozc-mode directly communicates with Mozc server and you can use
;; all features of Mozc, such as suggestion and prediction as well as
;; regular conversion.

;; Supported Emacs version and environment:
;;
;; mozc-mode supports Emacs version 22.1 and later.

;; How to set up mozc-mode:
;;
;; 1. Environment settings
;;
;; You need to install a helper program called 'mozc_emacs_helper'.
;; You may need to set PATH environment variable to the program, or
;; set `mozc-helper-program-name' to the path to the program.
;; You need to install this mozc.el as well.
;;
;; Most of Unix-like distributions install the helper program to
;; /usr/bin/mozc_emacs_helper and mozc.el to an appropriate site-lisp
;; directory (/usr/share/emacs/site-lisp/emacs-mozc for example)
;; by default, and you may have nothing to do on your side.
;;
;; 2. Settings in your init file
;;
;; mozc-mode supports LEIM (Library of Emacs Input Method) and
;; you only need the following settings in your init file
;; (~/.emacs.d/init.el or ~/.emacs).
;;
;;   (require 'mozc)  ; or (load-file "/path/to/mozc.el")
;;   (setq default-input-method "japanese-mozc")
;;
;; Having the above settings, just type \C-\\ which is bound to
;; `toggle-input-method' by default.
;;
;; Note for advanced users:
;; mozc-mode is provided as a minor-mode and it's able to work
;; without LEIM.  You can directly enable mozc-mode by running
;; `mozc-mode' command.
;;
;; 3. Customization
;;
;; 3.1. Server-side customization
;;
;; By the design policy, Mozc maintains most of user settings on
;; the server side.  Clients, including mozc.el, of Mozc do not
;; have many user settings on their side.
;;
;; You can change a variety of user settings through a GUI command
;; line tool 'mozc_tool' which must be shipped with the mozc server.
;; The command line tool may be installed to /usr/lib/mozc or /usr/lib
;; directory.
;; You need a command line option '--mode=config_dialog' as the
;; following.
;;
;;   $ /usr/lib/mozc/mozc_tool --mode=config_dialog
;;
;; Then, it shows a GUI dialog to edit your user settings.
;;
;; Note these settings are effective for all the clients of Mozc,
;; not limited to mozc.el.
;;
;; 3.2. Client-side customization.
;;
;; Only the customizable item on mozc.el side is the key map for kana
;; input.  When you've chosen kana input rather than roman input,
;; a kana key map is effective, and you can customize it.
;;
;; There are two built-in kana key maps, one for 106 JP keyboards and
;; one for 101 US keyboards.  You can choose one of them by setting
;; `mozc-keymap-kana' variable.
;;
;;   ;; for 106 JP keyboards
;;   (setq mozc-keymap-kana mozc-keymap-kana-106jp)
;;
;;   ;; for 101 US keyboards
;;   (setq mozc-keymap-kana mozc-keymap-kana-101us)
;;
;; For advanced users, there are APIs for more detailed customization
;; or even creating your own key map.
;; See `mozc-keymap-get-entry', `mozc-keymap-put-entry',
;; `mozc-keymap-remove-entry', and `mozc-keymap-make-keymap' and
;; `mozc-keymap-make-keymap-from-flat-list'.

;;; Code:


(eval-when-compile
  (require 'cl))



;;;; Customization group

(defgroup mozc nil
  "Mozc - Japanese input mode package."
  :group 'leim)

(defgroup mozc-faces nil
  "Faces for showing the preedit and candidates."
  :group 'mozc)



;;;; Macros

(defmacro mozc-with-undo-list-unchanged (&rest body)
  "Evaluate BODY forms without changing the undo list.
Return value of last one."
  `(let ((buffer-undo-list t))  ; Hide the original `buffer-undo-list'.
     ,@body))

(defmacro mozc-with-buffer-modified-p-unchanged (&rest body)
  "Evaluate BODY forms without changing the buffer modified status."
  `(let ((buffer-modified (buffer-modified-p)))
     (unwind-protect
         (progn ,@body)
       (if (and (not buffer-modified) (buffer-modified-p))
           (restore-buffer-modified-p nil)))))

(defmacro mozc-characterp (object)
  "Return non-nil if OBJECT is a character.

This macro is equivalent to `characterp' or `char-valid-p' depending on
the Emacs version.  `char-valid-p' is deprecated since Emacs 23."
  (if (fboundp #'characterp)
      `(characterp ,object)
    `(char-valid-p ,object)))



;;;; External interfaces

;; Mode variables
(defvar mozc-mode nil
  "Mode variable of mozc-mode.  Non-nil means mozc-mode is enabled.")
(make-variable-buffer-local 'mozc-mode)

(defvar mozc-mode-hook nil
  "A list of hooks called by the command `mozc-mode'.")

;; Mode functions
(defun mozc-mode (&optional arg)
  "Minor mode to input Japanese text with Mozc.
Toggle the mode if ARG is not given, or enable/disable the mode
according to ARG.

Hooks in `mozc-mode-hook' are run when the mode gets enabled.

Return non-nil when enabled, otherwise nil."
  (interactive (list current-prefix-arg))
  ;; Process the argument.
  (setq mozc-mode
        (if (null arg)
            (not mozc-mode)
          (> (prefix-numeric-value arg) 0)))

  (if (not mozc-mode)
      ;; disabled
      (mozc-clean-up-session)
    ;; enabled
    ;; Put the keymap at the top of the list of minor mode keymaps.
    (setq minor-mode-map-alist
          (cons (cons 'mozc-mode mozc-mode-map)
                (assq-delete-all 'mozc-mode minor-mode-map-alist)))
    ;; Run minor mode hooks.
    (run-hooks 'mozc-mode-hook)
    ;; Create a new session.
    (mozc-session-create t))

  mozc-mode)

(defsubst mozc-abort ()
  "Disable mozc-mode abnormally."
  (setq mozc-mode nil))

;; Mode line
(defcustom mozc-mode-string nil
  "Mode line string shown when mozc-mode is enabled.
Since LEIM shows another mode line indicator, the default value is nil.
If you don't want to use LEIM, you might want to specify a mode line
string, for instance, \" [Mozc]\"."
  :type '(choice (const :tag "No indicator" nil)
                 (string :tag "Show an indicator"))
  :group 'mozc)

(setq minor-mode-alist
      (cons '(mozc-mode mozc-mode-string)
            (assq-delete-all 'mozc-mode minor-mode-alist)))



;;;; Keymap

(defvar mozc-mode-map
  (let ((map (make-sparse-keymap)))
    ;; Leave single key bindings bound to specific commands as it is.
    ;; \M-x and \C-\\, which are single key binding, are usually bound to
    ;; `execute-extended-command' and `toggle-input-method' respectively.
    ;; Do not change those key bindings so users can disable mozc-mode.
    (mapc
     (lambda (command)
       (mapc
        (lambda (key-sequence)
          (and (= (length key-sequence) 1)
               (integerp (aref key-sequence 0))
               (define-key map key-sequence command)))
        (where-is-internal command global-map)))
     '(execute-extended-command toggle-input-method))

    ;; Hook all other input events.
    ;; NOTE: It's possible that there is no key binding to disable mozc-mode
    ;; since this key map hooks all key events except for above ones.
    ;; Though it usually doesn't matter since Mozc server usually doesn't
    ;; consume \M-x nor \C-\\.
    (define-key map [t] 'mozc-handle-event)
    map)
  "Keymap for mozc-mode.")

(defconst mozc-empty-map (make-sparse-keymap)
  "Empty keymap to disable Mozc keymap.
This keymap is needed for `mozc-fall-back-on-default-binding'.")

(defsubst mozc-enable-keymap ()
  "Enable Mozc keymap again."
  (setcdr (assq 'mozc-mode minor-mode-map-alist)
          mozc-mode-map))

(defsubst mozc-disable-keymap ()
  "Disable Mozc keymap temporarily."
  (setcdr (assq 'mozc-mode minor-mode-map-alist)
          mozc-empty-map))



;;;; Key event handling

(defun mozc-handle-event (event)
  "Pass all key inputs to Mozc server and render the resulting response.
If Mozc server didn't consume a key event, try to process the key event
without Mozc finding another command bound to the key sequence.

EVENT is the last input event, which is usually passed by the command loop."
  (interactive (list last-command-event))
  (cond
   ;; Keyboard event
   ((or (integerp event) (symbolp event))
    (let ((output (mozc-send-key-event event)))
      (cond
       ((null output)  ; Error occurred.
        (mozc-clean-up-session)  ; Discard the current session.
        (mozc-abort)
        (signal 'mozc-response-error output))

       ;; Mozc server consumed the key event.
       ((mozc-protobuf-get output 'consumed)
        (let ((result (mozc-protobuf-get output 'result))
              (preedit (mozc-protobuf-get output 'preedit))
              (candidates (mozc-protobuf-get output 'candidates)))
          (if (not (or result preedit))
              (mozc-clean-up-changes-on-buffer)  ; nothing to show
            (when result  ; Insert the result first.
              (mozc-clean-up-changes-on-buffer)
              (unless (eq (mozc-protobuf-get result 'type) 'string)
                (message "mozc.el: Unknown result type")
                (signal 'mozc-type-error `('string
                                           ,(mozc-protobuf-get result 'type))))
              (insert (mozc-protobuf-get result 'value)))
            (if preedit  ; Update the preedit.
                (mozc-preedit-update preedit candidates)
              (mozc-preedit-clear))
            (if candidates  ; Update the candidate window.
                (mozc-candidate-update candidates)
              (mozc-candidate-clear)))))

       (t  ; Mozc server didn't consume the key event.
        (mozc-clean-up-changes-on-buffer)
        ;; Process the key event as if Mozc didn't hook the key event.
        (mozc-fall-back-on-default-binding event)))))

   ;; Other events
   (t
    ;; Fall back on a default binding.
    ;; Leave the current preedit and candidate window as it is.
    (mozc-fall-back-on-default-binding event))))

(defun mozc-send-key-event (event)
  "Send a key event EVENT and return the resulting protobuf.
The resulting protocol buffer, which is represented as alist, is
mozc::commands::Output."
  (let* ((key-and-modifiers (mozc-key-event-to-key-and-modifiers event))
         (key (car key-and-modifiers))
         (keymap (mozc-keymap-current-active-keymap))
         (str (and (null (cdr key-and-modifiers))
                   (mozc-keymap-get-entry keymap key))))
    (mozc-session-sendkey (if str
                              (list key str)
                            key-and-modifiers))))

(defun mozc-key-event-to-key-and-modifiers (event)
  "Convert a keyboard event EVENT to a list of key and modifiers.
Key code and symbols are renamed so that the helper process understands them."
  (let ((basic-type (event-basic-type event))
        (modifiers (event-modifiers event)))
    ;; Rename special keys to ones the helper process understands.
    (let ((key (case basic-type
                 (?\b 'backspace)
                 (?\s 'space)
                 (?\d 'backspace)
                 ('eisu-toggle 'eisu)
                 ('hiragana-katakana 'kana)
                 ('next 'pagedown)
                 ('prior 'pageup)
                 ('kp-decimal 'decimal)
                 ('kp-0 'numpad0)
                 ('kp-1 'numpad1)
                 ('kp-2 'numpad2)
                 ('kp-3 'numpad3)
                 ('kp-4 'numpad4)
                 ('kp-5 'numpad5)
                 ('kp-6 'numpad6)
                 ('kp-7 'numpad7)
                 ('kp-8 'numpad8)
                 ('kp-9 'numpad9)
                 ('kp-delete 'delete)  ; .
                 ('kp-insert 'insert)  ; 0
                 ('kp-end 'right)      ; 1
                 ('kp-down 'down)      ; 2
                 ('kp-next 'pagedown)  ; 3
                 ('kp-left 'left)      ; 4
                 ('kp-begin 'clear)    ; 5
                 ('kp-right 'right)    ; 6
                 ('kp-home 'home)      ; 7
                 ('kp-up 'up)          ; 8
                 ('kp-prior 'pageup)   ; 9
                 ('kp-add 'add)
                 ('kp-subtract 'subtract)
                 ('kp-multiply 'multiply)
                 ('kp-divide 'divide)
                 ('kp-enter 'enter)
                 (t basic-type))))
      (cond
       ;; kana + shift + rest => katakana + rest
       ((and (eq key 'kana) (memq 'shift modifiers))
        (cons 'katakana (remq 'shift modifiers)))
       ;; lowercase + shift => uppercase
       ((and (mozc-characterp key) (equal modifiers '(shift))
             (/= key (upcase key)))
        (cons (upcase key) nil))
       (t
        (cons key modifiers))))))

(defun mozc-fall-back-on-default-binding (last-event)
  "Execute a command as if the command loop does.
Read a complete set of user input and execute the command bound to
the key sequence in almost the same way of the command loop.

LAST-EVENT is the last event which a user has input.  The last event is pushed
back and treated as if it's the first event of a next key sequence."
  (unwind-protect
      (progn
        ;; Disable the keymap in this unwind-protect.
        (mozc-disable-keymap)
        ;; Push back the last event on the event queue.
        (and last-event (push last-event unread-command-events))
        ;; Read and execute a command.
        (let* ((keys (read-key-sequence-vector nil))
               (bind (key-binding keys t)))
          ;; Pretend `mozc-handle-event' command was not running and just
          ;; the default binding is running.
          (setq last-command-event (aref keys (1- (length keys))))
          (setq this-command bind)
          (if bind
              (call-interactively bind nil keys)
            (let (message-log-max)
              (message "%s is undefined" (key-description keys))
              (undefined)))))
    ;; Recover the keymap.
    (mozc-enable-keymap)))



;;;; Cleanup

(defsubst mozc-clean-up-session ()
  "Clean up all changes on the current buffer and destroy the current session."
  (mozc-clean-up-changes-on-buffer)
  (mozc-session-delete)
  (mozc-clear-cached-header-line-height))

(defsubst mozc-clean-up-changes-on-buffer ()
  "Clean up all changes on the current buffer.
Clean up the preedit and candidate window."
  (mozc-preedit-clean-up)
  (mozc-candidate-clean-up))



;;;; Modifications on the buffer

(defvar mozc-buffer-placeholder-char ?*
  "The default character to be used as a placeholder of overlays.")

(defmacro mozc-buffer-placeholder-setq (symbol &rest args)
  "Insert a placeholder and store its region in SYMBOL.
ARGS are either strings or characters.  ARGS defaults to
`mozc-buffer-placeholder-char'."
  `(progn
     (mozc-buffer-delete-region ,symbol)
     (setq ,symbol (mozc-buffer-insert ,@args))))

(defmacro mozc-buffer-placeholder-setq-char (symbol &optional character count)
  "Insert a placeholder and store its region in SYMBOL.
CHARACTER is the character to be inserted and defaults to
`mozc-buffer-placeholder-char'.  COUNT is the number of copies to be inserted
and defaults to 1."
  `(progn
     (mozc-buffer-delete-region ,symbol)
     (setq ,symbol (mozc-buffer-insert-char ,character ,count))))

(defmacro mozc-buffer-placeholder-push (symbol &rest args)
  "Insert a placeholder and add its region to SYMBOL.
ARGS are either strings or characters.  ARGS defaults to
`mozc-buffer-placeholder-char'."
  `(push (mozc-buffer-insert ,@args) ,symbol))

(defmacro mozc-buffer-placeholder-push-char (symbol &optional character count)
  "Insert a placeholder and add its region to SYMBOL.
CHARACTER is the character to be inserted and defaults to
`mozc-buffer-placeholder-char'.  COUNT is the number of copies to be inserted
and defaults to 1."
  `(push (mozc-buffer-insert-char ,character ,count) ,symbol))

(defmacro mozc-buffer-placeholder-delete (symbol)
  "Delete a placeholder pointed to by SYMBOL.
SYMBOL is set to nil after the deletion."
  `(progn
     (mozc-buffer-delete-region ,symbol)
     (setq ,symbol nil)))

(defmacro mozc-buffer-placeholder-delete-all (symbol)
  "Delete all placeholders in SYMBOL.
SYMBOL is set to nil after the deletion."
  `(progn
     (mozc-buffer-delete-all-regions ,symbol)
     (setq ,symbol nil)))

(defun mozc-buffer-insert (&rest args)
  "Insert ARGS, either strings or characters, and return the region of them.
The undo list will not be affected.  ARGS defaults to
`mozc-buffer-placeholder-char'.

The return value is a cons which holds two markers which point to the region of
added characters."
  (let ((beg (point-marker)))
    (mozc-with-undo-list-unchanged
     (if args
         (apply #'insert args)
       (insert mozc-buffer-placeholder-char)))
    (cons beg (point-marker))))

(defun mozc-buffer-insert-char (&optional character count)
  "Insert CHARACTERs and return the region of them.
COUNT copies of CHARACTER are inserted.  The undo list will not be affected.
CHARACTER and COUNT default to `mozc-buffer-placeholder-char' and 1
respectively.

The return value is a cons which holds two markers which point to the region of
added characters."
  (let ((beg (point-marker)))
    (mozc-with-undo-list-unchanged
     (insert-char (or character mozc-buffer-placeholder-char) (or count 1)))
    (cons beg (point-marker))))

(defun mozc-buffer-delete-region (region)
  "Delete the text in the REGION."
  (when region
    (mozc-with-undo-list-unchanged
     (delete-region (car region) (cdr region)))))

(defun mozc-buffer-delete-all-regions (regions)
  "Delete each text in the REGIONS.
REGIONS must be a list of regions."
  (mapc #'mozc-buffer-delete-region regions))



;;;; Preedit

(defvar mozc-preedit-in-session-flag nil
  "Non-nil means the current buffer has a preedit session.")
(make-variable-buffer-local 'mozc-preedit-in-session-flag)

(defvar mozc-preedit-point-origin nil
  "This points to the insertion point in the current buffer.")
(make-variable-buffer-local 'mozc-preedit-point-origin)

(defvar mozc-preedit-posn-origin nil
  "Position information at the insertion point in the current buffer.")
(make-variable-buffer-local 'mozc-preedit-posn-origin)

(defun mozc-preedit-init ()
  "Initialize a new preedit session.
The preedit shows up at the current point."
  (mozc-preedit-clean-up)
  ;; Set the origin point.
  (setq mozc-preedit-point-origin (copy-marker (point)))
  ;; Set up an overlay for preedit.
  (mozc-preedit-overlay-make-overlay mozc-preedit-point-origin)
  (setq mozc-preedit-in-session-flag t))

(defun mozc-preedit-clean-up ()
  "Clean up the current preedit session."
  (mozc-preedit-overlay-delete-overlay)
  (when mozc-preedit-in-session-flag
    (goto-char mozc-preedit-point-origin))
  (setq mozc-preedit-point-origin nil
        mozc-preedit-in-session-flag nil))

(defsubst mozc-preedit-clear ()
  "Clear the current preedit.
This function expects an update just after this call.
If you want to finish a preedit session, call `mozc-preedit-clean-up'."
  (when mozc-preedit-in-session-flag
    (mozc-preedit-overlay-put-text nil)))

(defun mozc-preedit-update (preedit &optional candidates)
  "Update the current preedit.
PREEDIT must be the preedit field in a response protobuf.
CANDIDATES must be the candidates field in a response protobuf if any."
  (unless mozc-preedit-in-session-flag
    (mozc-preedit-init))  ; Initialize if necessary.

  (let ((text
         (apply
          (if (and (not (eq (mozc-protobuf-get candidates 'category)
                            'conversion))
                   (= (length (mozc-protobuf-get preedit 'segment)) 1))
              ;; Show the unsegmented preedit with the cursor highlighted.
              #'mozc-preedit-make-text
            ;; Show the segmented preedit.
            #'mozc-preedit-make-segmented-text)
          preedit
          (when (memq 'fence mozc-preedit-style)
            '("|" "|" " ")))))
    (if (and buffer-read-only (> (length text) 0))
        (progn
          ;; Reset the session and throw away the current preedit, but keep
          ;; the helper process running and connected.
          (mozc-clean-up-session)
          (barf-if-buffer-read-only))
      ;; Update the position information at the beginning of the preedit.
      (mozc-preedit-overlay-put-text nil)
      (setq mozc-preedit-posn-origin
            (mozc-posn-at-point mozc-preedit-point-origin))
      ;; Show the preedit.
      (mozc-preedit-overlay-put-text text)
      ;; Move the cursor onto the preedit overlay or to the following position.
      (goto-char (if (text-property-not-all 0 (length text) 'cursor nil text)
                     mozc-preedit-point-origin
                   (1+ mozc-preedit-point-origin))))))

(defvar mozc-preedit-overlay nil
  "An overlay which shows the preedit.")
(make-variable-buffer-local 'mozc-preedit-overlay)

(defvar mozc-preedit-overlay-placeholder-region nil
  "A region which is temporarily added for showing the preedit.
This is a cons which holds two markers which point to the region of
a temporarily added character.")
(make-variable-buffer-local 'mozc-preedit-overlay-placeholder-region)

(defun mozc-preedit-overlay-make-overlay (origin)
  "Create a new overlay at ORIGIN to show the preedit.
The preedit is stored in `mozc-preedit-overlay' and removed by
`mozc-preedit-overlay-delete-overlay'."
  (mozc-preedit-overlay-delete-overlay)
  (save-excursion
    (goto-char origin)
    (mozc-buffer-placeholder-setq mozc-preedit-overlay-placeholder-region))
  (setq mozc-preedit-overlay
        (make-overlay (car mozc-preedit-overlay-placeholder-region)
                      (cdr mozc-preedit-overlay-placeholder-region)
                      (marker-buffer origin))))

(defun mozc-preedit-overlay-put-text (text)
  "Change the display property of the preedit overlay to TEXT."
  (overlay-put mozc-preedit-overlay 'display text))

(defun mozc-preedit-overlay-delete-overlay ()
  "Remove the preedit overlay and the placeholder region."
  (when mozc-preedit-overlay
    (delete-overlay mozc-preedit-overlay)
    (setq mozc-preedit-overlay nil))
  (mozc-buffer-placeholder-delete mozc-preedit-overlay-placeholder-region))

(defun mozc-preedit-make-text (preedit &optional decor-left decor-right
                                       separator)
  "Compose a preedit text and set its text properties.
Return the composed preedit text.

PREEDIT must be the preedit field in a response protobuf.
DECOR-LEFT and DECOR-RIGHT are added at both ends of the text.
SEPARATOR will never be used.  This unused parameter exists just to have
the compatible parameter list as `mozc-preedit-make-segmented-text'."
  (let* ((text (mozc-protobuf-get preedit 'segment 0 'value))
         (cursor (max 0 (min (mozc-protobuf-get preedit 'cursor)
                             (length text)))))
    (concat decor-left  ; left decoration
            (propertize (mozc-preedit-put-cursor-at text cursor)  ; preedit text
                        'face 'mozc-preedit-face)
            (if (= cursor (length text))  ; right decoration
                (mozc-preedit-put-cursor-at decor-right 0)
              decor-right))))

(defun mozc-preedit-make-segmented-text (preedit
                                         &optional decor-left decor-right
                                         separator)
  "Compose a preedit text and set its text properties.
Return the composed preedit text.

PREEDIT must be the preedit field in a response protobuf.
DECOR-LEFT and DECOR-RIGHT are added at both ends of the text and
Non-nil SEPARATOR is inserted between each segment."
  (let ((segmented-text
         (mapconcat
          (lambda (segment)
            (apply #'propertize (mozc-protobuf-get segment 'value)
                   (case (mozc-protobuf-get segment 'annotation)
                     (highlight
                      '(face mozc-preedit-selected-face))
                     (t
                      '(face mozc-preedit-face)))))
          (mozc-protobuf-get preedit 'segment)
          separator)))
    (concat decor-left segmented-text
            (mozc-preedit-put-cursor-at decor-right 0))))

(defun mozc-preedit-put-cursor-at (text cursor-pos)
  "Put the cursor on TEXT's CURSOR-POSth character (0-origin).
Return the modified text.  If CURSOR-POS is over the TEXT length, do nothing
and return the same text as is."
  (if (and (<= 0 cursor-pos) (< cursor-pos (length text)))
      (let ((text (copy-sequence text)))  ; Do not modify the original string.
        (put-text-property cursor-pos (1+ cursor-pos)
                           'cursor (length text) text)
        text)
    text))

(defvar mozc-preedit-style nil
  "Visual style of preedit.
This variable must be a list of enabled styles or nil.
Supported styles are:
fence -- put vertical bars at both ends of preedit")

(defface mozc-preedit-selected-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "white" :background "brown"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "white" :background "brown"))
    (t
     (:inverse-video t)))
  "Face for the selected segment of preedit."
  :group 'mozc-faces)

(defface mozc-preedit-face
  '((((type graphic x w32) (class color) (background dark))
     (:underline t))
    (((type graphic x w32) (class color) (background light))
     (:underline t))
    (t
     (:underline t)))
  "Face for non-selected segments of preedit."
  :group 'mozc-faces)



;;;; Candidate window

(defcustom mozc-candidate-style 'overlay
  "The selected type of candidate windows.
Symbol `overlay' and `echo-area' are currently supported.

overlay   - Shows a candidate window in box style close to the point.
echo-area - Shows a candidate list in the echo area."
  :type '(choice (symbol :tag "overlaid box style" 'overlay)
                 (symbol :tag "single line in echo area" 'echo-area))
  :group 'mozc)

(defvar mozc-candidate-dispatch-table
  '((overlay (clean-up . mozc-cand-overlay-clean-up)
             (clear . mozc-cand-overlay-clear)
             (update . mozc-cand-overlay-update))
    (echo-area (clean-up . mozc-cand-echo-area-clean-up)
               (clear . mozc-cand-echo-area-clear)
               (update . mozc-cand-echo-area-update)))
  "Method dispatch table to support a variety of candidate windows.
The table is an alist from types of candidate windows to alist of methods.
Each type of candidate windows must support 3 methods; clean-up, clear and
update.")

(defsubst mozc-candidate-dispatch (method &rest args)
  "Dispatch a method call according to `mozc-candidate-style'.
METHOD is one of symbols `clean-up', `clear' and `update'.  For ARGS, see
`mozc-candidate-clean-up', `mozc-candidate-clear' and `mozc-candidate-update'
respectively."
  (let* ((style (if (minibufferp)
                    'echo-area
                  mozc-candidate-style))
         (method-table (cdr (assq style
                                  mozc-candidate-dispatch-table)))
         (func (cdr (assq method method-table))))
    (if func
        (apply func args)
      (signal 'mozc-internal-error (list mozc-candidate-style method args)))))

(defsubst mozc-candidate-clean-up ()
  "Clean up the current candidate session."
  (mozc-candidate-dispatch 'clean-up))

(defsubst mozc-candidate-clear ()
  "Clear the current candidate window.
This function expects an update just after this call.
If you want to finish a candidate session, call `mozc-candidate-clean-up'."
  (mozc-candidate-dispatch 'clear))

(defsubst mozc-candidate-update (candidates)
  "Update the candidate window.
CANDIDATES must be the candidates field in a response protobuf."
  (mozc-candidate-dispatch 'update candidates))



;;;; Candidate window (echo area version)

(defun mozc-cand-echo-area-clean-up ()
  "Clean up the current candidate session."
  (mozc-cand-echo-area-clear))

(defun mozc-cand-echo-area-clear ()
  "Clear the candidate list."
  (mozc-buffer-placeholder-delete mozc-cand-echo-area-placeholder-region))

(defun mozc-cand-echo-area-update (candidates)
  "Update the candidate list in the echo area.
CANDIDATES must be the candidates field in a response protobuf."
  (let ((contents (mozc-cand-echo-area-make-contents candidates)))
    (cond
     ((not (minibufferp))
      (let (message-log-max)
        (message "%s" contents)))
     ((null resize-mini-windows)
      ;; Do not show a candidate list because the space in the minibuffer is
      ;; very limited.  Show only the preedit.
      )
     (t
      (mozc-buffer-placeholder-delete mozc-cand-echo-area-placeholder-region)
      (save-excursion
        (goto-char (point-max))
        (mozc-buffer-placeholder-setq mozc-cand-echo-area-placeholder-region
                                      ?\n contents))))))

(defun mozc-cand-echo-area-make-contents (candidates)
  "Make a list of candidates as an echo area message.
CANDIDATES must be the candidates field in a response protobuf.
Return a string formatted to suit for the echo area."
  (let ((focused-index (mozc-protobuf-get candidates 'focused-index))
        (size (mozc-protobuf-get candidates 'size))
        (index-visible (mozc-protobuf-get candidates 'footer 'index-visible)))
    (apply
     #'concat
     ;; Show "focused-index/#total-candidates".
     (when (and index-visible focused-index size)
       (propertize
        (format "%d/%d" (1+ focused-index) size)
        'face 'mozc-cand-echo-area-stats-face))
     ;; Show each candidate.
     (mapcar
      (lambda (candidate)
        (let ((index (mozc-protobuf-get candidate 'index))
              (value (mozc-protobuf-get candidate 'value))
              (desc (mozc-protobuf-get candidate 'annotation 'description))
              (shortcut (mozc-protobuf-get candidate 'annotation 'shortcut)))
          (concat
           " "
           (propertize (if shortcut  ; shortcut
                           (format "%s." shortcut)
                         (format "%d." (1+ index)))
                       'face 'mozc-cand-echo-area-shortcut-face)
           " "
           (propertize value  ; candidate
                       'face (if (eq index focused-index)
                                 'mozc-cand-echo-area-focused-face
                               'mozc-cand-echo-area-candidate-face))
           (when desc " ")
           (when desc
             (propertize (format "(%s)" desc)
                         'face 'mozc-cand-echo-area-annotation-face)))))
      (mozc-protobuf-get candidates 'candidate)))))

(defvar mozc-cand-echo-area-placeholder-region nil
  "A region which is temporarily added for showing a candidate list.")
(make-variable-buffer-local 'mozc-cand-echo-area-placeholder-region)

(defface mozc-cand-echo-area-focused-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "black" :background "orange"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "black" :background "orange"))
    (t
     (:inverse-video t)))
  "Face for the focused candidate in the echo area candidate window."
  :group 'mozc-faces)

(defface mozc-cand-echo-area-candidate-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "yellow"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "blue"))
    (t
     (:underline t)))
  "Face for candidates in the echo area candidate window."
  :group 'mozc-faces)

(defface mozc-cand-echo-area-shortcut-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "grey"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "black")))
  "Face for shortcut keys in the echo area candidate window."
  :group 'mozc-faces)

(defface mozc-cand-echo-area-annotation-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "grey"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "black")))
  "Face for annotations in the echo area candidate window."
  :group 'mozc-faces)

(defface mozc-cand-echo-area-stats-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "orange"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "black" :background "light green"))
    (t
     (:inverse-video t)))
  "Face for the index of the focused candidate and the total number of
candidates in the echo area candidate window."
  :group 'mozc-faces)



;;;; Candidate window (overlay version)

(defun mozc-cand-overlay-clean-up ()
  "Clean up the current candidate session.
Remove all overlays and placeholder characters."
  (mozc-cand-overlay-clear))

(defun mozc-cand-overlay-clear ()
  "Clear the candidate window.
Remove all overlays and placeholder characters."
  (mozc-cand-overlay-delete-overlays)
  (mozc-with-buffer-modified-p-unchanged
   (mozc-buffer-placeholder-delete-all mozc-cand-overlay-placeholder-regions)))

(defun mozc-cand-overlay-update (candidates)
  "Update the candidate window using overlays.
CANDIDATES must be the candidates field in a response protobuf.

If there is no enough space to show the candidate window,
falls back to the echo area version."
  (mozc-cand-overlay-clear)
  (let ((contents (mozc-cand-overlay-make-contents candidates)))
    (condition-case nil
        (mozc-with-buffer-modified-p-unchanged
         (mozc-cand-overlay-draw contents))
      (error
       (mozc-cand-overlay-clear)
       ;; Fall back to the echo area version.
       (mozc-cand-echo-area-update candidates)))))

(defun mozc-cand-overlay-make-contents (candidates)
  "Return text contents for a candidate window.
This function returns a list of pairs of text (left- and right-aligned)
and face for each row.
CANDIDATES must be the candidates field in a response protobuf."
  (let ((focused-index (mozc-protobuf-get candidates 'focused-index))
        (size (mozc-protobuf-get candidates 'size))
        (index-visible (mozc-protobuf-get candidates 'footer 'index-visible)))
    (nconc
     (mapcar
      (lambda (candidate)
        (let ((index (mozc-protobuf-get candidate 'index))
              (value (mozc-protobuf-get candidate 'value))
              (desc (mozc-protobuf-get candidate 'annotation 'description))
              (shortcut (mozc-protobuf-get candidate 'annotation 'shortcut)))
          (list
           (concat (when shortcut  ; left-aligned text
                     (format "%s. " shortcut))
                   value)
           desc  ; right-aligned text
           (cond ((eq index focused-index)  ; face
                  'mozc-cand-overlay-focused-face)
                 ((and (integerp index) (= (logand index 1) 0))
                  'mozc-cand-overlay-odd-face)
                 ((and (integerp index) (= (logand index 1) 1))
                  'mozc-cand-overlay-even-face)))))
      (mozc-protobuf-get candidates 'candidate))
     ;; Footer line in the form of "focused-index/#total-candidates"
     (and index-visible focused-index size
          `((nil ,(format "%d/%d" (1+ focused-index) size)
                 mozc-cand-overlay-footer-face))))))

(defun mozc-cand-overlay-draw (contents)
  "Find the right place and draw a candidate window there.
If there is no space to show a candidate window, signal the symbol
`mozc-draw-error'.

CONTENTS is text contents for a candidate window returned by
`mozc-cand-overlay-make-contents'.

The function may scroll up the window to make enough space."
  (save-excursion
    (goto-char mozc-preedit-point-origin)
    (let* ((contents-lines (length contents))
           (contents-width (mozc-cand-overlay-estimate-max-width contents))
           (window-width (mozc-window-width))
           (posn1 (mozc-posn-at-point))
           (row0 (mozc-posn-row mozc-preedit-posn-origin))
           (row1 (mozc-posn-row posn1))
           (x0 (mozc-posn-x mozc-preedit-posn-origin))
           (x (if (< (+ x0 contents-width) window-width)
                  x0
                (- window-width contents-width 1))))
      (if (or (>= contents-width window-width)
              truncate-lines)
          ;; There is no enough space to show a candidate window or
          ;; truncate-lines is enabled, which is not supported.
          (signal 'mozc-draw-error "no space to show a candidate window")
        (or
         ;; Show below.
         (mozc-cand-overlay-draw-internal contents x contents-width 1)
         ;; Show above.
         (and (<= contents-lines row0)
              (mozc-cand-overlay-draw-internal contents x contents-width
                                               (- 0 contents-lines
                                                  (- row1 row0))))
         ;; Scroll up and show below.
         (mozc-cand-overlay-draw-internal contents x contents-width 1 row0)
         ;; All trials have failed.
         (signal 'mozc-draw-error "failed to show a candidate window"))))))

(defun mozc-cand-overlay-estimate-max-width (contents &optional space-width)
  "Return how many pixels in width are needed to show candidates.
CONTENTS is text contents for a candidate window returned by
`mozc-cand-overlay-make-contents'.
Optional SPACE-WIDTH is width of padding space between text, and
defaults to `frame-char-width'."
  (save-excursion
    (let* ((placeholder (mozc-buffer-insert-char))
           (overlay (make-overlay (car placeholder) (cdr placeholder))))
      (unwind-protect
          (progn
            (mozc-cand-overlay-put-space overlay 0)
            (let ((posn-origin (mozc-posn-at-point))
                  (space-width (or space-width (frame-char-width))))
              (apply #'max
                     (mapcar (lambda (content)
                               (mozc-cand-overlay-estimate-width
                                (car content) (cadr content) space-width
                                (caddr content)
                                overlay posn-origin))
                             contents))))
        (delete-overlay overlay)
        (mozc-buffer-delete-region placeholder)))))

(defun mozc-cand-overlay-estimate-width
  (left-text right-text space-width face overlay posn-origin)
  "Return how many pixels in width are needed to show a candidate.
LEFT-TEXT and RIGHT-TEXT are left- and right-aligned text in a row
respectively.  SPACE-WIDTH is width of padding space between text.
Text is shown in FACE.

OVERLAY is used as work space and the current point must be placed
just after the overlay.  POSN-ORIGIN must be the position info
just before the overlay.

This function is called from `mozc-cand-overlay-estimate-max-width'."
  (if (not (or left-text right-text))
      0  ;; No text
    (mozc-cand-overlay-put-text overlay (concat left-text right-text) face)
    (let ((posn (mozc-posn-at-point)))
      (let ((row0 (mozc-posn-row posn-origin))
            (row1 (mozc-posn-row posn))
            (x0 (mozc-posn-x posn-origin))
            (x1 (mozc-posn-x posn)))
        (+ (* (- row1 row0) (mozc-window-width))
           (- x1 x0)
           (if (and left-text right-text) space-width 0))))))

(defun mozc-cand-overlay-draw-internal (contents x width relative-start-row
                                                 &optional max-scroll-lines)
  "Draw a candidate window using overlays.

CONTENTS is text contents for a candidate window returned by
`mozc-cand-overlay-make-contents'.  X and WIDTH are the x position at
the left and the width of the candidate window.
RELATIVE-START-ROW is the top row of the candidate window relative to
the point.  Non-nil MAX-SCROLL-LINES scrolls up that number of lines
at most if short of space to show the candidate window.

The function returns non-nil on success, and nil on failure."
  (let ((window-start-pos (and max-scroll-lines (window-start)))
        (scrolled-lines 0))  ; the number of scrolled lines
    (condition-case nil
        (save-excursion
          (when (>= relative-start-row 0)  ; Make sure there are enough lines.
            (mozc-cand-overlay-insert-placeholder-newlines (length contents)))
          (vertical-motion relative-start-row)
          (while contents
            (while (and max-scroll-lines  ; Scroll up if necessary.
                        (< scrolled-lines max-scroll-lines)
                        (not (pos-visible-in-window-p)))
              (save-excursion
                ;; Put the point in the visible area because `scroll-up' doesn't
                ;; work as expected if the point is off the screen.
                (vertical-motion -1)
                (scroll-up 1))
              (incf scrolled-lines))
            (let ((content (car contents)))
              (let ((left-text (car content))
                    (right-text (cadr content))
                    (face (caddr content)))
                (mozc-cand-overlay-draw-row left-text right-text face x width)))
            (pop contents)
            (vertical-motion 1))
          t)  ; Return t on success.
      (mozc-draw-error
       (mozc-cand-overlay-clear)
       (when window-start-pos
         (set-window-start nil window-start-pos))
       nil))))  ; Return nil on failure.

(defun mozc-cand-overlay-insert-placeholder-newlines (contents-lines)
  "Insert newlines temporarily if necessary.
CONTENTS-LINES is the number of lines needed below the point."
  (save-excursion
    (let ((lines-short (- contents-lines (vertical-motion contents-lines))))
      (when (> lines-short 0)
        (goto-char (point-max))
        (mozc-buffer-placeholder-push-char mozc-cand-overlay-placeholder-regions
                                           ?\n lines-short)))))

(defun mozc-cand-overlay-draw-row (left-text right-text face x width)
  "Draw a row of a candidate window.
LEFT-TEXT and RIGHT-TEXT are left- and right-aligned text in a row
respectively.  FACE is face for LEFT-TEXT, RIGHT-TEXT and padding
space.  X is the x-position of the left-edge of the candidate window
in pixel.  WIDTH is the width of the candidate window.

This function may change the point."
  ;; This function uses at most 6 overlays to draw a row, left- and
  ;; right-margin, left- and right-text and the padding space between
  ;; the texts, and optionally another overlay to break a wrapped line
  ;; if any.
  ;;
  ;; Left-margin is used to align the left edge of the candidate window.
  ;; Left- and right-text overlays are used to show each text, and
  ;; the padding overlay is used to align right-text to the right edge of
  ;; the candidate window.  Right-margin is used to keep the position of
  ;; the text at the right of the candidate window unchanged.
  ;;
  ;; If X is close to the beginning of a wrapped row, the optional overlay
  ;; is used to break the wrapped row, otherwise the wrapped position may
  ;; change because a spacing overlay is treated as zero-width regardless
  ;; of the width set to the overlay.
  (let* ((x1 x)
         (x2 (+ x width))
         (y (or (mozc-posn-y (mozc-posn-at-point))
                ;; If the current row is out of the visible area, signals
                ;; an error.
                (signal 'mozc-draw-error (list left-text right-text))))
         (posn1 (posn-at-x-y x1 y))
         (posn2 (posn-at-x-y x2 y))
         (p1 (posn-point posn1))
         (p2 (posn-point posn2))
         (p1x (mozc-posn-x (mozc-posn-at-point p1)))
         (p2x (mozc-posn-x (mozc-posn-at-point p2)))
         ;; Unless p2x is at the right position, move p2 to right by 1 column
         ;; and update p2x accordingly.
         (just-in-pos (or (= p2 (line-end-position)) (= p2x x2)))
         (p2 (if just-in-pos p2
               (min (line-end-position) (1+ p2))))
         (p2x (if just-in-pos p2x
                (mozc-posn-x (mozc-posn-at-point p2))))
         ;; If short of characters to replace with overlays, insert characters
         ;; and update p2.
         (cols (- p2 p1))
         (min-cols 6)  ; 6 characters are needed for 6 overlays at most.
         (p2 (if (>= cols min-cols)  ; Revise p2 if short of columns.
                 p2
               (goto-char p2)  ; Insert temporary characters.
               (mozc-buffer-placeholder-push-char
                mozc-cand-overlay-placeholder-regions nil (- min-cols cols))
               (+ p1 min-cols))))
    ;; left margin
    (if (or (/= (mozc-posn-col posn1) 0)
            (progn (goto-char p1) (bolp)))
        (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 0) (+ p1 2))))
          (mozc-cand-overlay-put-space overlay (- x1 p1x)))
      ;; If p1 is the beginning of a wrapped row, replacing the char at p1 with
      ;; a spacing overlay may change the wrapped position.  Since we wouldn't
      ;; like to change the wrapped position, break a line explicitly.
      (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 0) (+ p1 1))))
        (mozc-cand-overlay-put-text overlay "\n"))
      (goto-char (1+ p1))  ; Put the point after the newline.
      (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 1) (+ p1 2))))
        (mozc-cand-overlay-put-space overlay (- x1 p1x))))
    ;; left text
    (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 2) (+ p1 3))))
      (if left-text
          (mozc-cand-overlay-put-text overlay left-text face)
        (mozc-cand-overlay-put-space overlay 0 face)))
    ;; right text
    (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 4) (+ p1 5))))
      (if right-text
          (mozc-cand-overlay-put-text overlay right-text face)
        (mozc-cand-overlay-put-space overlay 0 face)))
    ;; padding between left- and right-text
    (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 3) (+ p1 4))))
      (mozc-cand-overlay-put-space overlay 0 face)
      (let ((width (- x2 (mozc-posn-x (mozc-posn-at-point (+ 4 p1))))))
        (mozc-cand-overlay-put-space overlay width face)))
    ;; right margin
    (let ((overlay (mozc-cand-overlay-make-overlay (+ p1 5) p2)))
      (mozc-cand-overlay-put-space overlay (- p2x x2)))))

(defvar mozc-cand-overlay-overlays nil
  "Overlays which are put for showing candidate windows.")
(make-variable-buffer-local 'mozc-cand-overlay-overlays)

(defsubst mozc-cand-overlay-make-overlay (beg end)
  "Create a new overlay with the range from BEG to END.
`mozc-cand-overlay-delete-overlays' removes all overlays created by
this function."
  (let ((overlay (make-overlay beg end)))
    (push overlay mozc-cand-overlay-overlays)
    overlay))

(defsubst mozc-cand-overlay-put-text (overlay text &optional face)
  "Change the display property of OVERLAY to TEXT.
Optionally change the face property of OVERLAY to FACE."
  (overlay-put overlay 'display text)
  (when face
    (overlay-put overlay 'face face)))

(defsubst mozc-cand-overlay-put-space (overlay width &optional face)
  "Change the display property of OVERLAY to space of WIDTH pixel in width.
Optionally change the face property of OVERLAY to FACE."
  (overlay-put overlay 'display `(space . (:width (,(max 0 width)))))
  (when face
    (overlay-put overlay 'face face)))

(defun mozc-cand-overlay-delete-overlays ()
  "Remove overlays used for showing candidate windows."
  (mapc #'delete-overlay mozc-cand-overlay-overlays)
  (setq mozc-cand-overlay-overlays nil))

(defvar mozc-cand-overlay-placeholder-regions nil
  "Regions which are temporarily added for showing candidate windows.")
(make-variable-buffer-local 'mozc-cand-overlay-placeholder-regions)

(defface mozc-cand-overlay-focused-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "#191970" :background "#ffa500"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "#0f0f0f" :background "#ffa500"))
    (t
     (:inverse-video t)))
  "Face for the focused candidate in the overlay candidate window."
  :group 'mozc-faces)

(defface mozc-cand-overlay-odd-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "#fffacd" :background "#27408b"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "#0f0f0f" :background "#bfefff"))
    (t
     (:underline t)))
  "Face for candidates on odd rows in the overlay candidate window."
  :group 'mozc-faces)

(defface mozc-cand-overlay-even-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "#fffacd" :background "#27408b"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "#0f0f0f" :background "#bfefff"))
    (t
     (:underline t)))
  "Face for candidates on even rows in the overlay candidate window."
  :group 'mozc-faces)

(defface mozc-cand-overlay-footer-face
  '((((type graphic x w32) (class color) (background dark))
     (:foreground "#fffacd" :background "#4169e1"))
    (((type graphic x w32) (class color) (background light))
     (:foreground "#0f0f0f" :background "#b0e2ff"))
    (t
     (:inverse-video t)))
  "Face for the footer line of the overlay candidate window."
  :group 'mozc-faces)



;;;; Utilities for position and window coordinates

(defvar mozc-cached-header-line-height nil
  "Cached height of the header line.")

(defun mozc-posn-at-point (&optional pos window)
  "Return the same position information as `posn-at-point'.
The arguments POS and WINDOW are the same ones to `posn-at-point'.

The difference is that, while `posn-at-point' returns position information
at the previous point when it's on a terminal and the point is at the beginning
of a wrapped line, this function returns the position information exactly
at the point.

For example, suppose the following line in the buffer and the point is at 'd'
\(the beginning of character 'd'),
    ....... abc[wrap]
    def...
\(cdr (posn-actual-col-row (posn-at-point AT_D))) is the same number at 'c' on
a terminal.

In a word, this function is a fixed version of `posn-at-point'."
  (let ((posn (posn-at-point pos window)))
    (if window-system
        posn
      (let* ((pos (cond ((numberp pos) pos)
                        ((markerp pos) (marker-position pos))
                        (t (point))))
             (posn-next (posn-at-point (1+ pos) window))
             (row (cdr (posn-actual-col-row posn)))
             (row-next (cdr (posn-actual-col-row posn-next))))
        (if (or (= pos (line-end-position))
                (listp row)
                (listp row-next)
                (= row row-next)
                (eq (car (posn-actual-col-row posn-next)) 0))
            posn
          ;; On a terminal, row and y-position at the beginning of
          ;; a continued line are the ones at the previous position.
          ;; Use the ones at the next position.
          (setcar (nthcdr 5 posn) pos)  ; point
          (setcar (nth 6 posn) 0)  ; col
          (setcdr (nth 6 posn) (cdr (nth 6 posn-next)))  ; row
          (setcar (nth 2 posn) 0)  ; x
          (setcdr (nth 2 posn) (cdr (nth 2 posn-next)))  ; y
          posn)))))

(defsubst mozc-posn-col (position)
  "Return the column in POSITION."
  (car (posn-actual-col-row position)))

(defsubst mozc-posn-row (position)
  "Return the row in POSITION."
  (cdr (posn-actual-col-row position)))

(defsubst mozc-posn-x (position)
  "Return the x coordinate in POSITION."
  (car (posn-x-y position)))

(defsubst mozc-posn-y (position)
  "Return the y coordinate in POSITION.

This function returns y offset from the top of the buffer area including
the header line.  This definition could be changed in future.

Note: On Emacs 22 and 23, y offset, returned by `posn-at-point' and taken
by `posn-at-x-y', is relative to the top of the buffer area including
the header line.
However, on Emacs 24, y offset returned by `posn-at-point' is relative to
the text area excluding the header line, while y offset taken by
`posn-at-x-y' is relative to the buffer area including the header line.
This asymmetry is by design according to GNU Emacs team.

This function fixes the asymmetry between them on Emacs 24 and later versions.
This hack could be moved to mozc-posn-at-x-y in a future version."
  (let ((y (cdr (posn-x-y position))))
    (if (or (null header-line-format) (<= emacs-major-version 23))
        y
      (+ y (or mozc-cached-header-line-height
               (setq mozc-cached-header-line-height (mozc-header-line-height))
               0)))))

(defsubst mozc-window-width (&optional window)
  "Return the width of WINDOW in pixel.
WINDOW defaults to the selected window."
  (let ((rect (window-inside-pixel-edges window)))
    (- (third rect) (first rect))))

(defun mozc-header-line-height ()
  "Return the height of the header line.
If there is no header line, return nil."
  (let ((posn (posn-at-x-y 0 0)))
    (if (eq (posn-area posn) 'header-line)
        (cdr (posn-object-width-height posn)))))

(defun mozc-clear-cached-header-line-height ()
  "Clear the cached height of the header line."
  (setq mozc-cached-header-line-height nil))



;;;; Buffer local session management

(defvar mozc-session-process nil
  "Buffer local process object to compare with `mozc-helper-process'.
If both process objects are identical, it means the helper process is running
as expected.  If not, it means the helper process has restarted or crashed.
In this case, need to re-create a Mozc session.")
(make-variable-buffer-local 'mozc-session-process)

(defvar mozc-session-id nil
  "Buffer local Mozc session ID.
Each buffer has a different session so that a user can have different
conversion status in each buffer.")
(make-variable-buffer-local 'mozc-session-id)

(defvar mozc-session-seq 0
  "Next ID(sequence number) of messages sent to the helper process.
Using this ID, the program recognizes which response corresponds to
a certain request avoiding cross talk.

This sequence number is called 'event-id' in the helper process,
which doesn't have to be a *sequence* number.")

(defun mozc-session-create (&optional forcep)
  "Create a Mozc session if necessary and return it.

If FORCEP is non-nil, delete a current session (if it exists) and
create a new session."
  (mozc-session-connect-to-helper-process)

  ;; If forcep, re-create a session.
  (when (and mozc-session-id forcep)
    (mozc-session-delete))

  (unless mozc-session-id
    (mozc-session-execute-command 'CreateSession))
  mozc-session-id)

(defun mozc-session-connect-to-helper-process ()
  "Make it available to communicate with the helper process."
  (let ((process (mozc-helper-process-start)))
    ;; If the helper process is not available, invalidate the current session.
    (unless (eq process mozc-session-process)
      (setq mozc-session-id nil
            mozc-session-process process))))

(defun mozc-session-delete ()
  "Delete a current Mozc session."
  (when (and mozc-session-id mozc-session-process
             (eq mozc-session-process mozc-helper-process))
    ;; Just try to delete the session.  Ignore any error.
    (mozc-session-execute-command 'DeleteSession))
  (setq mozc-session-id nil)
  (setq mozc-session-process nil))

(defsubst mozc-session-sendkey (key-list)
  "Send a key event to the helper process and return the resulting protobuf.
The resulting protocol buffer, which is represented as alist, is
mozc::commands::Output in C++.  Return nil on error.

KEY-LIST is a list of a key code (97 = ?a), key symbols ('space, 'shift,
'meta and so on), and/or a string which represents the preedit to be
inserted (\"\\u3061\")."
  (when (mozc-session-create)
    (apply #'mozc-session-execute-command 'SendKey key-list)))

(defun mozc-session-execute-command (command &rest args)
  "Send a COMMAND and receive a corresponding response.
And then return mozc::commands::Output protocol buffer as alist.
If error occurred, return nil.

ARGS must suit to a COMMAND.  See the document of the helper process."
  (let ((seq mozc-session-seq))
    ;; Increment the seq first so that it produces another seq
    ;; when an error occurred.
    (mozc-session-seq-inc)
    ;; Send a command in the form of:
    ;;   EVENT-ID COMMAND [SESSION-ID] [ARGS]...
    (apply #'mozc-helper-process-send-sexpr
           seq command
           ;; Only CreateSession command doesn't need a session ID.
           (if (eq command 'CreateSession)
               args
             (cons mozc-session-id args)))
    ;; Check whether the session ID matches or not.
    (let* ((resp (mozc-session-recv-corresponding-response seq))
           (session-id (cdr (assq 'emacs-session-id resp)))
           (output (mozc-protobuf-get resp 'output)))
      ;; mozc-session-id should be nil when not yet connected.
      ;; session-id is nil when an error occurred.
      (cond
       ((eq command 'CreateSession)
        (if (setq mozc-session-id session-id)
            output
          (mozc-abort)
          (message "mozc.el: Failed to start a new session.")
          (signal 'mozc-session-error resp)))
       ((eq session-id mozc-session-id)
        output)
       ;; Otherwise, return nil.
       ))))

(defun mozc-session-recv-corresponding-response (seq)
  "Receive the response whose event ID is SEQ, and return it."
  (let* ((resp (mozc-helper-process-recv-sexpr))
         (event-id (and (listp resp) (cdr (assq 'emacs-event-id resp)))))
    ;; Check whether the event ID matches or not.
    (cond
     ((eq event-id seq)  ; expected response
      resp)
     (event-id  ; The event has other event ID.
      (mozc-session-recv-corresponding-response seq))
     (t  ; Error occurred.
      nil))))

(defun mozc-session-seq-inc ()
  "Increment `mozc-session-seq'.  If overflow, set it to 0.
Return the new value of `mozc-session-seq'."
  (setq mozc-session-seq
        (if (and (<= 0 mozc-session-seq)
                 (< mozc-session-seq 134217727))  ;; 28-bit signed integer.
            (1+ mozc-session-seq)
          0)))



;;;; Server side configuration

(defvar mozc-config-protobuf nil
  "Mozc server side configuration in the form of mozc::config::Config.")



;;;; Communication with Mozc server through the helper process

(defvar mozc-helper-program-name "mozc_emacs_helper"
  "Helper program's name or path to the helper program.
The helper program helps Emacs communicate with Mozc server,
which doesn't understand S-expression.")

(defvar mozc-helper-program-args '("--suppress_stderr")
  "A list of arguments passed to the helper program.")

(defvar mozc-helper-process-timeout-sec 1
  "Time-out in second to wait a response from Mozc server.")

(defvar mozc-helper-process nil
  "The process object currently running.")

(defvar mozc-helper-process-version nil
  "A version string of the helper process.")

(defvar mozc-helper-process-message-queue nil
  "A list of messages sent by the helper process.")

(defvar mozc-helper-process-string-buf nil
  "Raw string sent from the helper process.
This string may be a part of a complete message, or an empty string.
This is the temporary buffer to receive a complete message.
A single message may be sent in several bunches.")

(defun mozc-helper-process-start (&optional forcep)
  "Start the helper process if not running or not under the control.
If FORCEP is non-nil, stop the process (if running) and (re)start
the new process.
Return the process object on success.  Otherwise return nil."
  ;; If forcep, restart a process.
  (when (and mozc-helper-process forcep)
    (mozc-helper-process-stop))

  (unless mozc-helper-process
    (message "mozc.el: Starting mozc-helper-process...")
    (condition-case nil
        (let* ((process-connection-type nil)  ; We don't need pty. Use pipe.
               (proc (apply #'start-process "mozc-helper-process" nil
                            mozc-helper-program-name
                            mozc-helper-program-args)))
          ;; Set up the helper process.
          (set-process-query-on-exit-flag proc nil)
          (set-process-sentinel proc 'mozc-helper-process-sentinel)
          (set-process-coding-system proc 'utf-8-unix 'utf-8-unix)
          (set-process-filter proc 'mozc-helper-process-filter)
          ;; Reset the raw buffer and message queue.
          (setq mozc-helper-process-string-buf nil)
          (setq mozc-helper-process-message-queue nil)
          ;; Receive the greeting message.
          (if (mozc-helper-process-recv-greeting proc)
              (progn  ; Everything looks good.
                (setq mozc-helper-process proc)
                (message "mozc.el: Starting mozc-helper-process...done"))
            (message "mozc.el: Wrong start-up message from the helper process")
            (signal 'mozc-helper-process-error nil)))
      (error  ; Abort unless the helper process successfully runs.
       (mozc-abort)
       (message "mozc.el: Failed to start mozc-helper-process.")
       (signal 'mozc-helper-process-error nil))))
  mozc-helper-process)

(defun mozc-helper-process-recv-greeting (proc)
  "Try to receive the greeting message from PROC.
Return non-nil on success.

The expected greeting message is alist which includes the following keys
at least:
mozc-emacs-helper -- must be t
version           -- should be version string"
  ;; Set mozc-helper-process temporarily and try to receive
  ;; the greeting message of the helper process.
  (let* ((mozc-helper-process proc)
         (resp (mozc-helper-process-recv-sexpr)))
    (when (and (listp resp)
               ;; The value of mozc-emacs-helper must be t.
               (cdr (assq 'mozc-emacs-helper resp)))
      ;; Set the optional version string.
      (setq mozc-helper-process-version (cdr (assq 'version resp)))
      ;; Set the optional server side configuration.
      (setq mozc-config-protobuf (cdr (assq 'config resp)))
      t)))

(defun mozc-helper-process-stop ()
  "Stop the helper process if running and under the control."
  (when mozc-helper-process
    (message "mozc.el: Stopping mozc-helper-process...")
    (quit-process mozc-helper-process)
    (message "mozc.el: Stopping mozc-helper-process...done")
    (setq mozc-helper-process nil)))

(defun mozc-helper-process-sentinel (proc message)
  "Invalidate `mozc-helper-process' if PROC is not running normally.
Current implementation throws MESSAGE away."
  (when (eq proc mozc-helper-process)
    (case (process-status proc)
      (run)  ; Do nothing.
      (t  ; Invalidate mozc-helper-process.
       (setq mozc-helper-process nil)))))

(defun mozc-helper-process-filter (proc output-string)
  "Receive output from the helper process and store it in the queue.

This function must be set by `set-process-filter'.
PROC is a process which output OUTPUT-STRING.
OUTPUT-STRING is the entire string of output from PROC.

PROC should be equal to `mozc-helper-process-filter', otherwise
OUTPUT-STRING will be ignored.
This function accumulates output-string and splits it into messages.
Each message must end with newline.  Messages are stored in the queue
`mozc-helper-process-message-queue'."
  ;; If proc is no longer active, just throw away the data.
  (when (eq proc mozc-helper-process)
    ;; Define a complete message as a single line which must end with ?\n.
    (let* ((raw-string (concat mozc-helper-process-string-buf output-string))
           (pair (mozc-split-at-last (split-string raw-string "\n")))
           (complete-messages (car pair))
           (incomplete-string (cadr pair)))
      ;; Excluding the last one, append messages to the queue.
      (setq mozc-helper-process-message-queue
            (nconc mozc-helper-process-message-queue complete-messages))
      ;; The last part, which doesn't end with ?\n, is not a complete message.
      ;; Keep it in the raw buffer.  The last part can be "" (empty string).
      (setq mozc-helper-process-string-buf incomplete-string))))

(defsubst mozc-helper-process-send-sexpr (&rest args)
  "Send S-expressions ARGS to the helper process.
A message sent to the process is a list of ARGS and formatted in
a single line."
  ;; Newline is necessary to flush the output.
  (process-send-string mozc-helper-process (format "%S\n" args)))

(defun mozc-helper-process-recv-sexpr ()
  "Return a response from the helper process.
A returned object is alist on success.  Otherwise, an error symbol."
  (let ((response (mozc-helper-process-recv-response)))
    (if (not response)
        (progn  ; No data has been received.
          (message "mozc.el: No response from the server")
          'no-data-available)
      (condition-case nil
          (let ((obj-index
                 (read-from-string response)))  ; may signal end-of-file.
            (if (mozc-string-match-p "^[ \t\n\v\f\r]*$"
                                     (substring response (cdr obj-index)))
                ;; Only white spaces remain.
                (car obj-index)
              ;; Unexpected characters remain at the end.
              (message "mozc.el: Unexpected response from the server")
              (mozc-helper-process-stop)
              'wrong-format))
        ;; S-expr doesn't end or unexpected newline in a single S-expr.
        (end-of-file
         (message "mozc.el: Unexpected newline in a single S-expr")
         (mozc-helper-process-stop)
         'wrong-format)))))

(defun mozc-helper-process-recv-response ()
  "Return a single complete message from the helper process.
If timed out, return nil."
  (if mozc-helper-process-message-queue
      (pop mozc-helper-process-message-queue)
    (if (accept-process-output mozc-helper-process
                               mozc-helper-process-timeout-sec)
        (mozc-helper-process-recv-response)
      nil)))



;;;; Utilities

(defun mozc-protobuf-get (alist key &rest keys)
  "Look for a sequence of keys in ALIST recursively.
Return a found value, or nil if not found.
KEY and KEYS can be a symbol or integer.

For example, (mozc-protobuf-get protobuf 'key1 2 'key3) is equivalent to
  (cdr (assq 'key3
             (nth 2
                  (cdr (assq 'key1
                             protobuf)))))
except for error handling.  This is similar to
  protobuf.key1(2).key3()
in C++."
  (and (listp alist)
       (let ((value (if (integerp key)
                        (nth key alist)
                      (cdr (assq key alist)))))
         (if keys
             (apply #'mozc-protobuf-get value keys)
           value))))

(defun mozc-split-at-last (list &optional n)
  "Split LIST to last N nodes and the rest.
Return a cons of the beginning part of LIST and a list of last N nodes.
This function alters LIST.

If N is equal to or greater than the length of LIST, return a cons of nil
and LIST.  The default value of N is 1."
  (let* ((sentinel-list (cons t list))
         (pre-boundary (last sentinel-list (1+ (or n 1))))
         (post-boundary (cdr pre-boundary)))
    (if (eq pre-boundary sentinel-list)
        (cons nil list)
      (setcdr pre-boundary nil)  ; Drop the rest of list.
      (cons list post-boundary))))

(defsubst mozc-string-match-p (regexp string &optional start)
  "Same as `string-match' except this function never change the match data.
REGEXP, STRING and optional START are the same as for `string-match'.

This function is equivalent to `string-match-p', which is available since
Emacs 23."
  (let ((inhibit-changing-match-data t))
    (string-match regexp string start)))



;;;; Custom keymap

(defvar mozc-keymap-preedit-method-to-keymap-name-map
  '((roman . nil)
    (kana . mozc-keymap-kana))
  "Mapping from preedit methods (roman or kana) to keymaps.
The preedit method is taken from the server side configuration.")

(defun mozc-keymap-current-active-keymap ()
  "Return the current active keymap."
  (let* ((preedit-method
          (mozc-protobuf-get mozc-config-protobuf 'preedit-method))
         (keymap-name
          (cdr (assq preedit-method
                     mozc-keymap-preedit-method-to-keymap-name-map)))
         (keymap (and keymap-name (boundp keymap-name)
                      (symbol-value keymap-name))))
    (and (hash-table-p keymap) keymap)))

(defun mozc-keymap-make-keymap ()
  "Create a new empty keymap and return it."
  (make-hash-table :size 128 :test #'eq))

(defun mozc-keymap-make-keymap-from-flat-list (list)
  "Create a new keymap and fill it with entries in LIST.
LIST must be a flat list which contains keys and mapped strings alternately."
  (mozc-keymap-fill-entries-from-flat-list (mozc-keymap-make-keymap) list))

(defun mozc-keymap-fill-entries-from-flat-list (keymap list)
  "Fill KEYMAP with entries in LIST and return KEYMAP.
KEYMAP must be a key table from keycodes to mapped strings.
LIST must be a flat list which contains keys and mapped strings alternately."
  (if (not (and (car list) (cadr list)))
      keymap  ; Return the keymap.
    (mozc-keymap-put-entry keymap (car list) (cadr list))
    (mozc-keymap-fill-entries-from-flat-list keymap (cddr list))))

(defun mozc-keymap-get-entry (keymap keycode &optional default)
  "Return a mapped string if the entry for the keycode exists.
Otherwise, the default value, which must be a string.
KEYMAP must be a key table from keycodes to mapped strings.
KEYCODE must be an integer representing a key code to look up.
DEFAULT is returned if it's a string and no entry for KEYCODE is found."
  (let ((value (and (hash-table-p keymap)
                    (gethash keycode keymap default))))
    (and (stringp value) value)))

(defun mozc-keymap-put-entry (keymap keycode mapped-string)
  "Add a new key mapping to a keymap.
KEYMAP must be a key table from keycodes to mapped strings.
KEYCODE must be an integer representing a key code.
MAPPED-STRING must be a string representing a preedit string to be inserted."
  (when (and (hash-table-p keymap)
             (integerp keycode) (stringp mapped-string))
    (puthash keycode mapped-string keymap)
    (cons keycode mapped-string)))

(defun mozc-keymap-remove-entry (keymap keycode)
  "Remove an entry from a keymap.  If no entry for keycode exists, do nothing.
KEYMAP must be a key table from keycodes to mapped strings.
KEYCODE must be an integer representing a key code to remove."
  (when (hash-table-p keymap)
    (remhash keycode keymap)))

(defvar mozc-keymap-kana-106jp
  (mozc-keymap-make-keymap-from-flat-list
   '(;; 1st row
     ;; ?1 "ぬ" ?2 "ふ" ?3 "あ" ?4 "う" ?5 "え"
     ?1 "\u306c" ?2 "\u3075" ?3 "\u3042" ?4 "\u3046" ?5 "\u3048"
     ;; ?6 "お" ?7 "や" ?8 "ゆ" ?9 "よ" ?0 "わ"
     ?6 "\u304a" ?7 "\u3084" ?8 "\u3086" ?9 "\u3088" ?0 "\u308f"
     ;; ?- "ほ" ?^ "へ" ?| "ー"
     ?- "\u307b" ?^ "\u3078" ?| "\u30fc"
     ;; 2nd row
     ;; ?q "た" ?w "て" ?e "い" ?r "す" ?t "か"
     ?q "\u305f" ?w "\u3066" ?e "\u3044" ?r "\u3059" ?t "\u304b"
     ;; ?y "ん" ?u "な" ?i "に" ?o "ら" ?p "せ"
     ?y "\u3093" ?u "\u306a" ?i "\u306b" ?o "\u3089" ?p "\u305b"
     ;; ?@ "゛" ?\[ "゜"
     ?@ "\u309b" ?\[ "\u309c"
     ;; 3rd row
     ;; ?a "ち" ?s "と" ?d "し" ?f "は" ?g "き"
     ?a "\u3061" ?s "\u3068" ?d "\u3057" ?f "\u306f" ?g "\u304d"
     ;; ?h "く" ?j "ま" ?k "の" ?l "り" ?\; "れ"
     ?h "\u304f" ?j "\u307e" ?k "\u306e" ?l "\u308a" ?\; "\u308c"
     ;; ?: "け" ?\] "む"
     ?: "\u3051" ?\] "\u3080"
     ;; 4th row
     ;; ?z "つ" ?x "さ" ?c "そ" ?v "ひ" ?b "こ"
     ?z "\u3064" ?x "\u3055" ?c "\u305d" ?v "\u3072" ?b "\u3053"
     ;; ?n "み" ?m "も" ?, "ね" ?. "る" ?/ "め"
     ?n "\u307f" ?m "\u3082" ?, "\u306d" ?. "\u308b" ?/ "\u3081"
     ;; ?\\ "ろ"
     ?\\ "\u308d"
     ;; shift
     ;; ?# "ぁ" ?E "ぃ" ?$ "ぅ" ?% "ぇ" ?& "ぉ"
     ?# "\u3041" ?E "\u3043" ?$ "\u3045" ?% "\u3047" ?& "\u3049"
     ;; ?' "ゃ" ?\( "ゅ" ?\) "ょ" ?~ "を" ?Z "っ"
     ?' "\u3083" ?\( "\u3085" ?\) "\u3087" ?~ "\u3092" ?Z "\u3063"
     ;; ?< "、" ?> "。" ?? "・" ?{ "「" ?} "」"
     ?< "\u3001" ?> "\u3002" ?? "\u30fb" ?{ "\u300c" ?} "\u300d"
     ;; ?P "『" ?+ "』" ?_ "ろ"
     ?P "\u300e" ?+ "\u300f" ?_ "\u308d"
     ;; ?F "ゎ" ?T "ヵ" ?* "ヶ"
     ?F "\u308e" ?T "\u30f5" ?* "\u30f6"))
  "Key mapping from key codes to Kana strings based on 106-JP keyboard.")

(defvar mozc-keymap-kana-101us
  (mozc-keymap-make-keymap-from-flat-list
   '(;; 1st row
     ;; ?1 "ぬ" ?2 "ふ" ?3 "あ" ?4 "う" ?5 "え"
     ?1 "\u306c" ?2 "\u3075" ?3 "\u3042" ?4 "\u3046" ?5 "\u3048"
     ;; ?6 "お" ?7 "や" ?8 "ゆ" ?9 "よ" ?0 "わ"
     ?6 "\u304a" ?7 "\u3084" ?8 "\u3086" ?9 "\u3088" ?0 "\u308f"
     ;; ?- "ほ" ?= "へ" ?` "ろ"
     ?- "\u307b" ?= "\u3078" ?` "\u308d"
     ;; 2nd row
     ;; ?q "た" ?w "て" ?e "い" ?r "す" ?t "か"
     ?q "\u305f" ?w "\u3066" ?e "\u3044" ?r "\u3059" ?t "\u304b"
     ;; ?y "ん" ?u "な" ?i "に" ?o "ら" ?p "せ"
     ?y "\u3093" ?u "\u306a" ?i "\u306b" ?o "\u3089" ?p "\u305b"
     ;; ?\[ "゛" ?\] "゜" ?\\ "む"
     ?\[ "\u309b" ?\] "\u309c" ?\\ "\u3080"
     ;; 3rd row
     ;; ?a "ち" ?s "と" ?d "し" ?f "は" ?g "き"
     ?a "\u3061" ?s "\u3068" ?d "\u3057" ?f "\u306f" ?g "\u304d"
     ;; ?h "く" ?j "ま" ?k "の" ?l "り" ?\; "れ"
     ?h "\u304f" ?j "\u307e" ?k "\u306e" ?l "\u308a" ?\; "\u308c"
     ;; ?' "け"
     ?' "\u3051"
     ;; 4th row
     ;; ?z "つ" ?x "さ" ?c "そ" ?v "ひ" ?b "こ"
     ?z "\u3064" ?x "\u3055" ?c "\u305d" ?v "\u3072" ?b "\u3053"
     ;; ?n "み" ?m "も" ?, "ね" ?. "る" ?/ "め"
     ?n "\u307f" ?m "\u3082" ?, "\u306d" ?. "\u308b" ?/ "\u3081"
     ;; shift
     ;; ?# "ぁ" ?E "ぃ" ?$ "ぅ" ?% "ぇ" ?^ "ぉ"
     ?# "\u3041" ?E "\u3043" ?$ "\u3045" ?% "\u3047" ?^ "\u3049"
     ;; ?& "ゃ" ?* "ゅ" ?\( "ょ" ?\) "を" ?Z "っ"
     ?& "\u3083" ?* "\u3085" ?\( "\u3087" ?\) "\u3092" ?Z "\u3063"
     ;; ?< "、" ?> "。" ?? "・" ?{ "「" ?} "」"
     ?< "\u3001" ?> "\u3002" ?? "\u30fb" ?{ "\u300c" ?} "\u300d"
     ;; ?P "『" ?: "』" ?_ "ー" ?| "ー"
     ?P "\u300e" ?: "\u300f" ?_ "\u30fc" ?| "\u30fc"
     ;; ?F "ゎ" ?V "ゐ" ?+ "ゑ" ?T "ヵ" ?\" "ヶ"
     ?F "\u308e" ?V "\u3090" ?+ "\u3091" ?T "\u30f5" ?\" "\u30f6"))
  "Key mapping from key codes to Kana strings based on 101-US keyboard.")

(defvar mozc-keymap-kana mozc-keymap-kana-106jp
  "The default key mapping for Kana input method.")



;;;; Errors

(defmacro mozc-define-error (symbol-name message &rest conditions)
  "Define an error symbol.
SYMBOL-NAME is the name of an error symbol and MESSAGE is its error message.
CONDITIONS is a list of error conditions and shouldn't include symbol `error',
`mozc-error' and SYMBOL-NAME itself.  They are included by default."
  (let ((conditions-list (if (eq symbol-name 'mozc-error)
                             `(error mozc-error ,@conditions)
                           `(error mozc-error ,@conditions ,symbol-name))))
    `(progn
       (put ',symbol-name 'error-conditions ',conditions-list)
       (put ',symbol-name 'error-message ,message))))

(mozc-define-error mozc-error "Error happened inside Mozc")

(mozc-define-error mozc-internal-error "Internal state error")

(mozc-define-error mozc-draw-error "Drawing error")

(mozc-define-error mozc-response-error "Wrong response from the server")

(mozc-define-error mozc-type-error "Type mismatched" mozc-response-error)

(mozc-define-error mozc-session-error "Failed to establish a session")

(mozc-define-error mozc-helper-process-error
                   "Communication error with the helper process")



;;;; LEIM (Library of Emacs Input Method)

(require 'mule)

(defun mozc-leim-activate (input-method)
  "Activate mozc-mode via LEIM.
INPUT-METHOD is not used."
  (let ((new 'deactivate-current-input-method-function)
        (old 'inactivate-current-input-method-function))
    ;; `inactivate-current-input-method-function' is deprecated
    ;; since Emacs 24.3.
    (set (if (boundp new) new old) #'mozc-leim-deactivate))
  (mozc-mode t))

(defun mozc-leim-deactivate ()
  "Deactivate mozc-mode via LEIM."
  (mozc-mode nil))

(defcustom mozc-leim-title "[Mozc]"
  "Mode line string shown when mozc-mode is enabled.
This indicator is not shown when you don't use LEIM."
  :type '(choice (const :tag "No indicator" nil)
                 (string :tag "Show an indicator"))
  :group 'mozc)

(register-input-method
 "japanese-mozc"
 "Japanese"
 #'mozc-leim-activate
 mozc-leim-title
 "Japanese input method with Mozc/Google Japanese Input.")



(provide 'mozc)

;; Local Variables:
;; coding: utf-8
;; End:

;;; mozc.el ends here
