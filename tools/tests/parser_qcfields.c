/* Production SSQC/CSQC field mappings, checked against allocated real structs. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef float pvec_t, pvec3_t[3], vec_t, vec3_t[3];
typedef int32_t pint_t, string_t, func_t;
typedef uint64_t puint64_t;
typedef unsigned char qbyte;
/* ENGINE_ENTITY_TYPES */
/* ENGINE_CSQC_TYPES */
enum { ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_function, ev_integer };
static char server_id, client_id;
static void *svprogfuncs = &server_id, *csqcprogs = &client_id;
static struct { void *program; int type; const char *name; size_t offset; } records[2048];
static unsigned count, checked;
static void PR_RegisterFieldVar(void *program, int type, const char *name, size_t offset, int index)
{
    assert(index == -1 && name && count < sizeof(records)/sizeof(records[0]));
    records[count].program = program; records[count].type = type;
    records[count].name = name; records[count++].offset = offset;
}
static void register_server(void) {
/* ENGINE_SSQC_REGISTRATION */
}
static void register_client(void) {
/* ENGINE_CSQC_REGISTRATION */
}
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
static void expect(void *program, int type, const char *name, size_t offset)
{
    for (unsigned i = 0; i < count; ++i)
        if (records[i].program == program && !strcmp(records[i].name, name)) {
            assert(records[i].type == type && records[i].offset == offset); ++checked; return;
        }
    fprintf(stderr, "Missing field: %s\n", name); assert(0);
}
#define ADDRESS_OFFSET(object, field) ((size_t)((unsigned char *)&(object).field - (unsigned char *)&(object)))
/* ENGINE_CONTAINER_MACRO */
typedef struct link_s { struct link_s *prev, *next; } link_t;
typedef struct { uint64_t prefix[3]; link_t link; unsigned tail; } linked_t;
int main(void)
{
    stdentvars_t server = {0}; csqcentvars_t client = {0};
#ifdef VM_Q1
    extentvars_t server_extra = {0}; csqcextentvars_t client_extra = {0};
#endif
    register_server(); register_client();
#define TEST_OBJECT server
#define TEST_BIAS 0
#define comfieldfloat(name, desc) expect(svprogfuncs, ev_float, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldint(name, desc) expect(svprogfuncs, ev_integer, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldvector(name, desc) expect(svprogfuncs, ev_vector, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldentity(name, desc) expect(svprogfuncs, ev_entity, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldstring(name, desc) expect(svprogfuncs, ev_string, (!strcmp(#name, "message") ? "_message" : #name), TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldfunction(name, typestr, desc) expect(svprogfuncs, ev_function, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
comqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#undef TEST_OBJECT
#undef TEST_BIAS
#ifdef VM_Q1
#define TEST_OBJECT server_extra
#define TEST_BIAS sizeof(server)
#else
#define TEST_OBJECT server
#define TEST_BIAS 0
#endif
#define comfieldfloat(name, desc) expect(svprogfuncs, ev_float, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldint(name, desc) expect(svprogfuncs, ev_integer, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldvector(name, desc) expect(svprogfuncs, ev_vector, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldentity(name, desc) expect(svprogfuncs, ev_entity, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldstring(name, desc) expect(svprogfuncs, ev_string, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldfunction(name, typestr, desc) expect(svprogfuncs, ev_function, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
comextqcfields
svextqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#undef TEST_OBJECT
#undef TEST_BIAS
#define TEST_OBJECT client
#define TEST_BIAS 0
#define comfieldfloat(name, desc) expect(csqcprogs, ev_float, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldint(name, desc) expect(csqcprogs, ev_integer, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldvector(name, desc) expect(csqcprogs, ev_vector, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldentity(name, desc) expect(csqcprogs, ev_entity, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldstring(name, desc) expect(csqcprogs, ev_string, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldfunction(name, typestr, desc) expect(csqcprogs, ev_function, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
comqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#undef TEST_OBJECT
#undef TEST_BIAS
#ifdef VM_Q1
#define TEST_OBJECT client_extra
#define TEST_BIAS sizeof(client)
#else
#define TEST_OBJECT client
#define TEST_BIAS 0
#endif
#define comfieldfloat(name, desc) expect(csqcprogs, ev_float, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldint(name, desc) expect(csqcprogs, ev_integer, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldvector(name, desc) expect(csqcprogs, ev_vector, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldentity(name, desc) expect(csqcprogs, ev_entity, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldstring(name, desc) expect(csqcprogs, ev_string, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
#define comfieldfunction(name, typestr, desc) expect(csqcprogs, ev_function, #name, TEST_BIAS + ADDRESS_OFFSET(TEST_OBJECT, name));
comextqcfields
csqcextfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#undef TEST_OBJECT
#undef TEST_BIAS
    assert(checked == count && count > 100);
    linked_t object = {0};
    assert(STRUCT_FROM_LINK(&object.link, linked_t, link) == &object);
    printf("QuakeC fields: %u server/client type/name/byte offsets match allocated structures; link container roundtrip passed\n", checked);
    return 0;
}
