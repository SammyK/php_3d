PHP_ARG_ENABLE([3d],
  [whether to enable 3D support],
  [AS_HELP_STRING([--enable-3d],
    [Enable 3D support])],
  [no])

PHP_ARG_WITH([sketchup-api],
  [for SketchUpAPI support],
  [AS_HELP_STRING([[--with-sketchup-api[=DIR]]],
    [Include SketchUpAPI support (macOS only)])])

if test "$PHP_3D" != "no"; then
  for i in $PHP_SKETCHUP_API $HOME/Library/Frameworks /Library/Frameworks /System/Library/Frameworks; do
    test -f $i/SketchUpAPI.framework/SketchUpAPI && SKETCHUP_DIR=$i && break
  done

  if test -z "$SKETCHUP_DIR"; then
    AC_MSG_ERROR(Cannot find SketchUpAPI framework)
  fi

  AC_DEFINE_UNQUOTED(PHP_SKETCHUP_API_PATH, "${SKETCHUP_DIR}/SketchUpAPI.framework", [ ])

  dnl Why this no worky?
  dnl PHP_ADD_FRAMEWORK_WITH_PATH(SketchUpAPI, $SKETCHUP_DIR)
  PHP_3D_CFLAGS="-F$SKETCHUP_DIR -framework SketchUpAPI"
  EXTRA_LDFLAGS="$EXTRA_LDFLAGS -F$SKETCHUP_DIR -framework SketchUpAPI -rpath $SKETCHUP_DIR"

  AC_DEFINE(HAVE_3D, 1, [ Have 3D support ])
  PHP_NEW_EXTENSION(php_3d, 3d.c, $ext_shared, , $PHP_3D_CFLAGS)
fi
