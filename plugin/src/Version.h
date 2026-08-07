#pragma once

/** Version and per-build stamp baked in at compile time.
    The stamp changes every rebuild so you can tell whether the host actually
    loaded the binary you just built. */
namespace the89th_version
{
const char* version() noexcept;
const char* buildStamp() noexcept;
const char* gitHash() noexcept;

/** "0.0.1  |  a1b2c3d" */
const char* banner() noexcept;
} // namespace the89th_version
