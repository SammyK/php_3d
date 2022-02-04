/* php_3d extension for PHP (c) 2022 Sammy Kaye Powers <sammyk@php.net> */

#ifndef PHP_3D_H
# define PHP_3D_H

extern zend_module_entry php_3d_module_entry;
# define phpext_php_3d_ptr &php_3d_module_entry

# define PHP_3D_NAME "php_3d"
# define PHP_3D_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_PHP_3D)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

ZEND_BEGIN_MODULE_GLOBALS(php_3d)
	int generate_model;
ZEND_END_MODULE_GLOBALS(php_3d)

#ifdef ZTS
#define PHP3D_G(v) TSRMG(php_3d_globals_id, zend_php_3d_globals *, v)
#else
#define PHP3D_G(v) (php_3d_globals.v)
#endif

#endif	/* PHP_3D_H */
