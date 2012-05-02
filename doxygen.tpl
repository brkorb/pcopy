[= AutoGen5 Template null =]
[= (out-push-new) \=]
/^PROJECT_NAME/     s@=.*@= [= prog-name  =]@
/^PROJECT_NUMBER/   s@=.*@= [= version    =]@
/^PROJECT_BRIEF/    s@=.*@= [= prog-title =]@
/^OUTPUT_DIRECTORY/ s@=.*@= doxy-[= prog-name  =]@
[= (shellf "sed %s %s.doxy | doxygen -"
     (raw-shell-str (out-pop #t)) (get "prog-name"))
=]
