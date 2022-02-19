/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRNormalizeDialog.hh
 * Copyright (C) 2021-2022 Donn Terry<aesopPlayer@gmail.com>
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

#ifndef HOCRNORMALIZEDIALOG_HH
#define HOCRNORMALIZEDIALOG_HH

#include "common.hh"
#include "ui_HOCRNormalizeDialog.h"
#include "HOCRDocument.hh"
#include "DisplayerToolHOCR.hh"

#include <QFontDialog>
#include <QModelIndex>

class PreferenceChoice;

class HOCRNormalize : public QObject {
	Q_OBJECT
public:
	void normalizeTree(HOCRDocument* hocrdocument, QList<HOCRItem*>* items);
	void normalizeSingle(HOCRDocument* hocrdocument, const HOCRItem* item);
	void currentDefault(QString &title, int& number);

private:
	void normalizeSelection(PreferenceChoice *pref);
	void normalizeItem(const HOCRItem* item, PreferenceChoice *pref);

	class HOCRNormalizeDialog : public QDialog {
	public:
		HOCRNormalizeDialog(HOCRNormalize* parent);

	private:
		HOCRNormalize* m_parent;
		Ui::HOCRNormalizeDialog ui;

		void fontName(int index, const QFont& font, QFontComboBox *fontBox);
		void fontSize(int index, int size, QSpinBox *editBox);
		void apply(int index);
	};

	HOCRNormalizeDialog* m_dialog;
	HOCRDocument* m_doc;
	QList<HOCRItem*>* m_items;

	PreferenceChoice *m_preferences[4];
};

#endif // HOCRNORMALIZEDIALOG_HH
