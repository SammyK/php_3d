/* php_3d extension for PHP (c) 2022 Sammy Kaye Powers <sammyk@php.net> */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <php.h>
#include <ext/standard/info.h>
#include <Zend/zend_observer.h>

#include "php_3d.h"
#include "sketchup.h"

ZEND_DECLARE_MODULE_GLOBALS(php_3d)

ZEND_TLS sketchup_building php_3d_building;
ZEND_TLS size_t php_3d_room_index;

void static php3d_zval_to_sval(zval *zval, sketchup_val *sval) {
	switch (Z_TYPE_INFO_P(zval)) {
		case IS_STRING:
			sval->type = SKETCHUP_VAL_STRING;
			sval->ptr = (void *) Z_STRVAL_P(zval);
			break;
		case IS_LONG:
			sval->type = SKETCHUP_VAL_LONG;
			sval->ptr = &Z_LVAL_P(zval);
			break;
		case IS_DOUBLE:
			sval->type = SKETCHUP_VAL_DOUBLE;
			sval->ptr = &Z_DVAL_P(zval);
			break;
		default:
			sval->type = SKETCHUP_VAL_UNSUPPORTED;
			sval->ptr = NULL;
			break;
	}
}

void static php3d_cv_to_3d(zend_array *symbol_table, size_t room_index) {
	size_t var_index = 0;
	zend_string *name;
	zval *var;
	ZEND_HASH_FOREACH_STR_KEY_VAL_IND(symbol_table, name, var) {
		sketchup_val sval = SKETCHUP_NULL;
		php3d_zval_to_sval(var, &sval);
		if (!sketchup_room_append_variable(php_3d_building, room_index, var_index++, ZSTR_VAL(name), sval)) {
			php_log_err("[php_3d] Failed to append variable to room");
		}
	} ZEND_HASH_FOREACH_END();
}

void php3d_fcall_begin_handler(zend_execute_data *execute_data) {
	if (EX(func) && PHP3D_G(generate_model) && php_3d_building.ptr) {
		// TODO Snapshot vars of pre_execute_data
		char *scope = "";
		char *sep = "";
		if (EX(func)->common.scope) {
			zend_class_entry *ce = zend_get_called_scope(execute_data);
			if (ce) {
				scope = ZSTR_VAL(ce->name);
				sep = "::";
			}
		}
		char *fname = EX(func)->common.function_name ? ZSTR_VAL(EX(func)->common.function_name) : "{main}";
		char fqn[256];
		snprintf(fqn, sizeof(fqn), "%s%s%s", scope, sep, fname);
		if (!sketchup_building_append_room(php_3d_building, fqn, php_3d_room_index++)) {
			php_log_err("[php_3d] Failed to append room to building");
		}
	}
}

void php3d_fcall_end_handler(zend_execute_data *execute_data, zval *retval) {
	if (EX(func) && PHP3D_G(generate_model) && php_3d_building.ptr) {
		if (ZEND_CALL_INFO(execute_data) & ZEND_CALL_HAS_SYMBOL_TABLE) {
			php3d_cv_to_3d(EX(symbol_table), php_3d_room_index - 1);
		}
	}
}

zend_observer_fcall_handlers php3d_observer_fcall_init(zend_execute_data *execute_data) {
	return (zend_observer_fcall_handlers) {php3d_fcall_begin_handler, php3d_fcall_end_handler};
}

static void php_3d_init_globals(zend_php_3d_globals *g)
{
	g->generate_model = 0;
}

PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("php_3d.generate_model", "0", PHP_INI_SYSTEM, OnUpdateBool, generate_model, zend_php_3d_globals, php_3d_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(php_3d)
{
	ZEND_INIT_MODULE_GLOBALS(php_3d, php_3d_init_globals, NULL);
	REGISTER_INI_ENTRIES();

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
	php_3d_building.ptr = NULL;
	php_3d_room_index = 0;

	if (PHP3D_G(generate_model)) {
		if (!sketchup_building_ctor(&php_3d_building)) {
			php_log_err("[php_3d] Failed to ctor building");
		}
	}

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(php_3d)
{
#if defined(ZTS) && defined(COMPILE_DL_PHP_3D)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	if (PHP3D_G(generate_model) && php_3d_building.ptr) {
		if (!sketchup_building_save(php_3d_building, "php.skp")) {
			php_log_err("[php_3d] Failed to save .skp file");
		}
		if (!sketchup_building_dtor(php_3d_building)) {
			php_log_err("[php_3d] Failed to dtor building");
		}
	}

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
	PHP_RSHUTDOWN(php_3d),		/* PHP_RSHUTDOWN - Request shutdown */
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
