// Unit name and boundary: Gormiti link map (fmod_event.a fmod_musicengine.obj)

#include <new>

#include <fmod/fmod.hpp>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_musicengine.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names): lbl_80452AB0 (0x1C0), lbl_80452C70, lbl_80452C88, lbl_80452CA0, lbl_80452CB4,
// lbl_80452CC0, lbl_80452CDC (.data); lbl_8052FFD0 .. lbl_8052FFF0 (.sdata2).

namespace FMOD {

// Not reconstructed yet (NonMatching): getStartSegment (anonymous namespace), MusicSync::getCurrentBeat,
// getNextSyncTime, MusicEngine::init, release, stop, reset, setPaused, getInfo, update, beginTheme, endTheme,
// beginDefaultTransition, findSecondaryPlayerWithTheme, PlayerEntry::getNextSegment. Most of them call
// SegmentPlayer, SegmentBuffer, the repositories or Segment::quantizeTime, whose units are not reconstructed.
// MusicEngine::MusicEngine (66%): retail clears the primary state's cue list and the MusicSync block in its
// own order, which the member initialisers do not reproduce.

// Top of the primary state's scene stack: the primary state has one scene.
unsigned int MusicStatePrimary::getSceneStackTop() const {
    return 1;
} // 0x803AA590

// Theme of the top stack entry, or 0 when the stack is empty.
// Not matched: retail keeps the copied StackItem in a stack frame, this copy stays in registers
// (wall: placement of the aggregate local).
unsigned int MusicStatePrimary::getThemeStackTop() const {
    ThemeStack::StackItem item;
    if ( m_themestack.m_length != 0 ) {
        item = m_themestack.m_stack[ m_themestack.m_length - 1 ];
    } else {
        item.theme_id = 0;
        item.stack_id = 0;
    }
    return item.theme_id;
} // 0x803AA544

// Theme of the secondary state's stack top, kept by the state itself.
unsigned int MusicStateSecondary::getThemeStackTop() const {
    return m_themestacktop;
} // 0x803AA5F0

// The scene stack top follows the primary state, or is 0 when there is no primary state.
unsigned int MusicStateSecondary::getSceneStackTop() const {
    if ( m_primary != NULL ) {
        return m_primary->getSceneStackTop();
    }
    return 0;
} // 0x803AA5F8

FMOD_RESULT MusicEngine::updatePosition( unsigned int, unsigned int ) {
    return FMOD_OK;
} // 0x803ABEEC

FMOD_RESULT MusicEngine::startSegment( unsigned int segment ) {
    if ( m_callback != NULL ) {
        FMOD_RESULT result = m_callback( FMOD_MUSIC_CALLBACKTYPE_SEGMENT_START, (void *)segment, NULL, m_callbackdata );
        if ( result != FMOD_OK ) {
            return result;
        }
    }
    return FMOD_OK;
} // 0x803ABEF4

FMOD_RESULT MusicEngine::endSegment( unsigned int segment ) {
    if ( m_callback != NULL ) {
        FMOD_RESULT result = m_callback( FMOD_MUSIC_CALLBACKTYPE_SEGMENT_END, (void *)segment, NULL, m_callbackdata );
        if ( result != FMOD_OK ) {
            return result;
        }
    }
    return FMOD_OK;
} // 0x803ABF40

FMOD_RESULT MusicEngine::setCallback( FMOD_MUSIC_CALLBACK callback, void *userdata ) {
    m_callback = callback;
    m_callbackdata = userdata;
    return FMOD_OK;
} // 0x803AB08C

FMOD_RESULT MusicEngine::start() {
    m_playing = true;
    return FMOD_OK;
} // 0x803AAC4C

FMOD_RESULT MusicEngine::getPaused( bool *paused ) {
    if ( paused == NULL ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    *paused = m_paused;
    return FMOD_OK;
} // 0x803AAF6C

FMOD_RESULT MusicEngine::setVolume( float volume ) {
    FMOD_RESULT _result = m_channelgroup->setVolume( volume );
    if ( _result != FMOD_OK ) {
        return _result;
    }
    return FMOD_OK;
} // 0x803AAE6C

FMOD_RESULT MusicEngine::getVolume( float *volume ) {
    FMOD_RESULT _result = m_channelgroup->getVolume( volume );
    if ( _result != FMOD_OK ) {
        return _result;
    }
    return FMOD_OK;
} // 0x803AAEA4

FMOD_RESULT MusicEngine::setMute( bool mute ) {
    FMOD_RESULT _result = m_channelgroup->setMute( mute );
    if ( _result != FMOD_OK ) {
        return _result;
    }
    return FMOD_OK;
} // 0x803AAF8C

FMOD_RESULT MusicEngine::getMute( bool *mute ) {
    FMOD_RESULT _result = m_channelgroup->getMute( mute );
    if ( _result != FMOD_OK ) {
        return _result;
    }
    return FMOD_OK;
} // 0x803AAFC4

// The secondary state takes its theme as the stack top, and reports it through handle when asked.
FMOD_RESULT MusicStateSecondary::addTheme( unsigned int theme, unsigned int *handle ) {
    m_themestacktop = theme;
    if ( handle != NULL ) {
        *handle = theme;
    }
    return FMOD_OK;
} // 0x803AA598

// Removing the theme on top clears the stack top.
FMOD_RESULT MusicStateSecondary::removeTheme( unsigned int handle ) {
    if ( handle == m_themestacktop ) {
        m_themestacktop = 0;
    }
    return FMOD_OK;
} // 0x803AA5B0

// A cue is active when the primary state has it; a secondary state without a primary has none.
bool MusicStateSecondary::isCueActive( unsigned int cue ) const {
    if ( m_primary != NULL ) {
        return m_primary->isCueActive( cue );
    }
    return false;
} // 0x803AA5CC

// Adds a theme on top of the primary stack when there is room; handle receives its stack id (0 when full).
FMOD_RESULT MusicStatePrimary::addTheme( unsigned int theme, unsigned int *handle ) {
    unsigned int stack_id;
    if ( m_themestack.m_length < m_themestack.m_max_length ) {
        m_themestack.m_stack[ m_themestack.m_length ].theme_id = theme;
        m_themestack.m_stack[ m_themestack.m_length ].stack_id = m_themestack.m_next_stack_id;
        m_themestack.m_next_stack_id++;
        m_themestack.m_length++;
        stack_id = m_themestack.m_stack[ m_themestack.m_length - 1 ].stack_id;
    } else {
        stack_id = 0;
    }
    if ( handle != NULL ) {
        *handle = stack_id;
    }
    return FMOD_OK;
} // 0x803AA3E8

// Removes the theme with the given stack id, closing the gap by moving the entries above it down.
FMOD_RESULT MusicStatePrimary::removeTheme( unsigned int handle ) {
    unsigned int i;
    for ( i = 0; i < m_themestack.m_length; i++ ) {
        if ( m_themestack.m_stack[ i ].stack_id == handle ) {
            break;
        }
    }
    if ( i < m_themestack.m_length ) {
        m_themestack.m_length--;
        for ( ; i < m_themestack.m_length; i++ ) {
            m_themestack.m_stack[ i ] = m_themestack.m_stack[ i + 1 ];
        }
    }
    return FMOD_OK;
} // 0x803AA460

// A cue is active while it has begun and not yet ended.
bool MusicStatePrimary::isCueActive( unsigned int cue ) const {
    const LinkedListNode *node;
    for ( node = m_cuestates.m_cuestates.mNodeNext; node != &m_cuestates.m_cuestates; node = node->mNodeNext ) {
        if ( static_cast<const CueState *>( node )->m_cue == cue ) {
            break;
        }
    }
    if ( node == &m_cuestates.m_cuestates ) {
        node = NULL;
    }
    unsigned int count;
    if ( node != NULL ) {
        count = static_cast<const CueState *>( node )->m_count;
    } else {
        count = 0;
    }
    return count != 0;
} // 0x803AA4EC

// Starts a cue: the first begin creates its entry; each begin counts one more.
FMOD_RESULT MusicEngine::beginCue( unsigned int cue ) {
    LinkedListNode *node;
    for ( node = m_primary_state.m_cuestates.m_cuestates.mNodeNext; node != &m_primary_state.m_cuestates.m_cuestates; node = node->mNodeNext ) {
        if ( static_cast<CueState *>( node )->m_cue == cue ) {
            break;
        }
    }
    if ( node == &m_primary_state.m_cuestates.m_cuestates ) {
        node = NULL;
    }
    CueState *state = static_cast<CueState *>( node );
    if ( state == NULL ) {
#line 102
        state = new ( FMOD_Memory_Alloc( sizeof( CueState ) ) ) CueState;
        if ( state == NULL ) {
            return FMOD_ERR_MEMORY;
        }
        state->m_cue = cue;
        state->addBefore( &m_primary_state.m_cuestates.m_cuestates );
    }
    state->m_count++;
    return FMOD_OK;
} // 0x803AB950

// Ends one begin of a cue; the entry stays in the table with its count.
FMOD_RESULT MusicEngine::endCue( unsigned int cue ) {
    LinkedListNode *node;
    for ( node = m_primary_state.m_cuestates.m_cuestates.mNodeNext; node != &m_primary_state.m_cuestates.m_cuestates; node = node->mNodeNext ) {
        if ( static_cast<CueState *>( node )->m_cue == cue ) {
            break;
        }
    }
    if ( node == &m_primary_state.m_cuestates.m_cuestates ) {
        node = NULL;
    }
    if ( node != NULL ) {
        static_cast<CueState *>( node )->m_count--;
    }
    return FMOD_OK;
} // 0x803ABA48

// Every member starts cleared; the sub-objects set up their own state.
MusicEngine::MusicEngine()
    : m_system( NULL ), m_numplayers( 0 ), m_players( NULL ), m_primary_player( NULL ), m_master_player( NULL ),
      m_paused( false ), m_playing( false ), m_primary_state(), m_musicsync(), m_channelgroup( NULL ),
      m_timer_end( 0 ), m_current_bar( 0 ), m_current_beat( 0 ), m_callback( NULL ), m_callbackdata( NULL ) {
} // 0x803AA864

} // namespace FMOD
