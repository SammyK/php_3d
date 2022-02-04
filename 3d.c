/* php_3d extension for PHP (c) 2022 Sammy Kaye Powers <sammyk@php.net> */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <php.h>
#include <ext/standard/info.h>
#include <Zend/zend_extensions.h>
#include <Zend/zend_observer.h>

#include "php_3d.h"
#include "sketchup.h"

ZEND_DECLARE_MODULE_GLOBALS(php_3d)

#define PHP3D_OP_ARRAY_EXTENSION(op_array) ZEND_OP_ARRAY_EXTENSION(op_array, php3d_op_array_extension)

int php3d_op_array_extension = 0;

typedef struct php3d_visit_info_s {
	size_t room_index;
	size_t visit_count;
} php3d_visit_info;

ZEND_TLS sketchup_town php3d_town;
ZEND_TLS size_t php3d_room_next_index;
ZEND_TLS php3d_visit_info php3d_room_visit_info[SUP_MAX_ROOMS];

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

void static php3d_cv_to_3d(zend_execute_data *execute_data, size_t room_index, size_t visit_index) {
	if (!EX(func)) return;
	size_t cv_count = (size_t) EX(func)->op_array.last_var;
	if (!cv_count) return;

	zend_string **cv_names = EX(func)->op_array.vars;
	zval *var = ZEND_CALL_VAR_NUM(execute_data, 0);

	for (size_t i = 0; i < cv_count; i++) {
		sketchup_val sval = SKETCHUP_NULL;
		php3d_zval_to_sval(var, &sval);
		if (!sketchup_room_append_variable(php3d_town, room_index, visit_index, i, ZSTR_VAL(cv_names[i]), sval)) {
			php_log_err("[php_3d] Failed to append variable to room");
		}
		var++;
	}
}

void php3d_fcall_begin_handler(zend_execute_data *execute_data) {
	if (EX(func) && PHP3D_G(generate_model) && php3d_town.ptr) {
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

		php3d_visit_info *visit = (php3d_visit_info *)PHP3D_OP_ARRAY_EXTENSION(&EX(func)->op_array);
		if (!visit) {
			visit = PHP3D_OP_ARRAY_EXTENSION(&EX(func)->op_array) = &php3d_room_visit_info[php3d_room_next_index];
			visit->room_index = php3d_room_next_index;
			visit->visit_count = 0;
			php3d_room_next_index++;
		} else {
			visit->visit_count++;
		}

		if (!sketchup_town_append_room(php3d_town, fqn, visit->room_index, visit->visit_count)) {
			php_log_err("[php_3d] Failed to append room to town");
		}
	}
}

void php3d_fcall_end_handler(zend_execute_data *execute_data, zval *retval) {
	if (EX(func) && PHP3D_G(generate_model) && php3d_town.ptr) {
		php3d_visit_info *visit = (php3d_visit_info *)PHP3D_OP_ARRAY_EXTENSION(&EX(func)->op_array);
		ZEND_ASSERT(visit);
		// TODO Fix visit count issue
		php3d_cv_to_3d(execute_data, visit->room_index, visit->visit_count);
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
	STD_PHP_INI_BOOLEAN(PHP_3D_NAME ".generate_model", "0", PHP_INI_SYSTEM, OnUpdateBool, generate_model, zend_php_3d_globals, php_3d_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(php_3d)
{
	php3d_op_array_extension = zend_get_op_array_extension_handle(PHP_3D_NAME);

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
	php3d_town.ptr = NULL;
	php3d_room_next_index = 0;

	if (PHP3D_G(generate_model)) {
		if (!sketchup_town_ctor(&php3d_town)) {
			php_log_err("[php_3d] Failed to ctor town");
		}
	}

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(php_3d)
{
#if defined(ZTS) && defined(COMPILE_DL_PHP_3D)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	if (PHP3D_G(generate_model) && php3d_town.ptr) {
		if (!sketchup_town_save(php3d_town, "php.skp")) {
			php_log_err("[php_3d] Failed to save .skp file");
		}
		if (!sketchup_town_dtor(php3d_town)) {
			php_log_err("[php_3d] Failed to dtor town");
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
	PHP_3D_NAME,				/* Extension name */
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
