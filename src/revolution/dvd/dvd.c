// Reference: zeldaret/tp libs/revolution/src/dvd/dvd.c (primary), with doldecomp/ogws DVD/dvd.c where the Wii-era
// code differs. The retail bytes win over both.
// The debug-only ASSERTLINE and ASSERTMSGLINE checks compile to nothing in this release build and are left out
// (see dvdfs.c). DVD_ASSERTMSGLINE checks stay, with the retail build's line numbers. BB2 (as in ogws), CurrDiskID
// and __DVDTmdBuffer carry the alignments the retail .bss gaps require (DMA targets, ES tmd buffer); an aligned
// object in a function, inlined bodies included, turns MWCC's IR optimiser off for it, which keeps StampIntType's
// lastCommand[CommandInfoCounter - 1] unfolded (subi) in the callbacks that reach BB2 or CurrDiskID, as in retail.
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <string.h>

const char* __DVDVersion = "<< RVL_SDK - DVD \trelease build: Feb 27 2009 10:01:59 (0x4302_145) >>";

typedef void (*stateFunc)(DVDCommandBlock* block);
stateFunc LastState;

static DVDCommandBlock* executing;
static DVDDiskID* IDShouldBe;
static OSBootInfo* bootInfo;
static vu32 CurrCommand; // volatile: SDK declaration (zeldaret/tp dvd.c)
static void (*CancelCallback)(s32, DVDCommandBlock*);
static vu32 CancelLastError; // volatile: SDK declaration (zeldaret/tp dvd.c)
static u32 LastError;
static volatile BOOL ResetRequired; // volatile: retail keeps the LastResetEnd, ResetRequired and ResumeFromHere stores in program order (cbForStateReset, cbForStateBusy); zeldaret/tp and ogws declare it plain
static u32 MotorState;
static volatile OSTime LastResetEnd; // volatile: SDK declaration (zeldaret/tp dvd.c)
static u32 __DVDNumTmdBytes ATTRIBUTE_ALIGN(32);
static DVDGameTOC* GameToc;
static DVDPartitionInfo* PartInfo;
static DVDPartitionInfo* BootGameInfo;
static volatile BOOL Prepared; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 __BS2DVDLowIntType; // volatile: SDK declaration (zeldaret/tp dvd.c)
static int DVDInitialized;
vu32 __DVDLayoutFormat; // volatile: SDK declaration (zeldaret/tp dvd.c)
static volatile BOOL PreparingCover; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 ChangedDisc; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 MotorStopped; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 WaitingForCoverClose; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 WaitingForCoverOpen; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 Breaking; // volatile: SDK declaration (zeldaret/tp dvd.c)
static BOOL FirstTimeInBootrom;
static vs32 NumInternalRetry; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 ResumeFromHere; // volatile: SDK declaration (zeldaret/tp dvd.c)
static vu32 Canceling; // volatile: SDK declaration (zeldaret/tp dvd.c)
static BOOL FatalErrorFlag;
static volatile BOOL PausingFlag; // volatile: SDK declaration (zeldaret/tp dvd.c)
static volatile BOOL PauseFlag; // volatile: SDK declaration (zeldaret/tp dvd.c)

static BOOL autoInvalidation = TRUE;
static int CancelAllSyncComplete;
volatile u32 CommandInfoCounter = 0; // volatile: SDK declaration (zeldaret/tp dvd.c)

static void defaultOptionalCommandChecker(DVDCommandBlock*, DVDCommandCheckerCallback);
static DVDCommandChecker checkOptionalCommand = defaultOptionalCommandChecker;


static DVDBB2 BB2 ATTRIBUTE_ALIGN(32);
static DVDDiskID CurrDiskID ATTRIBUTE_ALIGN(32);
static DVDCommandBlock DummyCommandBlock;
static OSAlarm ResetAlarm;
static OSAlarm CoverAlarm;

static u8 __DVDGameTocBuffer[OSRoundUp32B(sizeof(DVDGameTOC) * 4)] ATTRIBUTE_ALIGN(32);
static u8 __DVDPartInfoBuffer[OSRoundUp32B(sizeof(DVDPartitionInfo) * 4)] ATTRIBUTE_ALIGN(32);
static u8 __DVDTmdBuffer[OSRoundUp32B(sizeof(ESTitleMeta))] ATTRIBUTE_ALIGN(64);
static u8 __DVDTicketViewBuffer[OSRoundUp32B(sizeof(ESTicketView))] ATTRIBUTE_ALIGN(32);

static OSAlarm FatalAlarm;
DVDCommandBlock __DVDStopMotorCommandBlock;
// Name from the Gormiti sibling's Final link map (object dvd.o); retail references no copy of it, the object only fills the .bss gap.
DVDCommandBlock __DVDRestartMotorCommandBlock;

// prototypes
static void stateDownRotation(DVDCommandBlock*);
static void stateReadingTOC(DVDCommandBlock*);
static void stateReadingPartitionInfo(DVDCommandBlock* block);
static void stateOpenPartition(DVDCommandBlock* block);
static void stateOpenPartition2(DVDCommandBlock* block);
static void stateReadingFST();
static void cbForStateReadingFST(u32 intType);
static void cbForStateError(u32 intType);
static void stateError(u32 error);
static void stateTimeout();
static void stateSecurityError(void);
static void stateGettingError();
static u32 CategorizeError(u32 error);
static BOOL CheckCancel(u32 resume);
static void cbForStateGettingError(u32 intType);
static void cbForUnrecoveredError(u32 intType);
static void cbForUnrecoveredErrorRetry(u32 intType);
static void stateGoToRetry();
static void cbForStateGoToRetry(u32 intType);
static void stateCheckID();
static void stateCheckID3();
static void stateCheckID2a();
static void stateCheckID2(DVDCommandBlock* block);
static void cbForStateCheckID1(u32 intType);
static void cbForStateCheckID2(u32 intType);
static void cbForStateCheckID3(u32 intType);
static void cbForStateCheckID2a(u32 intType);
static void AlarmHandler(OSAlarm* alarm, OSContext* context);
static void stateCoverClosed();
static void stateCoverClosed_CMD(DVDCommandBlock* command);
static void cbForStateCoverClosed(u32 intType);
static void stateMotorStopped();
static void cbForStateMotorStopped(u32 intType);
static void stateReady();
static void stateBusy(DVDCommandBlock* block);
static BOOL IsImmCommandWithResult(u32 command);
static int IsDmaCommand(u32 command);
static void cbForStateBusy(u32 intType);
static int issueCommand(s32 prio, DVDCommandBlock* block);
static void cbForChangeDiskSync(s32 result, DVDCommandBlock* block);
static void cbForStopMotorSync(s32 result, DVDCommandBlock* block);
static void cbForInquirySync(s32 result, DVDCommandBlock* block);
static void cbForCancelSync(s32 result, DVDCommandBlock* block);
static void cbForCancelAllSync(s32 result, DVDCommandBlock* block);
static void cbForStateOpenPartition(u32 intType);
static void cbForStateReset(u32 intType);
static void cbForStateDownRotation(u32 intType);

DECL_WEAK void StampCommand(u32 command, u32 offset, u32 length) {
    BOOL enabled = OSDisableInterrupts();

    if (CommandInfoCounter >= 5) {
        CommandInfoCounter = 0;
    }

    __ErrorInfo.lastCommand[CommandInfoCounter].command = command;
    __ErrorInfo.lastCommand[CommandInfoCounter].offset = offset;
    __ErrorInfo.lastCommand[CommandInfoCounter].length = length;
    __ErrorInfo.lastCommand[CommandInfoCounter].tick = OSGetTick();
    CommandInfoCounter++;
    OSRestoreInterrupts(enabled);
} // 0x8021F900

inline void StampIntType(u32 intType) {
    BOOL enabled = OSDisableInterrupts();

    if (CommandInfoCounter == 0) {
        __ErrorInfo.lastCommand[4].intType = intType;
    } else {
        __ErrorInfo.lastCommand[CommandInfoCounter - 1].intType = intType;
    }

    OSRestoreInterrupts(enabled);
}

static void defaultOptionalCommandChecker(DVDCommandBlock*, DVDCommandCheckerCallback) {} // 0x8021F9C0

DVDCommandChecker __DVDSetOptionalCommandChecker(DVDCommandChecker func) {
    checkOptionalCommand = func;
    return checkOptionalCommand;
}

void DVDInit(void) {
    DVDDiskID* id;
    s32 rv;

    if (!DVDInitialized) {
        OSRegisterVersion(__DVDVersion);
        DVDInitialized = TRUE;
        DVDLowInit();

        if (!__OSInIPL && __OSLockedFlag == 0x80) {
            rv = ESP_InitLib();

            if (rv == 0) {
                rv = ESP_DiGetTicketView(NULL, (ESTicketView*)__DVDTicketViewBuffer);
            }

            if (rv == 0) {
                rv = ESP_DiGetTmd(NULL, &__DVDNumTmdBytes);
            }

            if (rv == 0) {
                rv = ESP_DiGetTmd((ESTitleMeta*)__DVDTmdBuffer, &__DVDNumTmdBytes);
            }

            ESP_CloseLib();
        }

        __DVDFSInit();
        __DVDClearWaitingQueue();

        MotorState = 0;
        bootInfo = (void*)OSPhysicalToCached(0);
        IDShouldBe = &bootInfo->DVDDiskID;

        OSInitThreadQueue(&__DVDThreadQueue);

        DVDLowUnmaskStatusInterrupts();
        DVDLowMaskCoverInterrupt();

        if (bootInfo->magic != 0xE5207C22 && bootInfo->magic != 0x0D15EA5E) {
            FirstTimeInBootrom = TRUE;
        }

        memset(&__ErrorInfo, 0, sizeof(DVDErrorInfo));
        id = (DVDDiskID*)OSPhysicalToCached(0);
        memcpy(__ErrorInfo.gameName, id->gameName, 4);
        __ErrorInfo.diskNumber = id->diskNumber;
        __ErrorInfo.gameVersion = id->gameVersion;
        __DVDLayoutFormat = 0;
        DVDSetAutoFatalMessaging(TRUE);
    }
} // 0x8021F9D0

static void stateReadingFST() {
    LastState = (stateFunc)stateReadingFST;
    DVD_ASSERTMSGLINE(1090, bootInfo->FSTMaxLength >= BB2.FSTLength, "DVDChangeDisk(): FST in the new disc is too big.   ");
    DVDLowClearCoverInterrupt(NULL);
    StampCommand(1, BB2.FSTPosition >> __DVDLayoutFormat, OSRoundUp32B(BB2.FSTLength << (~__DVDLayoutFormat & 2)));
    DVDLowRead(bootInfo->FSTLocation, OSRoundUp32B(BB2.FSTLength << (~__DVDLayoutFormat & 2)), BB2.FSTPosition >> __DVDLayoutFormat, cbForStateReadingFST);
} // 0x8021FB30

static u32 DmaCommand[1] = {0xFFFFFFFF};

static void cbForStateReadingFST(u32 intType) {
    DVDCommandBlock* finished;
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & DVD_INTTYPE_TC) {
        NumInternalRetry = 0;

        __DVDFSInit();
        finished = executing;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_END;

        if (finished->callback) {
            finished->callback(0, finished);
        }

        stateReady();
        return;
    }

    stateGettingError();
} // 0x8021FCB0

static void FatalAlarmHandler(OSAlarm* alarm, OSContext* context) {
    __DVDPrintFatalMessage();
} // 0x8021FE40

static void cbForStateError(u32 intType) {
    DVDCommandBlock* finished;

    if (__DVDGetAutoFatalMessaging()) {
        OSCreateAlarm(&FatalAlarm);
        OSSetAlarm(&FatalAlarm, 1, FatalAlarmHandler);
    } else {
        executing->state = -1;

        if (intType == 16) {
            stateTimeout();
            return;
        }

        if (intType == 32) {
            stateSecurityError();
            return;
        }

        FatalErrorFlag = TRUE;
        finished = executing;
        executing = &DummyCommandBlock;

        if (finished->callback) {
            (finished->callback)(-1, finished);
        }

        if (Canceling) {
            Canceling = FALSE;
            if (CancelCallback)
                (CancelCallback)(0, finished);
        }

        stateReady();
    }
} // 0x8021FE50

static void cbForStoreErrorCode1(s32 result, DVDCommandBlock* block) {
    DVDLowStopMotor(FALSE, FALSE, cbForStateError);
} // 0x8021FF60

static void stateError(u32 error) {
    __DVDStoreErrorCode(error, cbForStoreErrorCode1);
}

static void cbForStoreErrorCode2(s32 result, DVDCommandBlock* block) {
    DVDLowSetSpinupFlag(0);
    DVDLowReset(cbForStateError);
    ResetRequired = FALSE;
    ResumeFromHere = 0;
} // 0x8021FF80

static void stateTimeout() {
    __DVDStoreErrorCode(0x01234568, cbForStoreErrorCode2);
}

static void stateSecurityError(void) {
    __DVDStoreErrorCode(0x1234569, cbForStoreErrorCode2);
}

static void stateGettingError() {
    StampCommand(39, 0, 0);
    DVDLowRequestError(cbForStateGettingError);
}

static u32 CategorizeError(u32 error) {
    if (error == 0x20400) {
        LastError = error;
        return 1;
    }
    
    error &= 0x00FFFFFF;
    if (error == 0x62800 || error == 0x23A00 || error == 0x53000 || error == 0xB5A01) {
        return 0;
    }

    if (error == 0x52000 && (executing->command == 37 || LastState == stateDownRotation)) {
        return 0;
    }

    NumInternalRetry++;
    if (NumInternalRetry == 2) {
        if (error == LastError) {
            LastError = error;
            return 1;
        }
        LastError = error;
        return 2;
    }

    LastError = error;

    if (error == 0x31100 || executing->command == DVD_COMMAND_READID) {
        return 2;
    }

    return 3;
} // 0x8021FFC0

static BOOL CheckCancel(u32 resume) {
    DVDCommandBlock* finished;

    if (Canceling) {
        ResumeFromHere = resume;
        Canceling = FALSE;

        finished = executing;
        executing = &DummyCommandBlock;

        finished->state = 10;

        if (finished->callback)
            (*finished->callback)(-3, finished);

        if (CancelCallback)
            (CancelCallback)(0, finished);

        stateReady();
        return TRUE;
    }

    return FALSE;
}

static void cbForStoreErrorCode3(s32 result, DVDCommandBlock* block) {
    stateGoToRetry();
} // 0x802200C0

static void cbForStateGettingError(u32 intType) {
    u32 error;
    u32 status;
    u32 errorCategory;
    u32 resume;

    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if (intType & 2) {
        stateError(0x1234567);
        return;
    }


    error = DVDLowGetImmBufferReg();
    status = error & 0xff000000;

    errorCategory = CategorizeError(error);

    if (errorCategory == 1) {
        stateError(error);
        return;
    }

    if (errorCategory == 2 || errorCategory == 3) {
        resume = 0;
    } else {
        if (status == 0x1000000) {
            resume = 4;
        } else if (status == 0x2000000) {
            resume = 6;
        } else if (status == 0x3000000) {
            resume = 3;
        } else if (status == 0) {
            if (error == 0x53000) {
                resume = 1;
            }
            else {
                resume = 5;
            }
        } else {
            resume = 5;
        }
    }

    if (CheckCancel(resume))
        return;

    if (errorCategory == 2) {
        __DVDStoreErrorCode(error, cbForStoreErrorCode3);
        return;
    }

    if (errorCategory == 3) {
        if ((error & 0x00ffffff) == 0x00031100) {
            StampCommand(2, executing->offset, 0);
            DVDLowSeek(executing->offset, cbForUnrecoveredError);
        } else {
            LastState(executing);
        }
        return;
    }

    if (status == 0x01000000) {
        executing->state = 5;
        stateMotorStopped();
        return;
    } else if (status == 0x02000000) {
        executing->state = 3;
        stateCoverClosed();
        return;
    } else if (status == 0x03000000) {
        executing->state = 4;
        stateMotorStopped();
        return;
    } else if (status == 0) {
        if (error == 0x53000) {
            StampCommand(16, 0, 0);
            DVDLowStopMotor(FALSE, FALSE, cbForStateCheckID1);
            return;
        } else {
            stateError(0x1234567);
            return;
        }
    } else {
        stateError(0x1234567);
        return;
    }
} // 0x80220190

static void cbForUnrecoveredError(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if (intType & 1) {
        stateGoToRetry();
        return;
    }

    StampCommand(39, 0, 0);
    DVDLowRequestError(cbForUnrecoveredErrorRetry);
} // 0x802206A0

static void cbForUnrecoveredErrorRetry(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if (intType & 2) {
        stateError(0x1234567);
    } else {
        stateError(DVDLowGetImmBufferReg());
    }
} // 0x80220880

static void stateGoToRetry() {
    StampCommand(16, 0, 0);
    DVDLowStopMotor(FALSE, FALSE, cbForStateGoToRetry);
}

static void cbForStateGoToRetry(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if (intType & 2) {
        stateError(0x1234567);
        return;
    }

    NumInternalRetry = 0;

    if (CurrCommand == 4 || CurrCommand == 5 || CurrCommand == 13 || CurrCommand == 33 || CurrCommand == 34 || CurrCommand == 41 || CurrCommand == 42 || CurrCommand == 15 || CurrCommand == 37) {
        ResetRequired = TRUE;
    }

    if (!CheckCancel(2)) {
        executing->state = 11;
        stateMotorStopped();
    }
} // 0x80220960

static void stateCheckID() {
    switch(CurrCommand) {
    case DVD_COMMAND_CHANGE_DISK:
        ChangedDisc = FALSE;

        if (DVDCompareDiskID(&CurrDiskID, executing->id)) {
            memcpy(IDShouldBe, &CurrDiskID, sizeof(DVDDiskID));
            executing->state = DVD_STATE_BUSY;
            DCInvalidateRange(&BB2.bootFilePosition, 0x20);
            NumInternalRetry = 0;
            stateReadingTOC(executing);
        } else {
            StampCommand(16, 0, 0);
            DVDLowStopMotor(FALSE, FALSE, cbForStateCheckID1);
        }
        break;
    default:
        if (memcmp(&CurrDiskID, IDShouldBe, sizeof(DVDDiskID)) != 0) {
            StampCommand(16, 0, 0);
            DVDLowStopMotor(FALSE, FALSE, cbForStateCheckID1);
        } else {
            NumInternalRetry = 0;
            stateReadingTOC(executing);
        }
        break;
    }
} // 0x80220BA0


static ESTitleMeta* Tmd;

static void cbForStateReadingTOC(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        NumInternalRetry = 0;
        GameToc = (DVDGameTOC*)__DVDGameTocBuffer;
        stateReadingPartitionInfo(executing);
    } else {
        stateGettingError();
    }
} // 0x80220EE0

static void stateReadingTOC(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(NULL);
    StampCommand(33, 0x40000 >> 2, OSRoundUp32B(sizeof(DVDGameTOC)));
    DVDLowUnencryptedRead(__DVDGameTocBuffer, OSRoundUp32B(sizeof(DVDGameTOC)), 0x10000, cbForStateReadingTOC);
}

static void cbForStateReadingPartitionInfo(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        s16 i;
        NumInternalRetry = 0;
        PartInfo = (DVDPartitionInfo*)__DVDPartInfoBuffer;
        BootGameInfo = NULL;

        if (__OSLaunchPartitionOffset != 0) {
            BootGameInfo = PartInfo;
            BootGameInfo->type = __OSLaunchPartitionType;
            BootGameInfo->gamePartition = (DVDGamePartition*)__OSLaunchPartitionOffset;
        } else {
            for (i = 0; i < GameToc->numGamePartitions; i++) {
                if (PartInfo->type == __OSLaunchPartitionType) {
                    BootGameInfo = PartInfo;
                }

                PartInfo++;
            }
        }

        if (BootGameInfo) {
            switch (CurrCommand) {
            case 3:
                NumInternalRetry = 0;
                stateOpenPartition(executing);
                break;

            default:
                NumInternalRetry = 0;
                stateOpenPartition2(executing);
                break;
            }
        } else {
            if (!CheckCancel(1)) {
                executing->state = 6;
                stateMotorStopped();
            }
        }
    } else {
        stateGettingError();
    }
} // 0x80221080

static void stateReadingPartitionInfo(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(0);
    StampCommand(33, (0x40000 + OSRoundUp32B(sizeof(DVDGameTOC))) >> 2, OSRoundUp32B(sizeof(DVDPartitionInfo)));
    DVDLowUnencryptedRead(__DVDPartInfoBuffer, OSRoundUp32B(sizeof(DVDPartitionInfo)), (0x40000 + OSRoundUp32B(sizeof(DVDGameTOC))) >> 2, cbForStateReadingPartitionInfo);
}

static void cbForStateOpenPartition(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        NumInternalRetry = 0;
        stateCheckID2(executing);
    } else {
        stateGettingError();
    }
} // 0x80221500

static void stateOpenPartition(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(0);
    StampCommand(34, (u32)BootGameInfo->gamePartition, 0);

    if (__OSLockedFlag == 0x80) {
        DVDLowOpenPartitionWithTmdAndTicketView((u32)BootGameInfo->gamePartition, (ESTicketView*)__DVDTicketViewBuffer, __DVDNumTmdBytes, (ESTitleMeta*)__DVDTmdBuffer, 0, NULL, cbForStateOpenPartition);
    } else {
        DVDLowOpenPartition((u32)BootGameInfo->gamePartition, NULL, 0, 0, (ESTitleMeta*)__DVDTmdBuffer, cbForStateOpenPartition);
    }
}

static void cbForStateOpenPartition2(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        NumInternalRetry = 0;

        if (!CheckCancel(0)) {
            executing->state = 1;
            stateBusy(executing);
        }
    } else {
        stateGettingError();
    }
} // 0x80221690

static void stateOpenPartition2(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(0);
    StampCommand(34, (u32)BootGameInfo->gamePartition, 0);

    if (__OSLockedFlag == 0x80) {
        DVDLowOpenPartitionWithTmdAndTicketView((u32)BootGameInfo->gamePartition, (ESTicketView*)__DVDTicketViewBuffer, __DVDNumTmdBytes, (ESTitleMeta*)__DVDTmdBuffer, 0, NULL, cbForStateOpenPartition2);
    } else {
        DVDLowOpenPartition((u32)BootGameInfo->gamePartition, NULL, 0, 0, (ESTitleMeta*)__DVDTmdBuffer, cbForStateOpenPartition2);
    }
}

static void stateCheckID2(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(0);
    StampCommand(1, (u32)(0x420 >> 2), OSRoundUp32B(sizeof(DVDBB2)));
    DVDLowRead(&BB2, OSRoundDown32B(sizeof(DVDBB2)), (u32)(0x420 >> 2), cbForStateCheckID2);
}

static void stateCheckID3() {
    DVDLowAudioBufferConfig(IDShouldBe->streaming, 0xA, cbForStateCheckID3);
}

static void stateCheckID2a() {
    DVDLowAudioBufferConfig(IDShouldBe->streaming, 0xA, cbForStateCheckID2a);
}

static void cbForStateCheckID2a(u32 intType) {
    if (intType == 16) {
        stateTimeout();
        return;
    }


    if (intType & DVD_INTTYPE_TC) {
        NumInternalRetry = 0;
        stateCheckID2(executing);
        return;
    }

    stateGettingError();
}

static void cbForStateCheckID1(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if (intType & DVD_INTTYPE_DE) {
        stateError(0x01234567);
        return;
    }

    NumInternalRetry = 0;

    if (CheckCancel(1) == FALSE) {
        executing->state = DVD_STATE_WRONG_DISK;
        stateMotorStopped();
    }
} // 0x80221800

static void cbForStateCheckID2(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
    } else if (intType == 32) {
        stateSecurityError();
    } else if (intType & DVD_INTTYPE_TC) {
        NumInternalRetry = 0;
        stateReadingFST();
    } else {
        stateGettingError();
    }
} // 0x802219D0

static void cbForStateCheckID3(u32 intType) {
    if (intType == 16) {
        stateTimeout();
        return;
    }


    if (intType & DVD_INTTYPE_TC) {
        NumInternalRetry = 0;
        if (CheckCancel(0) == FALSE) {
            executing->state = DVD_STATE_BUSY;
            stateBusy(executing);
        }
        return;
    }

    stateGettingError();
}

// No retail address: nothing in this build sets the alarm, so the linker dead-strips it.
static void AlarmHandler(OSAlarm* alarm, OSContext* context) {
    DVDReset();
    DCInvalidateRange(&CurrDiskID, sizeof(DVDDiskID));
    LastState = &stateCoverClosed_CMD;
    stateCoverClosed_CMD(executing);
}

static void stateCoverClosed() {
    DVDCommandBlock* finished;
    MotorState = 1;

    switch(CurrCommand) {
    case DVD_COMMAND_BSREAD:
    case DVD_COMMAND_READID:
    case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
    case DVD_COMMAND_BS_CHANGE_DISK:
    case 33:
    case 34:
    case 37:
    case 41:
    case 42:
        __DVDClearWaitingQueue();
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback) {
            finished->callback(-4, finished);
        }
        stateReady();
        break;
    case 32:
        MotorState = 0;
    case 35:
    case 38:
    case 36:
        executing->state = 1;
        stateBusy(executing);
        break;
    case 1:
    case 2:
        if (__OSInIPL) {
            break;
        }
    default:
        MotorState = 0;
        DVDLowSetSpinupFlag(1);
        DVDLowReset(cbForStateReset);
        break;
    }
} // 0x80221B70

static void ResetAlarmHandler(OSAlarm* alarm, OSContext* context) {
    if (__OSDeviceCode == (u16)(0x8000 | 0x003)) {
        LastState = stateDownRotation;
        stateDownRotation(executing);
    } else {
        DCInvalidateRange(&CurrDiskID, sizeof(DVDDiskID));
        LastState = stateCoverClosed_CMD;
        stateCoverClosed_CMD(executing);
    }
} // 0x80221C40

static void cbForStateReset(u32 intType) {
    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        LastResetEnd = __OSGetSystemTime();
        ResetRequired = FALSE;
        ResumeFromHere = 0;
        OSCreateAlarm(&ResetAlarm);
        OSSetAlarm(&ResetAlarm, DVD_RESETCOVER_TIMELAG_TICKS2, ResetAlarmHandler);
    } else {
        stateGettingError();
    }
} // 0x80221E90

static void stateDownRotation(DVDCommandBlock* block) {
    DVDLowClearCoverInterrupt(0);
    StampCommand(37, 0, 0);
    DVDLowSetMaximumRotation(0x20000, cbForStateDownRotation);
} // 0x80221F80

static void cbForStateDownRotation(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & 1) {
        DCInvalidateRange(&CurrDiskID, sizeof(DVDDiskID));
        LastState = stateCoverClosed_CMD;
        stateCoverClosed_CMD(executing);
    } else {
        stateGettingError();
    }
} // 0x80222050

static void stateCoverClosed_CMD(DVDCommandBlock* command) {
    if (CurrCommand == 40) {
        NumInternalRetry = 0;

        if (!CheckCancel(0)) {
            executing->state = 1;
            stateBusy(executing);
        }
    } else {
        DVDLowClearCoverInterrupt(0);
        StampCommand(5, 0, sizeof(DVDDiskID));
        DVDLowReadDiskID(&CurrDiskID, cbForStateCoverClosed);
    }
} // 0x80222290

static void cbForStateCoverClosed(u32 intType) {
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }


    if (intType & DVD_INTTYPE_TC) {
        NumInternalRetry = 0;
        stateCheckID();
        return;
    }

    stateGettingError();
} // 0x80222410

static void cbForPrepareCoverRegister(u32 intType) {
    PreparingCover = FALSE;

    if (WaitingForCoverClose) {
        if (!(DVDLowGetCoverRegister() & 1)) {
            OSCancelAlarm(&CoverAlarm);
            WaitingForCoverClose = FALSE;
            cbForStateMotorStopped(4);
        }
        return;
    }

    if (DVDLowGetCoverRegister() & 1) {
        WaitingForCoverOpen = FALSE;
        WaitingForCoverClose = TRUE;

        if (MotorState == 2) {
            executing->state = 12;
        }
        else {
            executing->state = 5;
        }
    } else if (DVDLowGetCoverRegister() & 4) {
        OSCancelAlarm(&CoverAlarm);
        WaitingForCoverOpen = FALSE;
        DVDLowClearCoverInterrupt(0);
        cbForStateMotorStopped(4);
    }
} // 0x802224F0

static void CoverAlarmHandler(OSAlarm* alarm, OSContext* context) {
    if (!PreparingCover) {
        PreparingCover = TRUE;
        DVDLowPrepareCoverRegister(cbForPrepareCoverRegister);
    }
} // 0x80222680

static void stateMotorStopped() {
    MotorState = 1;

    if (WaitingForCoverOpen || WaitingForCoverClose) {
        return;
    }

    WaitingForCoverOpen = TRUE;
    OSCreateAlarm(&CoverAlarm);
    OSSetPeriodicAlarm(&CoverAlarm, OSGetTick(), OSMillisecondsToTicks(100), CoverAlarmHandler);
}

static void cbForStateMotorStopped(u32 intType) {
    WaitingForCoverClose = FALSE;

    if (CurrCommand == 3) {
        ChangedDisc = TRUE;
    }

    if (MotorState == 2) {
        if (executing) {
            executing->state = 12;
        }

        return;
    }

    DVDLowMaskCoverInterrupt();

    if (executing) {
        executing->state = 3;
        stateCoverClosed();
    } else {
        ResumeFromHere = 7;
    }
}

static void stateReady() {
    DVDCommandBlock* finished;

    if (PauseFlag != 0) {
        PausingFlag = 1;
        executing = NULL;
        return;
    }

    if (__DVDCheckWaitingQueue() == 0) {
        executing = NULL;
        return;
    }

    executing = __DVDPopWaitingQueue();

    if (FatalErrorFlag) {
        executing->state = DVD_STATE_FATAL_ERROR;
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback) {
            (*finished->callback)(-1, finished);
        }

        stateReady();
        return;
    }

    CurrCommand = executing->command;
    if (CurrCommand == 32 || CurrCommand == 14 || CurrCommand == 35) {
        ResumeFromHere = 0;
    }

    if (ResumeFromHere != 0) {
        switch (ResumeFromHere) {
        case 2:
            executing->state = DVD_STATE_RETRY;
            stateMotorStopped();
            break;
        case 3:
            executing->state = DVD_STATE_NO_DISK;
            stateMotorStopped();
            break;
        case 4:
            executing->state = DVD_STATE_COVER_OPEN;
            stateMotorStopped();
            break;
        case 1:
        case 6:
        case 7:
            executing->state = DVD_STATE_COVER_CLOSED;
            stateCoverClosed();
            break;
        case 5:
            stateError(CancelLastError);
            break;
        }

        ResumeFromHere = 0;
        return;
    }

    switch (MotorState) {
    case 2:
        if (MotorStopped) {
            executing->state = 12;
        }
        else {
            executing->state = 3;
            stateCoverClosed();
        }
        break;
    case 0:
        executing->state = 1;
        stateBusy(executing);
        break;
    case 1:
    default:
        stateCoverClosed();
        break;
    }
} // 0x802226B0

static void stateBusy(DVDCommandBlock* block) {
    DVDCommandBlock* finished;
    LastState = stateBusy;

    switch (block->command) {
        case 5:
        case 2:
        case 3:
        case 15:
        case 14:
        case 13:
        case 16:
        case 34:
        case 37:
        case 41:
        case 42:
            StampCommand(block->command, block->offset, block->length);
            break;
        default:
            break;
    }

    switch (block->command) {
    case 5:
        DVDLowClearCoverInterrupt(0);
        block->currTransferSize = sizeof(DVDDiskID);
        DVDLowReadDiskID(block->addr, cbForStateBusy);
        break;
    case 1:
    case 4:
        if (block->length == 0) {
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = 0;

            if (finished->callback) {
                (finished->callback)(0, finished);
            }

            stateReady();
        } else {
            DVDLowClearCoverInterrupt(0);
            block->currTransferSize = (block->length - block->transferredSize > 0x80000) ? 0x80000 : (block->length - block->transferredSize);
            StampCommand(block->command, ((block->offset) + (block->transferredSize >> 2)), block->currTransferSize);
            DVDLowRead((void*)((u8*)block->addr + block->transferredSize), block->currTransferSize, ((block->offset) + (block->transferredSize >> 2)), cbForStateBusy);
        }
        break;
    case 2:
        DVDLowClearCoverInterrupt(0);
        DVDLowSeek(block->offset, cbForStateBusy);
        break;
    case 3:
        DVDLowStopMotor(FALSE, FALSE, cbForStateBusy);
        break;
    case 15:
        DVDLowStopMotor(FALSE, FALSE, cbForStateBusy);
        break;
    case 13:
        DVDLowClearCoverInterrupt(0);
        DVDLowAudioBufferConfig(block->offset, block->length, cbForStateBusy);
        break;
    case 14:
        DVDLowClearCoverInterrupt(0);
        block->currTransferSize = sizeof(DVDDriveInfo);
        DVDLowInquiry(block->addr, cbForStateBusy);
        break;
    case 16:
        DVDLowClearCoverInterrupt(0);
        DVDLowStopMotor(FALSE, FALSE, cbForStateBusy);
        break;
    case 32:
        DVDLowSetSpinupFlag(1);
        DVDLowReset(cbForStateBusy);
        break;
    case 33:
        if (block->length == 0) {
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = 0;
            
            if (finished->callback) {
                (finished->callback)(0, finished);
            }

            stateReady();
        } else {
            DVDLowClearCoverInterrupt(0);
            block->currTransferSize = (block->length - block->transferredSize > 0x80000) ? 0x80000 : (block->length - block->transferredSize);
            StampCommand(block->command, ((block->offset) + (block->transferredSize >> 2)), block->currTransferSize);
            DVDLowUnencryptedRead((void*)((u8*)block->addr + block->transferredSize), block->currTransferSize, ((block->offset) + (block->transferredSize >> 2)), cbForStateBusy);
        }
        break;
    case 34:
        DVDLowClearCoverInterrupt(0);
        DVDLowOpenPartition(block->offset, NULL, 0, 0, (ESTitleMeta*)block->addr, cbForStateBusy);
        break;
    case 35:
        DVDLowClearCoverInterrupt(0);
        DVDLowClosePartition(cbForStateBusy);
        break;
    case 38:
        DVDLowPrepareCoverRegister(cbForStateBusy);
        break;
    case 36:
        DVDLowPrepareCoverRegister(cbForStateBusy);
        break;
    case 37:
        DVDLowClearCoverInterrupt(0);
        DVDLowSetMaximumRotation(0x20000, cbForStateBusy);
        break;
    case 40:
        DVDLowClearCoverInterrupt(0);
        block->addr = &CurrDiskID;
        block->currTransferSize = sizeof(DVDDiskID);
        DVDLowReadDiskID(block->addr, cbForStateBusy);
        break;
    case 41:
    {
        DVDPartitionParams* params;
        DVDLowClearCoverInterrupt(0);
        params = block->addr;

        if (!params->numTmdBytes && !params->numCertBytes) {
            DVDLowGetNoDiscBufferSizes(block->offset, &params->numTmdBytes, &params->numCertBytes, cbForStateBusy);
        } else {
            DVDLowGetNoDiscOpenPartitionParams(block->offset, &params->ticket, &params->numTmdBytes, 
                                                &params->tmd, &params->numCertBytes, params->certificates, 
                                                &params->dataWordOffset, params->h3Hash, cbForStateBusy);
        }
        break;
    }
    case 42:
    {
        DVDPartitionParams* params;
        DVDLowClearCoverInterrupt(0);
        params = block->addr;
        DVDLowOpenPartitionWithTmdAndTicketView(block->offset, &params->ticketView, params->numTmdBytes, &params->tmd,
                                                params->numCertBytes, params->certificates, cbForStateBusy);
        break;
    }
    default:
        checkOptionalCommand(block, cbForStateBusy);
        break;
    }
} // 0x802229D0

static u32 ImmCommand[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

void __DVDSetImmCommand(u32 command) {
    static u32 immCount;
    ImmCommand[immCount++] = command;
}

void __DVDSetDmaCommand(u32 command) {
    static u32 dmaCount;
    DmaCommand[dmaCount++] = command;
}

static BOOL IsImmCommandWithResult(u32 command) {
    u32 i;

    if (command == 9 || command == 10 || command == 11 || command == 12) {
        return 1;
    }

    for (i = 0; i < 3; i++) {
        if (command == ImmCommand[i]) {
            return TRUE;
        }
    }

    return FALSE;
}

static int IsDmaCommand(u32 command) {
    u32 i;

    if (command == 1 || command == 4 || command == 5 || command == 33 || command == 14) {
        return 1;
    }

    for (i = 0; i < 1; i++) {
        if (command == DmaCommand[i]) {
            return TRUE;
        }
    }

    return FALSE;
}

static void cbForStateBusy(u32 intType) {
    DVDCommandBlock* finished;
    
    StampIntType(intType);

    if (intType == 16) {
        stateTimeout();
        return;
    }

    if (intType == 32) {
        stateSecurityError();
        return;
    }

    if ((CurrCommand == DVD_COMMAND_CHANGE_DISK) || (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK)) {
        if (intType & DVD_INTTYPE_DE) {
            stateError(0x01234567);
            return;
        }

        NumInternalRetry = 0;

        if (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK) {
            ResetRequired = 1;
        }

        if (CheckCancel(7)) {
            return;
        }

        if (MotorState != 2) {
            executing->state = 7;
            stateMotorStopped();
        }
        return;
    }


    if (IsDmaCommand(CurrCommand)) {
        executing->transferredSize += (intType & (8 | 1)) ? executing->currTransferSize : 0;
    }

    if (Breaking) {
        Breaking = FALSE;
        Canceling = 0;
        finished = executing;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_CANCELED;

        if (finished->callback) {
            finished->callback(-3, finished);
        }

        if (CancelCallback) {
            CancelCallback(0, finished);
        }

        stateReady();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;

        if (CurrCommand == 0x10) {
            if (executing->offset) {
                MotorState = 2;
            } else {
                MotorState = 1;
            }

            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = 0;

            if (finished->callback != 0) {
                (*finished->callback)(0, finished);
            }

            stateReady();
            return;
        }

        if (CurrCommand == 32) {
            LastResetEnd = __OSGetSystemTime();

            ResetRequired = FALSE;
            ResumeFromHere = 0;

            finished = executing;
            executing = &DummyCommandBlock;

            finished->state = 0;
            if (finished->callback) {
                (finished->callback)(0, finished);
            }

            stateReady();
            return;
        }

        if (CheckCancel(0) != FALSE) {
            return;
        }

        if (CurrCommand == 38) {
            s32 retVal;
            u32 coverReg;

            coverReg = DVDLowGetCoverRegister();

            if (__OSGetSystemTime() - LastResetEnd < DVD_RESETCOVER_TIMELAG_TICKS2) {
                retVal = 0;
            } else if (coverReg & 1) {
                retVal = 1;
            } else {
                retVal = 2;
            }

            finished = executing;
            executing = &DummyCommandBlock;

            finished->state  = 0;
            finished->offset = (u32)retVal;
            if (finished->callback) {
                (finished->callback)(retVal, finished);
            }

            stateReady();
            return;
        }

        if (CurrCommand == 36) {
            s32 retVal;
            u32 coverReg;

            coverReg = DVDLowGetCoverRegister();
            if ((((u32)(coverReg) & 0x00000004) >> 2) || (coverReg & 1)) {
                retVal = FALSE;
            } else {
                if (ResumeFromHere != 0) {
                    retVal = FALSE;
                }
                else {
                    retVal = TRUE;
                }
            }

            finished = executing;
            executing = &DummyCommandBlock;

            finished->state  = 0;
            finished->offset = (u32)retVal;

            if (finished->callback) {
                (finished->callback)(retVal, finished);
            }

            stateReady();
            return;
        }

        if (CurrCommand == 40) {
            if (DVDCompareDiskID(&CurrDiskID, executing->id)) {
                memcpy(IDShouldBe, &CurrDiskID, sizeof(DVDDiskID));
                finished = executing;
                executing = &DummyCommandBlock;

                finished->state  = 0;
                finished->offset = (u32)TRUE;
                if (finished->callback) {
                    (finished->callback)(TRUE, finished);
                }

                NumInternalRetry = 0;

                stateReady();
                return;
            } else {
                StampCommand(16, 0, 0);
                DVDLowStopMotor(FALSE, FALSE, cbForStateCheckID1);
                return;
            }
        }

        if (CurrCommand == 41) {
            DVDPartitionParams* params;

            params = (DVDPartitionParams*)(executing->addr);
            if (!params->dataWordOffset) {
                stateBusy(executing);
                return;
            }

            finished = executing;
            executing = &DummyCommandBlock;

            finished->state  = 0;
            if (finished->callback) {
                (finished->callback)(0, finished);
            }

            stateReady();
            return;
        }

        if (IsDmaCommand(CurrCommand)) {
            if (executing->transferredSize != executing->length) {
                stateBusy(executing);
                return;
            }

            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(finished->transferredSize, finished);
            }

            stateReady();
            return;
        } else if (IsImmCommandWithResult(CurrCommand)) {
            s32 result;
            if (CurrCommand == DVD_COMMAND_REQUEST_START_ADDR || CurrCommand == DVD_COMMAND_REQUEST_PLAY_ADDR) {
                result = (s32)(DVDLowGetImmBufferReg() << 2);
            } else {
                result = (s32)DVDLowGetImmBufferReg();
            }

            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(result, finished);
            }

            stateReady();
            return;
        } else {
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(0, finished);
            }

            stateReady();
            return;
        }
    } else {

        if (CurrCommand == 14) {
            stateError(0x01234567);
            return;
        }

        if ((CurrCommand == 1 || CurrCommand == 4 || CurrCommand == 5 || CurrCommand == 33 || CurrCommand == 14)
            && (executing->transferredSize == executing->length)) {

            if (CheckCancel(0)) {
                return;
            }
            finished  = executing;
            executing = &DummyCommandBlock;

            finished->state = DVD_STATE_END;
            if (finished->callback) {
                (finished->callback)((s32)finished->transferredSize, finished);
            }
            stateReady();
            return;
        }

        stateGettingError();
    }
} // 0x80222F80

void* __DVDGetIssueCommandAddr(void) {
    return issueCommand;
}

static int issueCommand(s32 prio, DVDCommandBlock* block) {
    BOOL level;
    int result;

    if (autoInvalidation != 0 && (block->command == DVD_COMMAND_READ || block->command == DVD_COMMAND_BSREAD 
        || block->command == DVD_COMMAND_READID || block->command == 33 || block->command == DVD_COMMAND_INQUIRY)) {
        DCInvalidateRange(block->addr, block->length);
    }

    level = OSDisableInterrupts();
#if DEBUG
    if (executing == block || (block->state == DVD_STATE_WAITING && __DVDIsBlockInWaitingQueue(block))) {
        OSPanic(__FILE__, 3764, "DVD library: Specified command block (or file info) is already in use\n");
    }
#endif

    block->state = DVD_STATE_WAITING;
    result = __DVDPushWaitingQueue(prio, block);
    if (executing == NULL && PauseFlag == 0) {
        stateReady();
    }

    OSRestoreInterrupts(level);
    return result;
}

int DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback, s32 prio) {
    int idle;

    
    block->command = DVD_COMMAND_READ;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(prio, block);
    return idle;
} // 0x80223960

int DVDSeekAbsAsyncPrio(DVDCommandBlock* block, s32 offset, DVDCBCallback callback, s32 prio) {
    int idle;

    
    block->command = DVD_COMMAND_SEEK;
    block->offset = offset;
    block->callback = callback;

    idle = issueCommand(prio, block);
    return idle;
}

int DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback) {
    int idle;

    
    block->command = DVD_COMMAND_BSREAD;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(2, block);
    return idle;
}

int DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback) {
    int idle;


    block->command = DVD_COMMAND_READID;
    block->addr = diskID;
    block->length = 0x20;
    block->offset = 0;
    block->transferredSize = 0;
    block->callback = callback;

    idle = issueCommand(2, block);
    return idle;
}

void __DVDAudioBufferConfig(DVDCommandBlock* block, u32 enable, u32 size, DVDCBCallback callback) {
    block->command = DVD_COMMAND_AUDIO_BUFFER_CONFIG;
    block->offset = enable;
    block->length = size;
    block->callback = callback;
    issueCommand(2, block);
}

int DVDChangeDiskAsyncForBS(DVDCommandBlock* block, DVDCBCallback callback) {
    int idle;

    
    block->command = DVD_COMMAND_BS_CHANGE_DISK;
    block->callback = callback;
    idle = issueCommand(2, block);
    return idle;
}

int DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* id, DVDCBCallback callback) {
    int idle;


    if (id->company[0] == 0) {
        OSReport("DVDChangeDiskAsync(): You can't specify NULL to company name.  \n");
        DVD_ASSERTMSGLINE(2902, 0, "");
    }

    block->command = DVD_COMMAND_CHANGE_DISK;
    block->id = id;
    block->callback = callback;
    DCInvalidateRange(bootInfo->FSTLocation, bootInfo->FSTMaxLength);
    
    idle = issueCommand(2, block);
    return idle;
}

s32 DVDChangeDisk(DVDCommandBlock* block, DVDDiskID* id) {
    int result;
    s32 state;
    BOOL enabled;
    s32 retVal;

    result = DVDChangeDiskAsync(block, id, cbForChangeDiskSync);
    if (result == 0) {
        return -1;
    }

    enabled = OSDisableInterrupts();
    while (1) {
        state = block->state;
        if (state == DVD_STATE_END) {
            retVal = 0;
            break;
        } else if (state == DVD_STATE_FATAL_ERROR) {
            retVal = -1;
            break;
        } else if (state == DVD_STATE_CANCELED) {
            retVal = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

static void cbForChangeDiskSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

int DVDStopMotorAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    int idle;

    block->command = DVD_COMMAND_UNK_16;
    block->callback = callback;
    
    idle = issueCommand(2, block);
    return idle;
}

s32 DVDStopMotor(DVDCommandBlock* block) {
    int result;
    s32 state;
    BOOL enabled;
    s32 retVal;

    result = DVDStopMotorAsync(block, cbForStopMotorSync);
    if (result == 0) {
        return -1;
    }

    enabled = OSDisableInterrupts();
    while (1) {
        state = block->state;
        if (state == DVD_STATE_END) {
            retVal = 0;
            break;
        } else if (state == DVD_STATE_FATAL_ERROR) {
            retVal = -1;
            break;
        } else if (state == DVD_STATE_CANCELED) {
            retVal = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

static void cbForStopMotorSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

int DVDInquiryAsync(DVDCommandBlock* block, DVDDriveInfo* info, DVDCBCallback callback) {
    int idle;


    block->command = DVD_COMMAND_INQUIRY;
    block->addr = info;
    block->length = 0x20;
    block->transferredSize = 0;
    block->callback = callback;
    idle = issueCommand(2, block);
    return idle;
} // 0x80223A50

s32 DVDInquiry(DVDCommandBlock* block, DVDDriveInfo* info) {
    int result;
    s32 state;
    BOOL enabled;
    s32 retVal;

    result = DVDInquiryAsync(block, info, cbForInquirySync);
    if (result == 0) {
        return -1;
    }

    enabled = OSDisableInterrupts();
    while (1) {
        state = block->state;
        if (state == DVD_STATE_END) {
            retVal = (u32)block->transferredSize;
            break;
        } else if (state == DVD_STATE_FATAL_ERROR) { 
            retVal = -1;
            break;
        } else if (state == DVD_STATE_CANCELED) {
            retVal = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

static void cbForInquirySync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

void DVDReset(void) {
    //DVDLowReset();
    __DIRegs[0] = 0x2A;
    __DIRegs[1] = __DIRegs[1]; // write the status bits back to acknowledge them
    ResetRequired = 0;
    ResumeFromHere = 0;
}

int DVDResetRequired(void) {
    return ResetRequired;
}

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) {
    BOOL enabled;
    s32 retVal;
    DVDCommandBlock* next;

    enabled = OSDisableInterrupts();

    if (((volatile DVDCommandBlock*)block)->state == 3) { // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
        retVal = 1;
    } else if (((volatile DVDCommandBlock*)block)->state == 5) { // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
        retVal = 4;
    } else if (executing == &__DVDStopMotorCommandBlock) {
        next = __DVDGetNextWaitingQueue();
        if (next) {
            if(block == next) {
                retVal = 1;
            } else {
                retVal = ((volatile DVDCommandBlock*)block)->state; // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
            }
        } else {
            if (block == &__DVDStopMotorCommandBlock) {
                retVal = 0;
            } else {
                retVal = ((volatile DVDCommandBlock*)block)->state; // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
            }
        }
    } else {
        retVal = ((volatile DVDCommandBlock*)block)->state; // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
    }

    OSRestoreInterrupts(enabled);
    return retVal;
} // 0x80223B30

s32 DVDGetDriveStatus(void) {
    BOOL enabled = OSDisableInterrupts();
    s32 retVal;
    
    if (FatalErrorFlag != FALSE) {
        retVal = DVD_STATE_FATAL_ERROR;
    } else {
        if (PausingFlag != FALSE) {
            retVal = DVD_STATE_PAUSING;
        } else {
            if (executing == NULL) {
                retVal = DVD_STATE_END;
            } else if (executing == &DummyCommandBlock) {
                retVal = DVD_STATE_END;
            } else {
                retVal = DVDGetCommandBlockStatus((DVDCommandBlock*)executing);
            }
        }
    }
    OSRestoreInterrupts(enabled);
    return retVal;
} // 0x80223C00

BOOL DVDSetAutoInvalidation(BOOL autoInval) {
    BOOL prev;

    prev = autoInvalidation;
    autoInvalidation = autoInval;
    return prev;
} // 0x80223CA0

void DVDPause(void) {
    BOOL level;

    level = OSDisableInterrupts();
    PauseFlag = 1;
    if (executing == NULL) {
        PausingFlag = 1;
    }
    OSRestoreInterrupts(level);
}

void DVDResume(void) {
    BOOL level;

    level = OSDisableInterrupts();
    PauseFlag = 0;
    if (PausingFlag != 0) {
        PausingFlag = 0;
        stateReady();
    }
    OSRestoreInterrupts(level);
} // 0x80223CB0

int DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    BOOL enabled;
    DVDCommandBlock* finished;

    enabled = OSDisableInterrupts();

    switch (block->state) {
    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_END:
    case DVD_STATE_CANCELED:
        if (callback)
            (*callback)(0, block);
        break;
    case DVD_STATE_BUSY:
        if (Canceling) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }

        Canceling = TRUE;
        CancelCallback = callback;
        if (block->command == 4 || block->command == 33 || block->command == 34 || block->command == 41 || block->command == 42 || block->command == 1) {
            __DVDLowBreak();
        }
        break;
    case DVD_STATE_WAITING:
        __DVDDequeueWaitingQueue(block);
        block->state = DVD_STATE_CANCELED;

        if (block->callback)
            (block->callback)(-3, block);

        if (callback)
            (*callback)(0, block);
        break;
    case DVD_STATE_COVER_CLOSED:
        switch (block->command) {
        case 5:
        case 4:
        case 13:
        case 15:
        case 33:
        case 34:
        case 37:
        case 41:
        case 42:
            if (callback)
                (*callback)(0, block);
            break;
        case 1:
        case 2:
            if (__OSInIPL) {
                finished = executing;
                executing = &DummyCommandBlock;
                block->state = 10;

                if (block->callback) {
                    (block->callback)(-3, block);
                }

                if (callback) {
                    (*callback)(0, block);
                }

                stateReady();
                break;
            }
        default:
            if (Canceling) {
                OSRestoreInterrupts(enabled);
                return FALSE;
            }
            Canceling = TRUE;
            CancelCallback = callback;
            break;
        }
        break;
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_RETRY:
        if (!(WaitingForCoverClose || WaitingForCoverOpen)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }

        if (WaitingForCoverOpen) {
            OSCancelAlarm(&CoverAlarm);
            WaitingForCoverOpen = FALSE;
        }

        if (block->state == DVD_STATE_NO_DISK)
            ResumeFromHere = 3;
        if (block->state == DVD_STATE_COVER_OPEN)
            ResumeFromHere = 4;
        if (block->state == DVD_STATE_WRONG_DISK)
            ResumeFromHere = 1;
        if (block->state == DVD_STATE_RETRY)
            ResumeFromHere = 2;
        if (block->state == DVD_STATE_MOTOR_STOPPED)
            ResumeFromHere = 7;

        finished = executing;
        executing = &DummyCommandBlock;

        block->state = DVD_STATE_CANCELED;
        if (block->callback) {
            (block->callback)(-3, block);
        }

        if (callback) {
            (callback)(0, block);
        }
        stateReady();
        break;
    case 12:
        finished = executing;
        executing = &DummyCommandBlock;
        block->state = 10;

        if (block->callback) {
            (block->callback)(-3, block);
        }

        if (callback) {
            (callback)(0, block);
        }

        stateReady();
        break;
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x80223D00

s32 DVDCancel(DVDCommandBlock* block) {
    int result;
    s32 state;
    u32 command;
    BOOL enabled;

    result = DVDCancelAsync((void*)block, cbForCancelSync);
    if (result == 0) {
        return -1;
    }

    enabled = OSDisableInterrupts();
    while (1) {
        state = ((volatile DVDCommandBlock*)block)->state; // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
        if (state == DVD_STATE_END || state == DVD_STATE_FATAL_ERROR || state == DVD_STATE_CANCELED) {
            break;
        }

        if (state == DVD_STATE_COVER_CLOSED) {
            command = ((volatile DVDCommandBlock*)block)->command; // volatile: callers poll the state that interrupt callbacks update (SDK, zeldaret/tp dvd.c)
            if ((command == 4) || (command == 5) || (command == 13) || (command == 33) || (command == 34) || (command == 41) ||
                (command == 42) || (command == 15) || (command == 37)) {
                break;
            }
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return 0;
} // 0x80224070

static void cbForCancelSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
} // 0x80224130

int DVDCancelAllAsync(DVDCBCallback callback) {
    BOOL enabled;
    DVDCommandBlock* p;
    int retVal;

    enabled = OSDisableInterrupts();
    DVDPause();
    while ((p = __DVDPopWaitingQueue())) {
        DVDCancelAsync(p, NULL);
    }

    if (executing) {
        retVal = DVDCancelAsync(executing, callback);
    } else {
        retVal = 1;
        if (callback) {
            callback(0, NULL);
        }
    }

    DVDResume();
    OSRestoreInterrupts(enabled);
    return retVal;
}

s32 DVDCancelAll(void) {
    int result;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    CancelAllSyncComplete = 0;
    result = DVDCancelAllAsync(cbForCancelAllSync);
    if (result == 0) {
        OSRestoreInterrupts(enabled);
        return -1;
    }

    while (1) {
        if (CancelAllSyncComplete == 0) {
            OSSleepThread(&__DVDThreadQueue);
        } else {
            break;
        }
    }

    OSRestoreInterrupts(enabled);
    return 0;
}

static void cbForCancelAllSync(s32 result, DVDCommandBlock* block) {
    CancelAllSyncComplete = 1;
    OSWakeupThread(&__DVDThreadQueue);
}

DVDDiskID* DVDGetCurrentDiskID(void) {
    return (void*)OSPhysicalToCached(0);
}

static void __BS2DVDLowCallback(u32 type) {
    __BS2DVDLowIntType = type;
} // 0x80224140

u32 __DVDGetCoverStatus(void) {
    u32 reg;
    __BS2DVDLowIntType = 0;
    DVDLowPrepareCoverRegister(__BS2DVDLowCallback);

    while (!__BS2DVDLowIntType) { 

    }

    if (!(__BS2DVDLowIntType & 1)) {
        return 0;
    }

    reg = DVDLowGetCoverRegister();

    if (__OSGetSystemTime() - LastResetEnd < DVD_RESETCOVER_TIMELAG_TICKS2) {
        return 0;
    } else if (reg & 1) {
        return 1;
    } else {
        return 2;
    }
} // 0x80224150

BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    BOOL enabled;
    s32 retVal, state;

    enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        state = -1;
    } else if (PausingFlag) {
        state = 8;
    } else {
        if (WaitingForCoverOpen) {
            state = 7;
        }

        if (WaitingForCoverClose) {
            state = 5;
        } else if (executing == NULL) {
            switch (ResumeFromHere) {
            case 3: {
                state = 4;
                break;
            }
            case 4: {
                state = 5;
                break;
            }
            case 1: {
                state = 6;
                break;
            }
            case 2: {
                state = 11;
                break;
            }
            case 7: {
                state = 7;
                break;
            }
            default: {
                state = 0;
                break;
            }
            }
        } else if (executing == &DummyCommandBlock) {
            state = 0;
        } else {
            state = executing->state;
        }
    }

    retVal = TRUE;
    switch (state) {
    case 1:
    case 9:
    case 10:
    case 2:
        block->state = 0;
        if (callback) {
            (*callback)(TRUE, block);
        }
        OSRestoreInterrupts(enabled);
        break;
    case -1:
    case 11:
    case 7:
    case 3:
    case 4:
    case 5:
    case 6:
    case 12:
        block->state = 0;
        if (callback) {
            (*callback)(FALSE, block);
        }
        OSRestoreInterrupts(enabled);
        break;
    case 0:
    case 8:
        OSRestoreInterrupts(enabled);

        block->command = 36;
        block->callback = callback;

        retVal = issueCommand(2, block);
        break;
    }

    return retVal;
} // 0x80224220

// NOTE: function doesn't exist in TP debug, needed for string data
void DVDPrepareDisk() {
    OSReport("DVDPrepareDisk(): null pointer is specified to command block address.");
    OSReport("DVDPrepareDisk(): null pointer is specified to id address.");
}

// NOTE: function doesn't exist in TP debug, needed for string data
void DVDPrepareDiskAsync() {
    OSReport("DVDPrepareDiskAsync(): You can't call this API from DVD application.  \n");
    OSReport("DVDPrepareDiskAsync(): You can't specify NULL to company name.  \n");
    OSReport("DVDPrepareDisk(): command block is used for processing previous request.");
}

BOOL DVDCheckDisk(void) {
    BOOL enabled;
    s32 retVal;
    s32 state;
    u32 coverReg;

    enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        state = -1;
    } else if (PausingFlag) {
        state = 8;
    } else {
        if (executing == NULL) {
            state = 0;
        } else if (executing == &DummyCommandBlock) {
            state = 0;
        } else {
            state = executing->state;
        }
    }

    switch (state) {
    case DVD_STATE_BUSY:
    case DVD_STATE_IGNORED:
    case DVD_STATE_CANCELED:
    case DVD_STATE_WAITING:
        retVal = TRUE;
        break;
    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_RETRY:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_COVER_CLOSED:
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
        retVal = FALSE;
        break;
    case DVD_STATE_END:
    case DVD_STATE_PAUSING:
        coverReg = __DIRegs[1];
        if (((coverReg >> 2) & 1) || (coverReg & 1)) {
            retVal = FALSE;
        } else if (ResumeFromHere != 0) {
            retVal = FALSE;
        } else {
            retVal = TRUE;
        }
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

void __DVDPrepareResetAsync(DVDCBCallback callback) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    __DVDClearWaitingQueue();

    if (Canceling) {
        CancelCallback = callback;
    } else {
        if (executing) {
            executing->callback = NULL;
        }

        DVDCancelAllAsync(callback);
    }

    OSRestoreInterrupts(enabled);
} // 0x80224460

static void Callback(s32 result, DVDCommandBlock* block ){
    Prepared = TRUE;
} // 0x80224580

void __DVDPrepareReset(void) {
    OSDisableInterrupts();

    Prepared = FALSE;

    __DVDPrepareResetAsync(Callback);
    OSEnableInterrupts();

    while (!(Prepared == TRUE)) {

    }
} // 0x80224590

int __DVDTestAlarm(const OSAlarm* alarm) {
    if (alarm == &ResetAlarm) {
        return 1;
    }
    return __DVDLowTestAlarm(alarm);
} // 0x802246D0

BOOL __DVDLowBreak(void) {
    Breaking = TRUE;
    return TRUE;
}

BOOL __DVDStopMotorAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    return TRUE;
} // 0x802246F0

void __DVDRestartMotor(void) {}
