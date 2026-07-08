#pragma once

#include "LyricsTrack.h"
#include "Score.h"

namespace jamstudio::notation
{

/** Builds a synced LyricsTrack from note-attached lyrics in a Score. */
class ScoreLyricsExtractor
{
public:
    [[nodiscard]] static LyricsTrack fromScore (const Score& score);
};

} // namespace jamstudio::notation