/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TreeViewHOCR.hh
 * Copyright (C) 2021 Donn Terry <aesopPlayer@gmail.com>
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
 * 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TREEVIEWHOCR_HH
#define TREEVIEWHOCR_HH

#include <QTreeView>

class TreeViewHOCR: public QTreeView {
	//Q_OBJECT
public:
    TreeViewHOCR(QWidget* parent = nullptr) : QTreeView(parent) {
        setObjectName("TreeViewHOCR");
    }
    ~TreeViewHOCR() {}

    void keyPressEvent(QKeyEvent *event) override {
        // Override to make public for ProofReadWidget
        if ((event->modifiers() & Qt::AltModifier) && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down
                                                    || event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
            // Alt-arrow is an easy and irritating accident here
            return;
        }
        QTreeView::keyPressEvent(event);
    }

private:
	// Disable auto tab handling
	bool focusNextPrevChild(bool next) override { 
        return false; 
    }
};
#endif // TREEVIEWHOCR_HH