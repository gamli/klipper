#ifndef LOGGING_H
#define LOGGING_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include "trapq.h"


struct text
{
    void* Ref;
};

EXTERNC void log_c_root(struct text t);

EXTERNC void log_c_print();

EXTERNC void log_c_discard();

EXTERNC void log_c_end();

EXTERNC void log_c_t(struct text t);

#define CONCAT_IMPL( x, y ) x##y
#define MACRO_CONCAT( x, y ) CONCAT_IMPL( x, y )

inline void cleanup_log_c_context(int* log_depth)
{
    for (int i = 0; i < *log_depth; ++i)
    {
        log_c_end();
    }
}
#define LOG_C_CONTEXT int log_depth __attribute__((__cleanup__ (cleanup_log_c_context))) = 0;

#define LOG_C_SENTINEL(...) __VA_ARGS__; log_depth++;

#define LOG_C_END log_c_end(); log_depth--;
    

// ONE

EXTERNC struct text log_one(const char* fmt, ...);
EXTERNC void log_c_one(const char* fmt, ...);

// MULTI

EXTERNC struct text log_multi();
EXTERNC void log_multi_add(struct text multi_text, struct text text_to_add);

EXTERNC void log_c_multi();
#define LOG_C_MULTI LOG_C_SENTINEL(log_c_multi())

// INDENT

EXTERNC struct text log_indent(struct text text);

// COLUMNS

EXTERNC struct text log_columns(const char* separator);
EXTERNC void log_columns_add(struct text column_text, struct text column_to_add);

EXTERNC void log_c_columns(const char* separator);
#define LOG_C_COLUMNS(SEP) LOG_C_SENTINEL(log_c_columns(SEP))

// XML

EXTERNC struct text log_xml(const char* tag);
EXTERNC void log_xml_attr(struct text xml_text, const char* name, const char* value);
EXTERNC void log_xml_inner(struct text xml_text, struct text inner_text);

EXTERNC void log_c_xml(const char* tag);
#define LOG_C_XML(TAG) LOG_C_SENTINEL(log_c_xml(TAG))
EXTERNC void log_c_xml_attr(const char* name, const char* value);
#define LOG_C_XML_ATTR(NAME, VALUE) LOG_C_SENTINEL(log_c_xml_attr(NAME, VALUE))

// SECTION

EXTERNC struct text log_section(const char* name);
EXTERNC void log_section_content(struct text section_text, struct text content);

EXTERNC void log_c_section(const char* name);
#define LOG_C_SECTION(NAME) LOG_C_SENTINEL(log_c_section(NAME))

// FUNCTION

EXTERNC struct text log_function(const char* name);
EXTERNC void log_function_params(struct text function_text, struct text param_text);
EXTERNC void log_function_body(struct text function_text, struct text body_text);
EXTERNC void log_function_return(struct text function_text, struct text return_text);

EXTERNC void log_c_function(const char* name);
#define LOG_C_FUNCTION LOG_C_SENTINEL(log_c_function(__func__))
EXTERNC void log_c_function_params();
#define LOG_C_FUNCTION_PARAMS LOG_C_SENTINEL(log_c_function_params())
EXTERNC void log_c_function_body();
#define LOG_C_FUNCTION_BODY LOG_C_SENTINEL(log_c_function_body())
EXTERNC void log_c_function_return();
#define LOG_C_FUNCTION_RETURN LOG_C_SENTINEL(log_c_function_return())

// LOOP

EXTERNC struct text log_loop(const char* name);
EXTERNC struct text log_loop_iter(int n);

EXTERNC void log_c_loop(const char* name);
#define LOG_C_LOOP(NAME) LOG_C_SENTINEL(log_c_loop(NAME))
EXTERNC void log_c_loop_iter(int n);
#define LOG_C_LOOP_ITER(N) LOG_C_SENTINEL(log_c_loop_iter(N))

// VALUES

EXTERNC struct text log_values_tag(const char* tag);
EXTERNC void log_values_add(struct text values_text, const char* name, const char* value);
EXTERNC void log_values_add_t(struct text values_text, const char* name, struct text value);
EXTERNC void log_values_add_i(struct text values_text, const char* name, int value);
EXTERNC void log_values_add_d(struct text values_text, const char* name, double value);
EXTERNC void log_values_add_b(struct text values_text, const char* name, int value);

EXTERNC void log_c_values_tag(const char* tag);
EXTERNC void log_c_values();
#define LOG_C_VALUES LOG_C_SENTINEL(log_c_values())
EXTERNC void log_c_values_add(const char* name, const char* value);
EXTERNC void log_c_values_add_t(const char* name, struct text value);
EXTERNC void log_c_values_add_i(const char* name, int value);
EXTERNC void log_c_values_add_d(const char* name, double value);
EXTERNC void log_c_values_add_b(const char* name, int value);


inline struct text log_value_coord3(double x, double y, double z)
{
    return log_one("{ x=%f, y=%f, z=%f }", x, y, z);
}

inline struct text log_value_coord(struct coord coord)
{
    return log_value_coord3(coord.x, coord.y, coord.z);
}

EXTERNC struct text log_value_move(struct move* move);

#endif // logging.h
