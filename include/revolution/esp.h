// Reference: zeldaret/tp libs/revolution/include/revolution/esp.h (ESTicket, ESTitleMeta and their field types)
#ifndef REVOLUTION_ESP_H
#define REVOLUTION_ESP_H

#include <revolution/types.h>
#include <revolution/ios/iostypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IOS ES (eTicket services) proxy: the types and entry points the OS and DVD libraries use. */

typedef u64 ESTitleId;
typedef u64 ESSysVersion;
typedef u32 ESId;
typedef u64 ESTicketId;
typedef u8 ESVersion;
typedef u8 ESTicketReserved[48];
typedef u8 ESSysAccessMask[2];
typedef u8 ESCidxMask[64];
typedef u32 ESContentId;
typedef u16 ESContentType;
typedef u32 ESTitleType;
typedef u16 ESTitleVersion;
typedef u8 ESTmdReserved[62];

/* ES hands title IDs to IOS by address: a title ID kept in a local needs 32-byte alignment. */
typedef u64 __attribute__((aligned(32))) ESTitleIdAligned;

/* Limit codes of a ticket's limited-play entries (names invented, values from __OSGetPlayTime). */
#define ES_LIMIT_NONE 0
#define ES_LIMIT_PLAYTIME 1
#define ES_LIMIT_LAUNCHES 4

#define ES_MAX_LIMITED_PLAY 8

/* Error returned for a bad argument (-1017; name invented: tp writes the literal). */
#define ES_EINVAL (-1017)

typedef struct ESLpEntry {
    u32 code;
    u32 limit;
} ESLpEntry;

#pragma pack(push, 4)
/* A ticket view as the ES service hands it out (0xD8 bytes). */
typedef struct ESTicketView {
    u32 view;
    u64 ticketID;
    u32 deviceType;
    u64 titleID;
    u16 accessMask;
    u8 reserved[60];
    u8 cidxMask[64];
    u16 padding; // pad: ES TicketView.padding
    ESLpEntry limits[ES_MAX_LIMITED_PLAY];
} ESTicketView;

/* A signed eTicket (0x2A4 bytes), as stored in a disc partition header. */
typedef struct ESTicket {
    IOSCSigRsa2048 sig;
    IOSCEccPublicKey serverPubKey;
    ESVersion version;
    ESVersion caCrlVersion;
    ESVersion signerCrlVersion;
    IOSCAesKey titleKey;
    ESTicketId ticketId;
    ESId deviceId;
    ESTitleId titleId;
    ESSysAccessMask sysAccessMask;
    u16 ticketVersion;
    u32 accessTitleId;
    u32 accessTitleMask;
    u8 licenseType;
    ESTicketReserved reserved;
    u8 audit;
    ESCidxMask cidxMask;
    ESLpEntry limits[ES_MAX_LIMITED_PLAY];
} ESTicket;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct ESContentMeta {
    ESContentId cid;
    u16 index;
    ESContentType type;
    u64 size;
    IOSCHash hash;
} ESContentMeta;

typedef struct ESTitleMetaHeader {
    ESVersion version;
    ESVersion caCrlVersion;
    ESVersion signerCrlVersion;
    ESSysVersion sysVersion;
    ESTitleId titleId;
    ESTitleType type;
    u16 groupId;
    ESTmdReserved reserved;
    u32 accessRights;
    ESTitleVersion titleVersion;
    u16 numContents;
    u16 bootIndex;
} ESTitleMetaHeader;

/* Title metadata (TMD), 0x49E4 bytes; callers allocate OSRoundUp32B(sizeof(ESTitleMeta)) = 0x4A00. */
typedef struct ESTitleMeta {
    IOSCSigRsa2048 sig;
    ESTitleMetaHeader head;
    ESContentMeta contents[512];
} ESTitleMeta;
#pragma pack(pop)

s32 ESP_InitLib(void);
s32 ESP_CloseLib(void);
s32 ESP_GetTitleId(ESTitleId* titleId);
s32 ESP_GetTicketViews(ESTitleId titleId, ESTicketView* views, u32* count);
s32 ESP_LaunchTitle(ESTitleId titleId, ESTicketView* view);
s32 ESP_DiGetTicketView(const void* ticket, ESTicketView* view);
s32 ESP_DiGetTmd(ESTitleMeta* tmd, u32* size);
s32 ESP_GetConsumption(u64 ticketId, ESLpEntry* entries, u32* nEntries);

#ifdef __cplusplus
}
#endif

#endif
