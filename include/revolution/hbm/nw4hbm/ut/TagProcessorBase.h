#ifndef NW4HBM_UT_TAG_PROCESSOR_BASE_H
#define NW4HBM_UT_TAG_PROCESSOR_BASE_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/TagProcessorBase.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_TagProcessorBase.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {

struct Rect;
template <typename CharT> class TextWriterBase;

/*
 * Operation
 *
 * What the text writer does after a tag processor has handled a character.
 */
enum Operation {
    OPERATION_DEFAULT,
    OPERATION_NO_CHAR_SPACE,
    OPERATION_CHAR_SPACE,
    OPERATION_NEXT_LINE,
    OPERATION_END_DRAW,

    NUM_OF_OPERATION
};

/*
 * PrintContext
 *
 * State of one TextWriterBase print, handed to the tag processor for every
 * control character.
 */
template <typename CharT>
struct PrintContext {
    TextWriterBase<CharT>* writer; // 0x00
    const CharT* str;              // 0x04
    f32 xOrigin;                   // 0x08 left edge of the text block
    f32 yOrigin;                   // 0x0C
    u32 flags;                     // 0x10
};

/*
 * TagProcessorBase
 *
 * Default handler of control characters in printed text: a line feed starts
 * a new line, a tab advances the cursor to the next tab stop.
 */
template <typename CharT>
class TagProcessorBase {
public:
    TagProcessorBase();
    virtual ~TagProcessorBase();
    virtual Operation Process(u16 code, PrintContext<CharT>* context);
    virtual Operation CalcRect(Rect* pRect, u16 code, PrintContext<CharT>* context);

    void ProcessLinefeed(PrintContext<CharT>* context);
    void ProcessTab(PrintContext<CharT>* context);
};

} // namespace ut
} // namespace nw4hbm

#endif
