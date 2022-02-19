/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRProofReadWidget.hh
 * Copyright (C) 2022 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRPROOFREADWIDGET_HH
#define HOCRPROOFREADWIDGET_HH

#include <QMap>
#include <QFrame>

#include <TreeViewHOCR.hh>

class QLabel;
class QSpinBox;
class QTreeView;
class QVBoxLayout;
class HOCRItem;

class HOCRProofReadWidget : public QFrame {
	Q_OBJECT
public:
	HOCRProofReadWidget(TreeViewHOCR* treeView, QWidget* parent = nullptr);
	void setProofreadEnabled(bool enabled);
	void clear();
	QTreeView* documentTree() const { return m_treeView; }
	void setConfidenceLabel(int wconf);
	QString confidenceStyle(int wconf) const;
	void adjustFontSize(int diff);
	void showWidget(bool show);

private:
	class LineEdit;
	friend class OutputEditorHOCR;
	class PointerWidget;

	TreeViewHOCR* m_treeView = nullptr;
	QVBoxLayout* m_linesLayout = nullptr;
	const HOCRItem* m_currentLine = nullptr;
	QWidget* m_controlsWidget = nullptr;
	QLabel* m_confidenceLabel = nullptr;
	QMap<const HOCRItem*, QWidget*> m_currentLines;
	QSpinBox* m_spinLinesBefore = nullptr;
	QSpinBox* m_spinLinesAfter = nullptr;
	QSpinBox* m_gapWidth = nullptr;
	int m_fontSizeDiff = 0;
	bool m_enabled = false;
	LineEdit *m_stub = nullptr;
	PointerWidget *m_pointer;
	int m_sceneBoxLeft;
	int m_sceneBoxRight;
	bool m_hidden = false;

	// Disable auto tab handling
	bool focusNextPrevChild(bool) override { return false; }

	void repositionWidget();
	void repositionPointer(int& frameY);
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void updateWidget(bool force = false);
	void showShortcutsDialog();
};


#endif // HOCRPROOFREADWIDGET_HH
