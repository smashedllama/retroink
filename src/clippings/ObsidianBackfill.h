#pragma once

#include <cstddef>

// Queues highlights that never went through Obsidian sync: ones saved before
// sync was set up or switched on, including any carried over from CrossInk.
// Only new highlights are queued as they're saved, so without this those
// older ones would never reach the vault.
namespace ObsidianBackfill {

struct Result {
  size_t queued = 0;   // added to the pending queue
  size_t skipped = 0;  // already sent or already waiting in the queue
  size_t dropped = 0;  // did not fit in the queue (see MAX_PENDING_BYTES)
};

// Walks every book's stored highlights and appends the ones not already sent
// (per ObsidianPendingQueue's sent record) or pending. Nothing is sent here;
// the next sync delivers them.
Result queueStoredClippings();

}  // namespace ObsidianBackfill
