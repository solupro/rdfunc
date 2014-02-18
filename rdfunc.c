/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_rdfunc.h"
#include <pcre.h>

/* If you declare any globals in php_rdfunc.h uncomment this:*/
ZEND_DECLARE_MODULE_GLOBALS(rdfunc)


/* True global resources - no need for thread safety here */
static int le_rdfunc;

/* {{{ rdfunc_functions[]
 *
 * Every user visible function must have an entry in rdfunc_functions[].
 */
const zend_function_entry rdfunc_functions[] = {
	PHP_FE(confirm_rdfunc_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in rdfunc_functions[] */
};
/* }}} */

/* {{{ rdfunc_module_entry
 */
zend_module_entry rdfunc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"rdfunc",
	rdfunc_functions,
	PHP_MINIT(rdfunc),
	PHP_MSHUTDOWN(rdfunc),
	PHP_RINIT(rdfunc),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(rdfunc),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(rdfunc),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_RDFUNC
ZEND_GET_MODULE(rdfunc)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini*/
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("rdfunc.disable_functions", "", PHP_INI_ALL, OnUpdateString, disable_functions, zend_rdfunc_globals, rdfunc_globals)
PHP_INI_END()

/* }}} */

/* {{{ php_rdfunc_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_rdfunc_init_globals(zend_rdfunc_globals *rdfunc_globals)
{
	rdfunc_globals->global_value = 0;
	rdfunc_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ my functions
 */

#define OVERCCOUNT 30
#define MAX_REGEX_COUNT 50 //最大支持规则数量

static char *replace_start(char *src) { //替换通配符*号
    static char buffer[4096];
    char *p, *str;
    char *orig = "*";
    char *rep = "(\\w+)";
    
    str = (char *)emalloc(4096);
    p = strstr(src, orig);
    if (p == src) {
        sprintf(str, "%s%s", src, "$");
    } else {
        sprintf(str, "%s%s%s", "^", src, "$");
    }
    if (!(p = strstr(str, orig))) {
        return str;
    }
    strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
    buffer[p-str] = '\0';

    sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));
    efree(str);
    return buffer;
}

static int matchpattern(const char *src, char *pattern) {
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[OVERCCOUNT];
    int rc;
    re = pcre_compile(pattern, PCRE_CASELESS|PCRE_DOTALL, &error, &erroffset, NULL);
    if (re == NULL)
        return 0;
    rc = pcre_exec(re, NULL, src, strlen(src), 0, 0, ovector, OVERCCOUNT);
    free(re);
    return rc;
}


PHP_FUNCTION(print_disabed_info)
{  
    /*
        I don't know why I can't use get_active_function_name
        but without 
        if (!zend_is_executing(TSRMLS_C)) {
            return NULL;
        }
        it run!!!

        just ZEND_INTERNAL_FUNCTION only.
    */
    const char *func;
    switch (EG(current_execute_data)->function_state.function->type) {
        case ZEND_USER_FUNCTION: {
                const char *function_name = ((zend_op_array *) EG(current_execute_data)->function_state.function)->function_name;

                if (function_name) {
                    func = function_name;
                } else {
                    func = "main";
                }
            }
            break;
        case ZEND_INTERNAL_FUNCTION:
            func = ((zend_internal_function *) EG(current_execute_data)->function_state.function)->function_name;
            break;
        default:
            func = "?";
    }
    
    zend_error(E_WARNING, "rdfunc: %s has been disabled! (°Д°≡°д°)ｴｯ!?", func);
}

static zend_function_entry disabled_function[] = {
    PHP_FALIAS(display_disabled_function, print_disabed_info, NULL)
    PHP_FE_END
};

static void remove_function() {
#ifdef ZEND_SIGNALS
    TSRMLS_FETCH();
#endif

    char *s, *p;
    char *delim = ", ";//这里支持,号和空格来分割规则
    char *regex_list[MAX_REGEX_COUNT] = {0};

    int i = 0;
    s = estrndup(RDFUNC_G(disable_functions), strlen(RDFUNC_G(disable_functions)));
    p = strtok(s, delim);
    if (p) {
        do {
            //p = replace_str(p, "*", "(\\w+)");
            p = replace_start(p);
            regex_list[i] = estrndup(p, strlen(p));
            i++;
        } while ((p = strtok(NULL, delim)));
    }
   
    int match = -1, k;
    char *regex;
    HashTable ht_func, *pht_func;
    Bucket *pBk;
    //拷贝一份CG(function_table)进行操作
    zend_hash_init(&ht_func, zend_hash_num_elements(CG(function_table)), NULL, NULL, 0);
    zend_hash_copy(&ht_func, CG(function_table), NULL, NULL, sizeof(zval*));
    pht_func = &ht_func;

    for (pBk = pht_func->pListHead; pBk != NULL; pBk = pBk->pListNext) {
        for (k = 0; k < MAX_REGEX_COUNT; k++) {
            regex = regex_list[k];
            if (!regex) break;
            //regex = "^array_p(\\w+)";
            match = matchpattern(pBk->arKey, regex);
            if (match >= 0) {
                //printf("function:%s are disabled!!\n", pBk->arKey);
                //zend_disable_function(func, sizeof(func));
                if (zend_hash_del(CG(function_table), pBk->arKey, strlen(pBk->arKey)+1) == FAILURE) {
                    printf("disable %s error\n", pBk->arKey);
                };
                disabled_function[0].fname = pBk->arKey;
                zend_register_functions(NULL, disabled_function, CG(function_table), MODULE_PERSISTENT TSRMLS_CC);
            }
        }
    }

    //free memory
    zend_hash_destroy(&ht_func); //销毁HashTable
    pht_func = NULL;
    for(i = 0; i < MAX_REGEX_COUNT; i++) {
        regex = regex_list[i];
        if (regex) {
            efree(regex);
            regex_list[i] = NULL;
        }
    }
    efree(s);
    s = NULL;   
}

/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(rdfunc)
{
	/* If you have INI entries, uncomment these lines */
	REGISTER_INI_ENTRIES();

	remove_function();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(rdfunc)
{
	/* uncomment this line if you have INI entries */
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(rdfunc)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(rdfunc)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(rdfunc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "rdfunc support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini */
	DISPLAY_INI_ENTRIES();
	
}
/* }}} */


/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_rdfunc_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_rdfunc_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "rdfunc", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
