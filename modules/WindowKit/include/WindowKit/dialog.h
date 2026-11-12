// Dialog — a message being read: its pages, the page up, and how much of it is typed out.
//
//     WindowKit::Dialog dialog;                                                  // in the game's state
//     dialog.open(passage, {.width = room.width, .height = room.height}, font);  // when a message starts
//     dialog.advance(confirmPressed);                                            // once per tick
//
// `room` is inside(box, border) for the box the message is drawn in (dialog_box.h), so what the dialog
// pages and what the box draws agree. A Dialog is a value a game's simulation holds; it names no draw
// type and returns none.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/geometry.h"

#include "TextKit/font.h"
#include "TextKit/reveal.h"
#include "TextKit/text.h"

namespace WindowKit {

// A message being read: its text cut into pages that fit a text area, which page is up, and how much
// of it is typed out. The game advances it once per tick.
struct Dialog {
    std::uint32_t ticksPerGlyph = 2;     // as TextKit::Reveal's
    std::uint32_t arrowBlinkTicks = 16;  // the arrow shows this long, then hides this long; 0 keeps it on

    // Starts `text`: wrapped to as many characters as `area` is wide in `font` and cut into pages of as
    // many lines as it is tall (at least one of each; an advance of 0 on an axis counts as 1 pixel).
    // Opening while a message is showing replaces it. An empty `text` opens nothing and leaves the
    // dialog as it was.
    void open(std::string_view text, retropp::PixelSize area, const TextKit::Font& font) {
        if (text.empty()) {
            return;
        }
        const int stepX = std::max(1, font.advance.x);
        const int stepY = std::max(1, font.advance.y);
        const int columns = std::max(1, area.width / stepX);
        const std::size_t lines = static_cast<std::size_t>(std::max(1, area.height / stepY));

        const std::string wrapped = TextKit::wrap(text, columns);
        std::vector<std::string> all;
        std::size_t start = 0;
        while (true) {
            const std::size_t end = wrapped.find('\n', start);
            all.push_back(wrapped.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }

        pages_.clear();
        for (std::size_t first = 0; first < all.size(); first += lines) {
            std::string page;
            for (std::size_t n = first; n < std::min(first + lines, all.size()); ++n) {
                if (n != first) {
                    page += '\n';
                }
                page += all[n];
            }
            pages_.push_back(std::move(page));
        }
        startPage(0);
    }

    // Ends the message: nothing is showing.
    void close() noexcept {
        pages_.clear();
        startPage(0);
    }

    // One tick. The page types on. With `confirm`: a page still typing is shown whole; a whole page
    // gives way to the next, typed from its start; the last page closes the message. Nothing while no
    // message is showing.
    void advance(bool confirm) {
        if (!showing()) {
            return;
        }
        const bool wasDone = pageDone();
        reveal_.advance();
        if (wasDone) {
            ++doneTicks_;
        }
        if (!confirm) {
            return;
        }
        if (!pageDone()) {
            reveal_.skip();
        } else if (lastPage()) {
            close();
        } else {
            startPage(page_ + 1);
        }
    }

    [[nodiscard]] bool showing() const noexcept { return !pages_.empty(); }
    [[nodiscard]] std::size_t page() const noexcept { return page_; }  // the page up, from 0
    [[nodiscard]] std::size_t pageCount() const noexcept { return pages_.size(); }

    // The page up, its lines joined by '\n'; empty when nothing is showing.
    [[nodiscard]] std::string_view text() const noexcept {
        return showing() ? std::string_view{pages_[page_]} : std::string_view{};
    }

    // How much of the page is typed out, as TextKit::Reveal::shown: std::string_view::npos once the page
    // has been shown whole by a confirm.
    [[nodiscard]] std::size_t shown() const noexcept { return reveal_.shown(); }

    [[nodiscard]] bool pageDone() const noexcept { return showing() && reveal_.done(pages_[page_].size()); }
    [[nodiscard]] bool lastPage() const noexcept { return showing() && page_ + 1 == pages_.size(); }

    // Whether the arrow shows: the page is done, another follows, and the blink is in its on phase —
    // on from the tick the page is done for arrowBlinkTicks ticks, then off as long, and so on.
    [[nodiscard]] bool arrowOn() const noexcept {
        if (!pageDone() || lastPage()) {
            return false;
        }
        return arrowBlinkTicks == 0 || (doneTicks_ / arrowBlinkTicks) % 2 == 0;
    }

private:
    void startPage(std::size_t page) noexcept {
        page_ = page;
        reveal_ = TextKit::Reveal{};
        reveal_.ticksPerGlyph = ticksPerGlyph;
        doneTicks_ = 0;
    }

    std::vector<std::string> pages_;
    std::size_t page_ = 0;
    TextKit::Reveal reveal_;
    std::uint64_t doneTicks_ = 0;
};

}  // namespace WindowKit
