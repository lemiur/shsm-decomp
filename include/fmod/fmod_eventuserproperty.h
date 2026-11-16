/*
 * FMOD::UserProperty: a user property (name and value) of an event or event group, loaded from the
 * project file. Layout: Gormiti DWARF (sibling.exe types UserProperty), size 0x18.
 */
#ifndef _FMOD_EVENTUSERPROPERTY_H
#define _FMOD_EVENTUSERPROPERTY_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class File;

class UserProperty : public LinkedListNode {
public:
    char                    *m_name;    // 0x0C
    FMOD_EVENTPROPERTY_TYPE  m_type;    // 0x10
    void                    *m_value;   // 0x14, the int or float bits, or an owned string

    UserProperty();
    // Copies the property of another event.
    FMOD_RESULT init( UserProperty *src );
    // Frees the name, the string value and the property.
    FMOD_RESULT release();
    FMOD_RESULT load( File *file, unsigned int version );
};

} // namespace FMOD

#endif /* _FMOD_EVENTUSERPROPERTY_H */
