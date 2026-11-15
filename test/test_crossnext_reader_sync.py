"""Regression wiring for CrossNext navigation entering Sticky's sync policy."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/activities/reader/EpubReaderActivity.cpp").read_text()


def method(name):
    start = SOURCE.index("EpubReaderActivity::" + name + "(")
    start = SOURCE.index("{", start)
    depth = 1
    end = start + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


class CrossNextReaderSync(unittest.TestCase):
    def test_new_navigation_records_intent(self):
        for name in ("restoreContentsPosition", "onReturnFromEndOfBook"):
            with self.subTest(name=name):
                body = method(name)
                self.assertLess(body.index("noteReadingIntent();"), body.index("currentSpineIndex ="))
        self.assertRegex(method("loadBook"), r"vocabularySpine <[^\n]+\{\s+noteReadingIntent\(\);")
        self.assertRegex(method("onReaderMenuConfirm"),
                         r"noteReadingIntent\(\);\s+currentSpineIndex = chapter->spineIndex;")

    def test_finish_uses_revision_aware_save(self):
        body = method("saveFinishedPosition")
        self.assertIn("noteReadingIntent();", body)
        self.assertIn("saveProgress(epub->getSpineItemsCount(), 0, 0)", body)
        self.assertNotIn("EpubReaderUtils::saveProgress", body)

    def test_completed_book_sync_uses_final_content_position(self):
        body = method("getCurrentPosition")
        self.assertIn("atEnd ? std::max(2, savedTotalPages)", body)
        self.assertIn("atEnd ? totalPages - 1", body)
        self.assertIn("if (!atEnd && section", body)

    def test_layout_changes_do_not_create_reading_edits(self):
        for name in ("applyOrientation", "applyDeferredReposition", "applyReaderTextSettings"):
            with self.subTest(name=name):
                self.assertNotIn("noteReadingIntent", method(name))

    def test_sticky_uses_button_operable_native_menu(self):
        body = method("usesToolbarMenu")
        sticky = body.split("#ifdef FREEINK_DEVICE_STICKY", 1)[1].split("#else", 1)[0]
        self.assertIn("return false;", sticky)

    def test_prefetch_cleanup_and_exit_sync_both_survive_port(self):
        body = method("onExit")
        self.assertIn("clearPrefetchedPage();", body)
        self.assertIn("readingSyncState.dirty", body)
        self.assertIn("autosync::pushProgress", body)
        self.assertEqual(len(re.findall(r"void EpubReaderActivity::onExit\(", SOURCE)), 1)


if __name__ == "__main__":
    unittest.main()
