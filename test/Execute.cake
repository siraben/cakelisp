#!/home/macoy/Repositories/cakelisp/bin/cakelisp --execute
(c-import "<stdio.h>")

(defun main (&return int)
  (printf "Hello, execute!\n")
  (return 0))

(set-cakelisp-option executable-output "test/ExecuteMe")
(add-build-config-label "Debug" "2")
