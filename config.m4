PHP_ARG_ENABLE([3d],
  [whether to enable 3D support],
  [AS_HELP_STRING([--enable-3d],
    [Enable 3D support])],
  [no])

if test "$PHP_3D" != "no"; then
  AC_DEFINE(HAVE_3D, 1, [ Have 3D support ])
  PHP_NEW_EXTENSION(php_3d, 3d.c, $ext_shared)
fi
