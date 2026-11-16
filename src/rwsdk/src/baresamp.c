// Reference: tgsm/pbg config/GPLE9G/splits.txt (baresamp.c, after baraster.c)
// Reference: tgsm/pbg config/GPLE9G/symbols.txt (ImageResampleGetSpan, ImageResampleGetAvgPixel, RwImageResample, RwImageCreateResample)
// Unit name and boundary: Gormiti link map (RwCore.a baresamp). Locals and parameters: Gormiti DWARF.
/*
 * baresamp.c: image resampling. Each destination pixel is the area-weighted average of the
 * source pixels its footprint covers; positions and footprints are 16.16 fixed point, and the
 * averaging is done in real colour.
 */
#include <rwsdk/baresamp.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/badebug.h>

/*
 * Averages one source line between two fixed-point positions: the end pixels are weighted by
 * how much of them the span covers, the pixels between them count in full.
 * Locals: Gormiti DWARF.
 */
static void ImageResampleGetSpan(const RwImage* image, long nStartX, long nEndX, long nY,
                                 RwRGBAReal* rrpCol) {
    float nArea;
    float nScale;
    RwRGBA* rpSpan;

    RWASSERT(image);
    RWASSERT(rrpCol);

    nArea = RwFixedToReal(nEndX - nStartX);
    rpSpan = (RwRGBA*)(image->cpPixels + RwFixedToInt(nY) * image->stride) + RwFixedToInt(nStartX);

    if (RwFixedToInt(nStartX) == RwFixedToInt(nEndX)) {
        RwRGBARealFromRwRGBA(rrpCol, rpSpan);
        nScale = RwFixedToReal(nEndX - nStartX);
        RwRGBARealScale(rrpCol, rrpCol, nScale);
    } else {
        RwRGBAReal rrAdd;
        long nPos = RwIntToFixed(RwFixedToInt(nStartX) + 1);

        RwRGBARealFromRwRGBA(rrpCol, rpSpan);
        nScale = RwFixedToReal(nPos - nStartX);
        RwRGBARealScale(rrpCol, rrpCol, nScale);
        rpSpan++;

        while (RwFixedToInt(nPos) != RwFixedToInt(nEndX)) {
            RwRGBARealFromRwRGBA(&rrAdd, rpSpan);
            RwRGBARealAdd(rrpCol, rrpCol, &rrAdd);
            rpSpan++;
            nPos += RwIntToFixed(1);
        }

        RwRGBARealFromRwRGBA(&rrAdd, rpSpan);
        nScale = RwFixedToReal(nEndX - nPos);
        RwRGBARealScale(&rrAdd, &rrAdd, nScale);
        RwRGBARealAdd(rrpCol, rrpCol, &rrAdd);
    }

    nScale = (RwReal)1 / nArea;
    RwRGBARealScale(rrpCol, rrpCol, nScale);
} // 0x802975B0

/*
 * Averages the source rectangle between two fixed-point corners, line by line: the end lines
 * are weighted by how much of them the rectangle covers, the lines between them count in full.
 * Locals: Gormiti DWARF.
 */
static void ImageResampleGetAvgPixel(const RwImage* image, long nXStart, long nXEnd, long nYStart,
                                     long nYEnd, RwRGBAReal* rrpCol) {
    float nArea;
    float nScale;

    RWASSERT(image);
    RWASSERT(rrpCol);

    nArea = RwFixedToReal(nYEnd - nYStart);

    if (RwFixedToInt(nYStart) == RwFixedToInt(nYEnd)) {
        ImageResampleGetSpan(image, nXStart, nXEnd, nYStart, rrpCol);
        nScale = RwFixedToReal(nYEnd - nYStart);
        RwRGBARealScale(rrpCol, rrpCol, nScale);
    } else {
        RwRGBAReal rrAdd;
        long nPos = RwIntToFixed(RwFixedToInt(nYStart) + 1);

        ImageResampleGetSpan(image, nXStart, nXEnd, nYStart, rrpCol);
        nScale = RwFixedToReal(nPos - nYStart);
        RwRGBARealScale(rrpCol, rrpCol, nScale);

        while (RwFixedToInt(nPos) != RwFixedToInt(nYEnd)) {
            ImageResampleGetSpan(image, nXStart, nXEnd, nPos, &rrAdd);
            RwRGBARealAdd(rrpCol, &rrAdd, rrpCol);
            nPos += RwIntToFixed(1);
        }

        ImageResampleGetSpan(image, nXStart, nXEnd, nPos, &rrAdd);
        nScale = RwFixedToReal(nYEnd - nPos);
        RwRGBARealScale(&rrAdd, &rrAdd, nScale);
        RwRGBARealAdd(rrpCol, rrpCol, &rrAdd);
    }

    nScale = (RwReal)1 / nArea;
    RwRGBARealScale(rrpCol, rrpCol, nScale);
} // 0x80297904

/*
 * Resamples a 32-bit image into another of any size, averaging the source area under each
 * destination pixel. The destination takes the source's gamma-corrected flag.
 */
RwImage* RwImageResample(RwImage* dstImage, const RwImage* srcImage) {
    int nX, nY;
    long nXPos;
    long nXDelta;
    long nYPos;
    long nYDelta;
    int dstWidth, dstHeight;
    int srcWidth, srcHeight;

    RWASSERT(dstImage);
    RWASSERT(srcImage);
    RWASSERT(RwImageGetDepth(srcImage) == 32 && RwImageGetDepth(dstImage) == 32);

    dstWidth = RwImageGetWidth(dstImage);
    dstHeight = RwImageGetHeight(dstImage);
    srcWidth = RwImageGetWidth(srcImage);
    srcHeight = RwImageGetHeight(srcImage);

    dstImage->flags |= srcImage->flags & rwIMAGEGAMMACORRECTED;

    nXDelta = RwRealToFixed((RwReal)srcWidth / (RwReal)dstWidth);
    nYDelta = RwRealToFixed((RwReal)srcHeight / (RwReal)dstHeight);

    nYPos = 0;
    for (nY = 0; nY < dstHeight; nY++) {
        RwRGBA* rpDstSpan = (RwRGBA*)(RwImageGetPixels(dstImage) + RwImageGetStride(dstImage) * nY);

        nXPos = 0;
        for (nX = 0; nX < dstWidth; nX++) {
            RwRGBAReal rrCol;

            ImageResampleGetAvgPixel(srcImage, nXPos, nXPos + nXDelta - 1, nYPos, nYPos + nYDelta - 1, &rrCol);
            RwRGBAFromRwRGBAReal(rpDstSpan + nX, &rrCol);
            nXPos += nXDelta;
        }

        nYPos += nYDelta;
    }

    return dstImage;
} // 0x80297B98

/*
 * Creates a 32-bit image of the given size holding a resampled copy of an image; an image of
 * another depth is first converted through a temporary 32-bit copy.
 */
RwImage* RwImageCreateResample(const RwImage* srcImage, RwInt32 width, RwInt32 height) {
    RwImage* dstImage = RwImageCreate(width, height, 32);

    if (!dstImage) {
        return NULL;
    }

    if (!RwImageAllocatePixels(dstImage)) {
        RwImageDestroy(dstImage);
        return NULL;
    }

    if (RwImageGetDepth(srcImage) != 32) {
        RwImage* tmpImage = RwImageCreate(RwImageGetWidth(srcImage), RwImageGetHeight(srcImage), 32);

        if (!tmpImage) {
            RwImageFreePixels(dstImage);
            RwImageDestroy(dstImage);
            return NULL;
        }

        if (!RwImageAllocatePixels(tmpImage)) {
            RwImageDestroy(tmpImage);
            RwImageFreePixels(dstImage);
            RwImageDestroy(dstImage);
            return NULL;
        }

        RwImageCopy(tmpImage, srcImage);

        if (!RwImageResample(dstImage, tmpImage)) {
            RwImageFreePixels(tmpImage);
            RwImageDestroy(tmpImage);
            RwImageFreePixels(dstImage);
            RwImageDestroy(dstImage);
            return NULL;
        }

        RwImageFreePixels(tmpImage);
        RwImageDestroy(tmpImage);
    } else if (!RwImageResample(dstImage, srcImage)) {
        RwImageFreePixels(dstImage);
        RwImageDestroy(dstImage);
        return NULL;
    }

    return dstImage;
} // 0x80297D7C
