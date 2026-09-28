#include "ObsidianBackfill.h"

#include <Logging.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ClippingStore.h"
#include "ObsidianPendingQueue.h"

namespace ObsidianBackfill {

Result queueStoredClippings() {
  Result result;

  std::vector<uint32_t> known = ObsidianPendingQueue::readSentKeys();
  for (const auto& pending : ObsidianPendingQueue::readAll()) {
    known.push_back(ObsidianPendingQueue::clippingKey(pending.book, pending.text));
  }
  std::sort(known.begin(), known.end());

  std::vector<ClippedBookEntry> books;
  ClippingStore::getAllClippedBooks(books);

  // Borrows the shared store, which is otherwise only loaded while a book is
  // open in the reader; this runs from the web server, when none is.
  ClippingStore& store = ClippingStore::getInstance();
  std::string text;
  for (const auto& book : books) {
    if (!store.loadForBook(book.bookPath, book.bookTitle, book.bookAuthor, book.bookType)) continue;
    for (const Clipping& clipping : store.getClippings()) {
      if (!store.readClippingText(clipping, text) || text.empty()) continue;
      const uint32_t key = ObsidianPendingQueue::clippingKey(book.bookTitle, text);
      if (std::binary_search(known.begin(), known.end(), key)) {
        result.skipped++;
        continue;
      }
      ObsidianPendingClipping pending;
      pending.book = book.bookTitle;
      pending.author = book.bookAuthor;
      pending.chapter = clipping.chapterTitle;
      pending.page = static_cast<int>(clipping.startPage) + 1;
      pending.text = text;
      pending.timestamp = clipping.timestamp;
      if (!ObsidianPendingQueue::append(pending)) {
        result.dropped++;
        continue;
      }
      known.insert(std::upper_bound(known.begin(), known.end(), key), key);
      result.queued++;
    }
    store.unload();
  }

  LOG_DBG("OBS", "Backfill queued %u, skipped %u, dropped %u", static_cast<unsigned>(result.queued),
          static_cast<unsigned>(result.skipped), static_cast<unsigned>(result.dropped));
  return result;
}

}  // namespace ObsidianBackfill
