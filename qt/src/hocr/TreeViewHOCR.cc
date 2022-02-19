
/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TreeViewHOCR.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
 * Copyright (C) 2021-2022 Donn Terry <aesopPlayer@gmail.com>
 *
 * gImageReader is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gImageReader is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QKeyEvent>
#include <QDebug>

#include "TreeViewHOCR.hh"
#include "HOCRDocument.hh"

void TreeViewHOCR::keyPressEvent(QKeyEvent *event) {
    if ((event->modifiers() & Qt::AltModifier) && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down
                                                || event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
        // Alt-arrow is an easy and irritating accident here
        return;
    }

    if(event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        // Do not do arrow keys here!
        HOCRDocument* document = static_cast<HOCRDocument*>(model());
        tabToNext(event, document->itemAtIndex(currentIndex()));
        return;
    }

    QTreeView::keyPressEvent(event);
}

void TreeViewHOCR::tabToNext(QKeyEvent* ev, const HOCRItem* currItem) {
    HOCRDocument* document = static_cast<HOCRDocument*>(model());

    bool atWord = currItem->itemClass() == "ocrx_word";
    enum actions {none, prevLine, prevWhole, nextLine, beginCurrent, nextWord, prevWord} action = actions::none;

    if((ev->modifiers() == Qt::NoModifier || ev->modifiers() == Qt::ShiftModifier) && ev->key() == Qt::Key_Down) {
        action = nextLine;
    }
    else if((ev->modifiers() == Qt::NoModifier || ev->modifiers() == Qt::ShiftModifier) && ev->key() == Qt::Key_Up) {
        action = atWord ? prevLine : prevWhole;
    }
    else if(ev->key() == Qt::Key_Tab || (ev->modifiers() == Qt::ShiftModifier && ev->key() == Qt::Key_Right)) {
        if(atWord) {
            if(currItem == currItem->parent()->children().last()) {
                action = nextLine;
            } else {
                action = nextWord;
            }
        } else {
            action = beginCurrent;
        }
    }
    else if(ev->key() == Qt::Key_Backtab || (ev->modifiers() == Qt::ShiftModifier && ev->key() == Qt::Key_Left)) {
        if(atWord) {
            if(currItem == currItem->parent()->children().first()) {
                action = prevLine;
            } else {
                action = prevWord;
            }
        } else {
            action = prevWhole;
        }
    }

    if (action != none) {
        bool next = false;
        QModelIndex index = document->indexAtItem(currItem);
        switch(action) {
        case nextLine:
            // Move to first word of next line
            index = document->prevOrNextIndex(true, index, "ocr_line");
            index = document->prevOrNextIndex(true, index, "ocrx_word");
            break;
        case prevLine:
            // Move to last word of prev line (from a word within this line)
            index = document->prevOrNextIndex(false, index, "ocr_line");
            index = document->prevOrNextIndex(false, index, "ocrx_word");
            break;
        case prevWhole:
            // Move to last word of prev line (from something enclosing)
        case prevWord:
            index = document->prevOrNextIndex(false, index, "ocrx_word");
            break;
        case nextWord:
            index = document->prevOrNextIndex(true, index, "ocrx_word");
            break;
        case beginCurrent: {
            // Move to first word below any parent items.
            const HOCRItem* item = currItem;
            while (item->children().size() > 0 && item->itemClass() != "ocrx_word") {
                item = item->children().first();
            }
            QModelIndex newIndex = document->indexAtItem(item);
            if (index == newIndex) {
                index = document->prevOrNextIndex(true, index, "ocrx_word", false);
            } else {
                index = newIndex;
            }
            break;
        } 
        }
        setCurrentIndex(index);
    }
}