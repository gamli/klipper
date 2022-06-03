#ifndef LOGGING_H
#define LOGGING_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif


struct text
{
    void* Ref;
};


EXTERNC void log_print(struct text text);
EXTERNC void log_c_print();

EXTERNC void log_c_root(struct text t);

EXTERNC struct text log_one(const char* fmt, ...);
EXTERNC void log_c_one(const char* fmt, ...);

EXTERNC struct text log_multi();
EXTERNC void log_c_multi_begin();
EXTERNC void log_c_multi_end();
EXTERNC void log_multi_add(struct text multi_text, struct text text_to_add);
EXTERNC void log_c_multi_add(struct text text_to_add);
EXTERNC struct text log_multi_2(struct text txt1, struct text txt2);
EXTERNC struct text log_multi_3(struct text txt1, struct text txt2, struct text txt3);
EXTERNC struct text log_multi_4(struct text txt1, struct text txt2, struct text txt3, struct text txt4);
EXTERNC struct text log_multi_5(struct text txt1, struct text txt2, struct text txt3, struct text txt4,
                                struct text txt5);

EXTERNC struct text log_indent(struct text text);

EXTERNC struct text log_columns(const char* separator);
EXTERNC void log_c_columns_begin(const char* separator);
EXTERNC void log_c_columns_end();
EXTERNC void log_columns_add(struct text column_text, struct text column_to_add);

EXTERNC struct text log_xml(const char* tag);
EXTERNC void log_c_xml_begin(const char* tag);
EXTERNC void log_c_xml_end();
EXTERNC void log_xml_attr(struct text xml_text, const char* name, const char* value);
EXTERNC void log_c_xml_attr(const char* name, const char* value);
EXTERNC void log_xml_inner(struct text xml_text, struct text inner_text);
EXTERNC void log_c_xml_inner(struct text inner_text);

EXTERNC struct text log_section(const char* name);
EXTERNC void log_c_section_begin(const char* name);
EXTERNC void log_c_section_end();
EXTERNC void log_section_content(struct text section_text, struct text content);
EXTERNC void log_c_section_content(struct text content);

EXTERNC struct text log_function(const char* name);
EXTERNC void log_c_function_begin(const char* name);
EXTERNC void log_c_function_end();
EXTERNC void log_c_function_params_begin();
EXTERNC void log_c_function_params_end();
EXTERNC void log_c_function_body_begin();
EXTERNC void log_c_function_body_end();
EXTERNC void log_c_function_return_begin();
EXTERNC void log_c_function_return_end();

EXTERNC struct text log_loop(const char* name);
EXTERNC void log_c_loop_begin(const char* name);
EXTERNC void log_c_loop_end();
EXTERNC struct text log_loop_iter(int n);
EXTERNC void log_c_loop_iter_begin(int n);
EXTERNC void log_c_loop_iter_end();

EXTERNC struct text log_values();
EXTERNC void log_c_values_begin();
EXTERNC void log_c_values_end();
EXTERNC void log_values_add(struct text values_text, const char* name, const char* value);
EXTERNC void log_c_values_add(const char* name, const char* value);
EXTERNC void log_values_add_t(struct text values_text, const char* name, struct text value);
EXTERNC void log_c_values_add_t(const char* name, struct text value);
EXTERNC void log_values_add_i(struct text values_text, const char* name, int value);
EXTERNC void log_c_values_add_i(const char* name, int value);
EXTERNC void log_values_add_d(struct text values_text, const char* name, double value);
EXTERNC void log_c_values_add_d(const char* name, double value);
EXTERNC void log_value_add_b(struct text values_text, const char* name, int value);
EXTERNC void log_c_values_add_b(const char* name, int value);

#endif // logging.h
