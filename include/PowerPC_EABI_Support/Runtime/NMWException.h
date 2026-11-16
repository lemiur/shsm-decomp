#ifndef NMWEXCEPTION_H
#define NMWEXCEPTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constructors and destructors called through untyped pointers take a
 * hidden argument: 1 constructs the complete object (with virtual bases),
 * -1 destroys it without freeing its storage. */
typedef void* ConstructorDestructor;

#define CTORARG_TYPE int
#define CTORARG_PARTIAL (0)
#define CTORARG_COMPLETE (1)
#define CTORCALL_COMPLETE(ctor, objptr) (((void (*)(void*, CTORARG_TYPE))ctor)(objptr, CTORARG_COMPLETE))

#define DTORARG_TYPE int
#define DTORARG_UNKNOWN (-1)
#define DTORCALL_COMPLETE(dtor, objptr) (((void (*)(void*, DTORARG_TYPE))dtor)(objptr, DTORARG_UNKNOWN))

struct __eti_init_info;

typedef struct DestructorChain {
    struct DestructorChain* next;
    void* destructor;
    void* object;
} DestructorChain;

extern DestructorChain* __global_destructor_chain;

void* __register_global_object(void* object, void* destructor, void* regmem);
void __destroy_global_chain(void);

void* __construct_new_array(void* block, ConstructorDestructor ctor, ConstructorDestructor dtor, size_t size, size_t n);
void __construct_array(void* ptr, ConstructorDestructor ctor, ConstructorDestructor dtor, size_t size, size_t n);
void __destroy_arr(void* block, ConstructorDestructor dtor, size_t size, size_t n);
void __destroy_new_array(void* block, ConstructorDestructor dtor);

int __register_fragment(struct __eti_init_info* info, char* TOC);
void __unregister_fragment(int fragmentID);

#ifdef __cplusplus
}
#endif

#endif
