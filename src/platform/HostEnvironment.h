#pragma once

namespace jamstudio::platform
{

/**
 * Adjust the process environment so GUI launches can find the same tools a
 * terminal would. On macOS, Finder and Launch Services start apps with a
 * minimal PATH that omits Homebrew, pipx, and user-local bins.
 */
void prepareHostEnvironment();

} // namespace jamstudio::platform
