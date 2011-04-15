;; Copyright 2010-2011, Google Inc.
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
;; First of all, you need to install a helper program called
;; 'mozc_emacs_helper' into your PATH.  (you can change the program
;; name or path by setting `mozc-helper-program-name').
;; Of course, you want to install mozc.el too.
;;
;; Then you can use mozc-mode via LEIM (Library of Emacs Input Method).
;; Write the following settings into your init file (~/.emacs.d/init.el
;; or ~/.emacs) in order to use mozc-mode by default, or you can call
;; `set-input-method' and set "japanese-mozc" anytime you have loaded
;; mozc.el.
;;
;;   (require 'mozc)  ; or (load-file "/path/to/mozc.el")
;;   (set-language-environment "Japanese")
;;   (setq default-input-method "japanese-mozc")
;;
;; Having the above settings, just type \C-\\ which is bound to
;; `toggle-input-method' by default.

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

(defmacro mozc-characterp (object)
  "Return non-nil if OBJECT is a character.

This macro is equivalent to `characterp' or `char-valid-p' depending on
the Emacs version.  `char-valid-p' is obsolete since Emacs 23."
  (if (fboundp #'characterp)
      `(characterp ,object)
    `(char-valid-p ,object)))



;;;; External interfaces

;; Mode variables
(defvar mozc-mode nil
  "Mode variable of mozc-mode.  Non-nil means mozc-mode is enabled.")
(make-variable-buffer-local 'mozc-mode)

(defvar mozc-mode-hook nil
  "A list of hooks called by `mozc-mode'.")

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
        (error "Connection error"))

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
                (error "Unknown result type"))
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
  "Convert a keyboard event EVENT to a list of key and modifiers and return it.
Key code and symbols are renamed so that the helper process understands them."
  (let* ((basic-type (event-basic-type event))
         (key (case basic-type
                (?\b 'backspace)
                (?\s 'space)
                (?\d 'backspace)
                (t basic-type))))
    ;; `event-basic-type' always returns a lowercase character
    ;; so reconstruct the original uppercase if any.
    (mozc-reconstruct-uppercase-key-event key (event-modifiers event))))

(defun mozc-reconstruct-uppercase-key-event (key modifiers)
  "Reconstruct an uppercase character and return it with modifiers.
If KEY is a key code and it has the corresponding uppercase and 'shift is
included in MODIFIERS, this function reconstructs the uppercase character
and returns a list of the uppercase character and modifiers excluding 'shift.
Otherwise, return a list of the same key and modifiers."
  (if (and (mozc-characterp key) (equal modifiers '(shift))
           (/= key (upcase key)))
      ;; Return the uppercase of the key and modifiers excluding shift.
      (cons (upcase key) nil)
    ;; Return the same key and modifiers.
    (cons key modifiers)))

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
  (mozc-session-delete))

(defsubst mozc-clean-up-changes-on-buffer ()
  "Clean up all changes on the current buffer.
Clean up the preedit and candidate window."
  (mozc-preedit-clean-up)
  (mozc-candidate-clean-up))



;;;; Preedit

(defvar mozc-preedit-in-session-flag nil
  "Non-nil means the current buffer has a preedit session.")
(make-variable-buffer-local 'mozc-preedit-in-session-flag)

(defvar mozc-preedit-point-origin nil
  "This points to the insertion point in the current buffer.")
(make-variable-buffer-local 'mozc-preedit-point-origin)

(defvar mozc-preedit-overlay nil
  "An overlay which shows preedit.")
(make-variable-buffer-local 'mozc-preedit-overlay)

(defun mozc-preedit-init ()
  "Initialize a new preedit session.
The preedit shows up at the current point."
  (mozc-preedit-clean-up)
  ;; Set the origin point.
  (setq mozc-preedit-point-origin (copy-marker (point)))
  ;; Set up an overlay for preedit.
  (let ((origin mozc-preedit-point-origin))
    (setq mozc-preedit-overlay
          (make-overlay origin origin (marker-buffer origin))))
  (setq mozc-preedit-in-session-flag t))

(defun mozc-preedit-clean-up ()
  "Clean up the current preedit session."
  (when mozc-preedit-in-session-flag
    (delete-overlay mozc-preedit-overlay)
    (goto-char mozc-preedit-point-origin))
  (setq mozc-preedit-point-origin nil
        mozc-preedit-in-session-flag nil))

(defsubst mozc-preedit-clear ()
  "Clear the current preedit.
This function expects an update just after this call.
If you want to finish a preedit session, call `mozc-preedit-clean-up'."
  (when mozc-preedit-in-session-flag
    (overlay-put mozc-preedit-overlay 'before-string nil)))

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
              'mozc-preedit-make-text
            ;; Show the segmented preedit.
            'mozc-preedit-make-segmented-text)
          preedit
          (when (memq 'fence mozc-preedit-style)
            '("|" "|" " ")))))
    (if (or (not buffer-read-only)
            (= (length text) 0))
        (overlay-put mozc-preedit-overlay 'before-string text)
      ;; Reset the session and throw away the current preedit, but keep
      ;; the helper process running and connected.
      (mozc-clean-up-session)
      (barf-if-buffer-read-only))))

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
            (apply 'propertize (mozc-protobuf-get segment 'value)
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
        (put-text-property cursor-pos (1+ cursor-pos) 'cursor t text)
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

(defvar mozc-candidate-in-session-flag nil
  "Non-nil means the current buffer has a candidate session.")
(make-variable-buffer-local 'mozc-candidate-in-session-flag)

(defsubst mozc-candidate-init ()
  "Initialize a new candidate session."
  (mozc-candidate-clean-up)
  ;; The current implementation supports only an echo area candidate window
  ;; and it doesn't need any initialization.  Nothing to do here.
  (setq mozc-candidate-in-session-flag t))

(defsubst mozc-candidate-clean-up ()
  "Clean up the current candidate session."
  ;; The current implementation supports only an echo area candidate window
  ;; and it doesn't need any clean-up process.  Nothing to do here.
  (setq mozc-candidate-in-session-flag nil))

(defsubst mozc-candidate-clear ()
  "Clear the current candidate window.
This function expects an update just after this call.
If you want to finish a candidate session, call `mozc-candidate-clean-up'."
  ;; The current implementation supports only an echo area candidate window
  ;; and there is no need to clear it.  Nothing to do here.
  (when mozc-candidate-in-session-flag
    t))

(defsubst mozc-candidate-update (candidates)
  "Update the candidate window.
CANDIDATES must be the candidates field in a response protobuf."
  (unless mozc-candidate-in-session-flag
    (mozc-candidate-init))  ; Initialize if necessary.

  (mozc-cand-echo-area-update candidates))

;;;; Candidate window (echo area version)

(defun mozc-cand-echo-area-update (candidates)
  "Update the candidate list in the echo area.
CANDIDATES must be the candidates field in a response protobuf."
  ;; If the current buffer is a minibuffer, shows only conversion results
  ;; to avoid hiding the original contents of the minibuffer as much as
  ;; possible.  Despite not showing a candidate list for non-conversion
  ;; results, suggestion and prediction are still available and work.
  (when (or (not (minibufferp))
            (eq (mozc-protobuf-get candidates 'category) 'conversion))
    (let (message-log-max)
      (message "%s" (mozc-cand-echo-area-make-contents candidates)))))

(defun mozc-cand-echo-area-make-contents (candidates)
  "Make a list of candidates as an echo area message.
CANDIDATES must be the candidates field in a response protobuf.
Return a string formatted to suit for the echo area."
  (let ((focused-index (mozc-protobuf-get candidates 'focused-index))
        (size (mozc-protobuf-get candidates 'size))
        (index-visible (mozc-protobuf-get candidates 'footer 'index-visible)))
    (apply
     'concat
     ;; Show "focused-index/#total-candidates".
     (when (and index-visible focused-index size)
       (propertize
        (format "%d/%d" (1+ focused-index) size)
        'face 'mozc-cand-echo-area-stats-face))
     ;; Show each candidate.
     (mapcar
      (lambda (candidate)
        (let ((shortcut (mozc-protobuf-get candidate 'annotation 'shortcut))
              (value (mozc-protobuf-get candidate 'value))
              (desc (mozc-protobuf-get candidate 'annotation 'description))
              (index (mozc-protobuf-get candidate 'index)))
          (concat
           " "
           (propertize  ; shortcut
            (if shortcut
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
    (apply 'mozc-session-execute-command 'SendKey key-list)))

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
    (apply 'mozc-helper-process-send-sexpr
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
          (error "Failed to start a new session")
          nil))
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
               (proc (start-process "mozc-helper-process" nil
                                    mozc-helper-program-name)))
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
            (message "mozc.el: Wrong start-up message from the helper process.")
            (signal 'error nil)))
      (error  ; Abort unless the helper process successfully runs.
       (mozc-abort)
       (error "Failed to start mozc-helper-process"))))
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
        'no-data-available  ; No data received.
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
  "Look for a series of keys in ALIST recursively.
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
             (apply 'mozc-protobuf-get value keys)
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



;;;; LEIM (Library of Emacs Input Method)

(require 'mule)

(defun mozc-leim-activate (input-method)
  "Activate mozc-mode via LEIM.
INPUT-METHOD is not used."
  (setq inactivate-current-input-method-function 'mozc-leim-inactivate)
  (mozc-mode t))

(defun mozc-leim-inactivate ()
  "Inactivate mozc-mode via LEIM."
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
 'mozc-leim-activate
 mozc-leim-title
 "Japanese input method with Mozc/Google Japanese Input.")



(provide 'mozc)

;; Local Variables:
;; coding: utf-8
;; End:

;;; mozc.el ends here
