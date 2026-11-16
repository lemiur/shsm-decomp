#include <PowerPC_EABI_Support/Runtime/NMWException.h>

DestructorChain* __global_destructor_chain;

/*
 * __register_global_object
 *
 * Links a constructed static object into the chain destroyed at exit; the
 * compiler supplies the chain node storage in regmem.
 */
void* __register_global_object(void* object, void* destructor, void* regmem) {
    ((DestructorChain*)regmem)->next = __global_destructor_chain;
    ((DestructorChain*)regmem)->destructor = destructor;
    ((DestructorChain*)regmem)->object = object;
    __global_destructor_chain = (DestructorChain*)regmem;

    return object;
} // 0x801A6310

/*
 * __destroy_global_chain
 *
 * Destroys registered static objects in reverse order of construction.
 */
void __destroy_global_chain(void) {
    DestructorChain* iter;

    while ((iter = __global_destructor_chain) != 0) {
        __global_destructor_chain = iter->next;
        DTORCALL_COMPLETE(iter->destructor, iter->object);
    }
} // 0x801A6328
