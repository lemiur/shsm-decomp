#ifndef RWSDK_RPCRITER_H
#define RWSDK_RPCRITER_H

#include <rwsdk/rwtypes.h>

/*
 * IDs of Criterion's plugins and toolkits (vendor rwVENDORID_CRITERIONTK): each plugin registers
 * its data and its stream chunks under its ID, and its memory hints carry it. Names, values and the
 * #define form are the SDK's.
 */
#define rwID_USERDATAPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x1F)
#define rwID_QUATPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x95)
#define rwID_SLERPPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x83)
#define rwID_RANDOMPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x0C)
/* The collision plugin's ID; retail's registrations pass 0x11D. */
#define rwID_COLLISPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x1D)
/* The PVS plugin's ID; value from retail's plugin registrations (0x2A in the Criterion toolkit range). */
#define rwID_PVSPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x2A)
#define rwID_ANIMTOOLKIT MAKECHUNKID(rwVENDORID_CRITERIONTK, 0xB7)
/* The skin split toolkit's ID, in its memory hints. Name invented (the RW SDK's is not in this tree's headers); value from retail's 0x101B8. */
#define rwID_SKINSPLITTOOKIT MAKECHUNKID(rwVENDORID_CRITERIONTK, 0xB8)
#define rwID_DICTTOOLKIT MAKECHUNKID(rwVENDORID_CRITERIONTK, 0xBF)

#endif
