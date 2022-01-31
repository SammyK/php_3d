/* php_3d extension for PHP (c) 2022 Sammy Kaye Powers <sammyk@php.net> */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_3d.h"

PHP_RINIT_FUNCTION(php_3d)
{
#if defined(ZTS) && defined(COMPILE_DL_PHP_3D)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

PHP_MINFO_FUNCTION(php_3d)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "3D support", "enabled");
	php_info_print_table_end();
}

zend_module_entry php_3d_module_entry = {
	STANDARD_MODULE_HEADER,
	"php_3d",					/* Extension name */
	NULL,						/* zend_function_entry */
	NULL,						/* PHP_MINIT - Module initialization */
	NULL,						/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(php_3d),			/* PHP_RINIT - Request initialization */
	NULL,						/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(php_3d),			/* PHP_MINFO - Module info */
	PHP_3D_VERSION,				/* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHP_3D
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(php_3d)
#endif
