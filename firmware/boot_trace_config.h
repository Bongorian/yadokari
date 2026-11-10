#pragma once
// Temporary private diagnostic build. Raw startup text uses CDC only before
// updater ownership begins; disable for the final normal-protocol image.
#define GROOVE_BOOT_TRACE 0
