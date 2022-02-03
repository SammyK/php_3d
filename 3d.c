/* php_3d extension for PHP (c) 2022 Sammy Kaye Powers <sammyk@php.net> */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <php.h>
#include <ext/standard/info.h>
#include <Zend/zend_observer.h>

#include "php_3d.h"
#include "sketchup.h"

void php3d_fcall_begin_handler(zend_execute_data *execute_data) {
	//
}

void php3d_fcall_end_handler(zend_execute_data *execute_data, zval *retval) {
	//
}

zend_observer_fcall_handlers php3d_observer_fcall_init(zend_execute_data *execute_data) {
	return (zend_observer_fcall_handlers) {php3d_fcall_begin_handler, php3d_fcall_end_handler};
}

PHP_MINIT_FUNCTION(php_3d)
{
	zend_observer_fcall_register(php3d_observer_fcall_init);
	sketchup_startup();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(php_3d)
{
	sketchup_shutdown();
	return SUCCESS;
}

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
	php_info_print_table_row(2, "SketchUpAPI path", PHP_SKETCHUP_API_PATH);
	char tmp[10];
	sketchup_sdk_version(sizeof(tmp), tmp);
	php_info_print_table_row(2, "SketchUpAPI version", tmp);

	php_info_print_table_end();
}

zend_module_entry php_3d_module_entry = {
	STANDARD_MODULE_HEADER,
	"php_3d",					/* Extension name */
	NULL,						/* zend_function_entry */
	PHP_MINIT(php_3d),			/* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(php_3d),		/* PHP_MSHUTDOWN - Module shutdown */
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
