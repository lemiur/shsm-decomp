// Unit name and boundary: Gormiti link map (RwCore.a p2altmdl). Locals: Gormiti DWARF.
/*
 * p2altmdl.c: the pipeline packet's clusters. Only _rxPacketDestroy has a copy in retail; the
 * rest of the object (cross-pipeline cluster alignment, embedded packets) is dead-stripped.
 */
#include <rwsdk/pipe/p2/p2altmdl.h>

/*
 * Hands the pipeline heap's cluster data back and empties every cluster that is in use; the
 * packet is left unused (its pipeline's embedded packet is free again). The loop body is one
 * source line in Gormiti's line table: the CLUSTERTERMINATE macro, whose 32-bit store of flags
 * and stride is the single stw retail has there (Gormiti's Debug build stores a word too).
 */
void _rxPacketDestroy(RxPacket* packet) {
    RwUInt32 n;
    RxCluster* cl;

    packet->pipeline->embeddedPacketState = rxPKST_UNUSED;

    cl = packet->clusters;
    n = packet->numClusters;
    do {
        CLUSTERTERMINATE(cl);
        cl++;
    } while (--n);

    packet->flags = 0;
} // 0x8029B26C
