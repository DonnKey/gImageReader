/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRProofReadWidget.cc
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

#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "Displayer.hh"
#include "ConfigSettings.hh"
#include "HOCRDocument.hh"
#include "HOCRProofReadWidget.hh"
#include "HOCRSpellChecker.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "OutputEditorHOCR.hh"


static const QString classNames[] {"", "ocr_page", "ocr_carea", "ocr_par", "ocr_line", "ocrx_word"};
class HOCRProofReadWidget::LineEdit : public QLineEdit {
public:
	LineEdit(HOCRProofReadWidget* proofReadWidget, const HOCRItem* wordItem, QWidget* parent = nullptr) :
		QLineEdit(wordItem->text(), parent), m_proofReadWidget(proofReadWidget), m_wordItem(wordItem) {
		connect(this, &LineEdit::textChanged, this, &LineEdit::onTextChanged);

		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		connect(document, &HOCRDocument::dataChanged, this, &LineEdit::onModelDataChanged);
		connect(document, &HOCRDocument::itemAttributeChanged, this, &LineEdit::onAttributeChanged);

		QModelIndex index = document->indexAtItem(m_wordItem);
		setReadOnly(!m_wordItem->isEnabled());
		setStyleSheet( getStyle( document->indexIsMisspelledWord(index), m_wordItem->isEnabled() ) );

		QFont ft = m_wordItem->fontFamily();
		ft.setBold(m_wordItem->fontBold());
		ft.setItalic(m_wordItem->fontItalic());
		// Don't change the widget font size - be sure it remains readable 
		ft.setPointSize(font().pointSize());
		setFont(ft);
		setObjectName(wordItem->text());
	}
	LineEdit(HOCRProofReadWidget* proofReadWidget, QWidget* parent = nullptr) :
		// Special for the invisible m_stub that gets focus when there aren't words to focus on.
		QLineEdit("", parent), m_wordItem(nullptr), m_proofReadWidget(proofReadWidget) {
		resize(0,0);
		setObjectName("*stub*");
	}
	const HOCRItem* item() const { return m_wordItem; }
	void setStubItem(const HOCRItem* item) {
		Q_ASSERT(this == m_proofReadWidget->m_stub);
		m_wordItem = item;
	}

private:
	// Disable auto tab handling
	bool focusNextPrevChild(bool) override { return false; }

	HOCRProofReadWidget* m_proofReadWidget = nullptr;
	const HOCRItem* m_wordItem = nullptr;
	bool m_blockSetText = false;

	QString getStyle(bool misspelled, bool enabled) const {
		QStringList styles;
		if(!enabled) {
			styles.append( "color: grey;" );
		} else if(misspelled) {
			styles.append( "color: red;" );
		}
		return styles.isEmpty() ? "" : QString("QLineEdit {%1}").arg(styles.join(" "));
	}

	void onTextChanged() {
		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());

		// Update data in document
		QModelIndex index = document->indexAtItem(m_wordItem);
		m_blockSetText = true;
		document->setData(index, text(), Qt::EditRole);
		m_blockSetText = false;
	}
	void onModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		QItemSelectionRange range(topLeft, bottomRight);
		QModelIndex index = document->indexAtItem(m_wordItem);
		if(range.contains(index)) {
			if(roles.contains(Qt::DisplayRole) && !m_blockSetText) {
				setText(m_wordItem->text());
			}
			if(roles.contains(Qt::ForegroundRole)) {
				setStyleSheet( getStyle( document->indexIsMisspelledWord(index), m_wordItem->isEnabled() ) );
			}
			if(roles.contains(Qt::CheckStateRole)) {
				// setEnabled doesn't do the right thing, unfortunately
				setReadOnly(!m_wordItem->isEnabled());
				setStyleSheet( getStyle( document->indexIsMisspelledWord(index), m_wordItem->isEnabled() ) );
			}
		}
	}
	void onAttributeChanged(const QModelIndex& index, const QString& name, const QString& /*value*/) {
		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		if(document->itemAtIndex(index) == m_wordItem) {
			if(name == "bold" || name == "italic" || name == "title:x_font" || name == "title:x_fsize") {
				QFont ft = m_wordItem->fontFamily();
				ft.setBold(m_wordItem->fontBold());
				ft.setItalic(m_wordItem->fontItalic());
				// Don't change the widget font size - be sure it remains readable 
				ft.setPointSize(font().pointSize());
				setFont(ft);
			} else if(name == "title:bbox") {
				QPoint sceneCorner = MAIN->getDisplayer()->getSceneBoundingRect().toRect().topLeft();
				QRect sceneBBox = m_wordItem->bbox().translated(sceneCorner);
				QPoint bottomLeft = MAIN->getDisplayer()->mapFromScene(sceneBBox.bottomLeft());
				QPoint bottomRight = MAIN->getDisplayer()->mapFromScene(sceneBBox.bottomRight());
				int frameX = parentWidget()->parentWidget()->parentWidget()->pos().x();
				move(bottomLeft.x() - frameX, 0);
				setFixedWidth(bottomRight.x() - bottomLeft.x() + 8); // 8: border + padding
			}
		}
	}
	typedef enum {OCR_none, OCR_page, OCR_carea, OCR_par, OCR_line, OCRX_word} ClassOrdinal;
	ClassOrdinal classNumber(const HOCRItem* item) {
		if (m_wordItem->itemClass() == "ocrx_word") return OCRX_word;
		if (m_wordItem->itemClass() == "ocr_line") return OCR_line;
		if (m_wordItem->itemClass() == "ocr_par") return OCR_par;
		if (m_wordItem->itemClass() == "ocr_carea") return OCR_carea;
		if (m_wordItem->itemClass() == "ocr_page") return OCR_page;
		return OCR_none;
	}
	void moveToClass(ClassOrdinal target) {
		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		ClassOrdinal depth = classNumber(m_wordItem);
		if (depth > target) {
			QModelIndex newIndex = document->indexAtItem(m_wordItem->parent());
			for (int d = depth-1; d > target; d--) {
				newIndex = newIndex.parent();
			}
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			m_proofReadWidget->repositionWidget();
		} else if (depth != target) {
			QModelIndex index = document->indexAtItem(m_wordItem);
			QModelIndex newIndex = document->prevOrNextIndex(true, index, classNames[target]);
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			m_proofReadWidget->repositionWidget();
		}
	}
	void keyPressEvent(QKeyEvent* ev) override {
		if(ev->modifiers() == Qt::KeypadModifier && ev->key() == Qt::Key_Enter) {
			if(ev->isAutoRepeat()) {
				return;
			}
			static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->showPreview(true);
		}

		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());

		bool atWord = m_wordItem->itemClass() == "ocrx_word";
		enum actions {none, prevLine, prevWhole, nextLine, beginCurrent, nextWord, prevWord} action = actions::none;

		if(ev->modifiers() == Qt::NoModifier && ev->key() == Qt::Key_Down) {
			action = nextLine;
		}
		else if(ev->modifiers() == Qt::NoModifier && ev->key() == Qt::Key_Up) {
		    action = atWord ? prevLine : prevWhole;
		}
		else if(ev->key() == Qt::Key_Tab) {
		    if(atWord) {
			    if(m_wordItem == m_wordItem->parent()->children().last()) {
					action = nextLine;
				} else {
					action = nextWord;
				}
		    } else {
				action = beginCurrent;
		    }
		}
		else if(ev->key() == Qt::Key_Backtab) {
		    if(atWord) {
			    if(m_wordItem == m_wordItem->parent()->children().first()) {
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
			QModelIndex index = document->indexAtItem(m_wordItem);
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
				const HOCRItem* item = m_wordItem;
				while (item->children().size() > 0 && item->itemClass() != "ocrx_word") {
					item = item->children().first();
				}
				index = document->indexAtItem(item);
				break;
			} 
			}
			m_proofReadWidget->documentTree()->setCurrentIndex(index);
		} else if(ev->key() == Qt::Key_Space && ev->modifiers() == Qt::ControlModifier) {
			// Spelling menu
			QModelIndex index = document->indexAtItem(m_wordItem);
			QMenu menu;
			document->addSpellingActions(&menu, index);
			menu.exec(mapToGlobal(QPoint(0, -menu.sizeHint().height())));
		} else if((ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return) && ev->modifiers() & Qt::ControlModifier) {
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->addWordToDictionary(index);
		} else if(ev->key() == Qt::Key_B && ev->modifiers() == Qt::ControlModifier) {
			// Bold
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->editItemAttribute(index, "bold", m_wordItem->fontBold() ? "0" : "1");
		} else if(ev->key() == Qt::Key_I && ev->modifiers() == Qt::ControlModifier) {
			// Italic
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->editItemAttribute(index, "italic", m_wordItem->fontItalic() ? "0" : "1");
		} else if(ev->key() == Qt::Key_T && ev->modifiers() == Qt::ControlModifier) {
			// Trim
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->fitToFont(index);
		} else if((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down) && ev->modifiers() & Qt::ControlModifier) {
			// Adjust bbox top/bottom
			QModelIndex index = document->indexAtItem(m_wordItem);
			QRect bbox = m_wordItem->bbox();
			if(ev->modifiers() & Qt::ShiftModifier) {
				bbox.setBottom(bbox.bottom() + (ev->key() == Qt::Key_Up ? -1 : 1));
			} else {
				bbox.setTop(bbox.top() + (ev->key() == Qt::Key_Up ? -1 : 1));
			}
			QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
			document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if((ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Right) && ev->modifiers() & Qt::ControlModifier) {
			// Adjust bbox left/right
			QModelIndex index = document->indexAtItem(m_wordItem);
			QRect bbox = m_wordItem->bbox();
			if(ev->modifiers() & Qt::ShiftModifier) {
				bbox.setRight(bbox.right() + (ev->key() == Qt::Key_Left ? -1 : 1));
			} else {
				bbox.setLeft(bbox.left() + (ev->key() == Qt::Key_Left ? -1 : 1));
			}
			QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
			document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down || ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Right) && ev->modifiers() & Qt::AltModifier) {
			// Move bbox 
			QModelIndex index;
			if ((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down) && m_wordItem->itemClass() == "ocrx_word") {
				m_wordItem = m_wordItem->parent();
				index = document->indexAtItem(m_wordItem);
				m_proofReadWidget->documentTree()->setCurrentIndex(index);
			} else {
				index = document->indexAtItem(m_wordItem);
			}

			switch(ev->key()) {
			case Qt::Key_Up:
				document->xlateItem(index, -1, 0);
				break;
			case Qt::Key_Down:
				document->xlateItem(index, +1, 0);
				break;
			case Qt::Key_Left: 
				document->xlateItem(index, 0, -1);
				break;
			case Qt::Key_Right: 
				document->xlateItem(index, 0, +1);
				break;
			}
		} else if(ev->key() == Qt::Key_W && ev->modifiers() == Qt::ControlModifier) {
			// Add word
			QPoint p = QCursor::pos();
			static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->addWordAtCursor();
		} else if(ev->key() == Qt::Key_D && ev->modifiers() == Qt::ControlModifier) {
			// Divide
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->splitItemText(index, cursorPosition());
		} else if(ev->key() == Qt::Key_M && ev->modifiers() & Qt::ControlModifier) {
			// Merge
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->mergeItemText(index, (ev->modifiers() & Qt::ShiftModifier) != 0);
		} else if(ev->key() == Qt::Key_Delete && ev->modifiers() == (Qt::ControlModifier|Qt::ShiftModifier)) {
			QPersistentModelIndex index = document->indexAtItem(m_wordItem);
			int currPage = m_wordItem->page()->pageNr();
            QPersistentModelIndex newIndex = document->prevOrNextIndex(true, index, "ocrx_word");
			if (document->itemAtIndex(newIndex)->page()->pageNr() != currPage) {
				newIndex = document->prevOrNextIndex(false, index, "ocrx_word");
			}
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			document->removeItem(index); // must be last!
		} else if(ev->key() == Qt::Key_Delete && ev->modifiers() == Qt::ControlModifier) {
			QModelIndex index = document->indexAtItem(m_wordItem);
			document->toggleEnabledCheckbox(index);
		} else if(ev->key() == Qt::Key_1 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCRX_word);
		} else if(ev->key() == Qt::Key_2 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_line);
		} else if(ev->key() == Qt::Key_3 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_par);
		} else if(ev->key() == Qt::Key_4 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_carea);
		} else if(ev->key() == Qt::Key_5 && ev->modifiers() & Qt::KeypadModifier) {
			QModelIndex newIndex = document->indexAtItem(m_wordItem->page());
			HOCRProofReadWidget* widget = m_proofReadWidget; // some ops change 'this', and we need the widget after
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			widget->repositionWidget();
		} else if(ev->key() == Qt::Key_Plus && ev->modifiers() & Qt::ControlModifier) {
			m_proofReadWidget->adjustFontSize(+1);
		} else if((ev->key() == Qt::Key_Minus || ev->key() == Qt::Key_Underscore) && ev->modifiers() & Qt::ControlModifier) {
			m_proofReadWidget->adjustFontSize(-1);
		} else {
			if(m_wordItem->itemClass() == "ocrx_word") {
				// Tab/Backtab here just goes to the next widget (the next bbox!) by default,
				// which ultmitately reaches showItemProperties. See also the top of this function.
				QLineEdit::keyPressEvent(ev);
			} else {
				MAIN->getDisplayer()->keyPressEvent(ev);
			}
		}
	}
	void keyReleaseEvent(QKeyEvent* ev) override {
		if(ev->modifiers() == Qt::KeypadModifier && ev->key() == Qt::Key_Enter) {
			if(ev->isAutoRepeat()) {
				return;
			}
			static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->showPreview(false);
		}
		QLineEdit::keyReleaseEvent(ev);
	}
	void mousePressEvent(QMouseEvent* ev) override {
		HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		m_proofReadWidget->documentTree()->setCurrentIndex(document->indexAtItem(m_wordItem));
		QLineEdit::mousePressEvent(ev);
	}
	void focusInEvent(QFocusEvent* ev) override {
		m_proofReadWidget->show();
		if(m_wordItem != nullptr && m_wordItem->itemClass() == "ocrx_word") {
			HOCRDocument* document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
			// Don't setCurrentIndex (don't clear multiple select)
			QMap<QString,QString> attrs = m_wordItem->getTitleAttributes();
			QMap<QString,QString>::iterator i = attrs.find("x_wconf");
			if(i != attrs.end()) {
				m_proofReadWidget->setConfidenceLabel(i.value().toInt());
			}
		}
		QLineEdit::focusInEvent(ev);
		if(ev->reason() != Qt::MouseFocusReason) {
			deselect();
			setCursorPosition(0);
		}
	}
};


HOCRProofReadWidget::HOCRProofReadWidget(TreeViewHOCR* treeView, QWidget* parent)
	: QFrame(parent), m_treeView(treeView) {
	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(2, 2, 2, 2);
	layout->setSpacing(2);
	setLayout(layout);

	m_stub = new LineEdit(this,this);

	QWidget* linesWidget = new QWidget();
	m_linesLayout = new QVBoxLayout();
	m_linesLayout->setContentsMargins(0, 0, 0, 0);
	m_linesLayout->setSpacing(0);
	linesWidget->setLayout(m_linesLayout);
	layout->addWidget(linesWidget);

	m_controlsWidget = new QWidget();
	m_controlsWidget->setLayout(new QHBoxLayout);
	m_controlsWidget->layout()->setSpacing(2);
	m_controlsWidget->layout()->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_controlsWidget);

	QFont smallFont;
	smallFont.setPointSizeF(0.8 * smallFont.pointSizeF());

	m_confidenceLabel = new QLabel();
	m_confidenceLabel->setFont(smallFont);
	m_controlsWidget->layout()->addWidget(m_confidenceLabel);

	m_controlsWidget->layout()->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

	QLabel* helpButton = new QLabel(QString("<a href=\"#help\">%1</a>").arg(_("Keyboard shortcuts")));
	helpButton->setFont(smallFont);
	connect(helpButton, &QLabel::linkActivated, this, &HOCRProofReadWidget::showShortcutsDialog);
	m_controlsWidget->layout()->addWidget(helpButton);


	QMenu* settingsMenu = new QMenu();

	QWidget* numBeforeWidget = new QWidget();
	numBeforeWidget->setLayout(new QHBoxLayout());
	numBeforeWidget->layout()->setContentsMargins(4, 4, 4, 4);
	numBeforeWidget->layout()->setSpacing(2);
	numBeforeWidget->layout()->addWidget(new QLabel(_("Lines before:")));

	m_spinLinesBefore = new QSpinBox();
	m_spinLinesBefore->setRange(0, 10);
	numBeforeWidget->layout()->addWidget(m_spinLinesBefore);

	QWidgetAction* numBeforeAction = new QWidgetAction(settingsMenu);
	numBeforeAction->setDefaultWidget(numBeforeWidget);
	settingsMenu->addAction(numBeforeAction);

	QWidget* numAfterWidget = new QWidget();
	numAfterWidget->setLayout(new QHBoxLayout());
	numAfterWidget->layout()->setContentsMargins(4, 4, 4, 4);
	numAfterWidget->layout()->setSpacing(2);
	numAfterWidget->layout()->addWidget(new QLabel(_("Lines after:")));

	m_spinLinesAfter = new QSpinBox();
	m_spinLinesAfter->setRange(0, 10);
	numAfterWidget->layout()->addWidget(m_spinLinesAfter);

	QWidgetAction* numAfterAction = new QWidgetAction(settingsMenu);
	numAfterAction->setDefaultWidget(numAfterWidget);
	settingsMenu->addAction(numAfterAction);

	QToolButton* settingsButton = new QToolButton();
	settingsButton->setAutoRaise(true);
	settingsButton->setIcon(QIcon::fromTheme("preferences-system"));
	settingsButton->setPopupMode(QToolButton::InstantPopup);
	settingsButton->setMenu(settingsMenu);
	m_controlsWidget->layout()->addWidget(settingsButton);

	setObjectName("proofReadWidget");
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setAutoFillBackground(true);

	setStyleSheet("QLineEdit { border: 1px solid #ddd; }");

	connect(m_treeView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] { updateWidget(); });

	HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());

	// Clear for rebuild if structure changes
	connect(document, &HOCRDocument::rowsAboutToBeRemoved, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsAboutToBeInserted, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsAboutToBeMoved, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsRemoved, this, [this] { updateWidget(); });
	connect(document, &HOCRDocument::rowsInserted, this, [this] { updateWidget(); });
	connect(document, &HOCRDocument::rowsMoved, this, [this] { updateWidget(); });
	connect(MAIN->getDisplayer(), &Displayer::imageChanged, this, [this] { updateWidget(true); });
	connect(MAIN->getDisplayer(), &Displayer::viewportChanged, this, &HOCRProofReadWidget::repositionWidget);
	connect(m_spinLinesBefore, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateWidget(true); });
	connect(m_spinLinesAfter, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateWidget(true); });

	ADD_SETTING(SpinSetting("proofReadLinesBefore", m_spinLinesBefore, 1));
	ADD_SETTING(SpinSetting("proofReadLinesAfter", m_spinLinesAfter, 1));

	// Start hidden
	hide();
}

void HOCRProofReadWidget::setProofreadEnabled(bool enabled) {
	m_enabled = enabled;
	if (enabled) {
		repositionWidget();
	} else {
		hide();
	}
}

void HOCRProofReadWidget::clear() {
	qDeleteAll(m_currentLines);
	m_currentLines.clear();
	m_currentLine = nullptr;
	m_confidenceLabel->setText("");
	m_confidenceLabel->setStyleSheet("");
	if (MAIN->focusWidget() == nullptr) {
		// We might have just deleted the focus item (see end of updateWidget)
		// so focus on the displayer. (Happens with page down key into unscanned window.)
		MAIN->getDisplayer()->setFocus();
	}
	hide();
}

void HOCRProofReadWidget::updateWidget(bool force) {
	QModelIndex current = m_treeView->currentIndex();
	int nrLinesBefore = m_spinLinesBefore->value();
	int nrLinesAfter = m_spinLinesAfter->value();

	HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());
	const HOCRItem* item = document->itemAtIndex(current);
	if(!item) {
		clear();
		return;
	}
	int page = -1;
	if (item->page()->sourceFile() != MAIN->getDisplayer()->getCurrentImage(page) || page != item->page()->pageNr()) {
		clear();
		return;
	}
	const HOCRItem* lineItem = nullptr;
	const HOCRItem* wordItem = nullptr;
	if(item->itemClass() == "ocrx_word") {
		lineItem = item->parent();
		wordItem = item;
		MAIN->getDisplayer()->setFocusProxy(nullptr);
	} else {
		m_stub->setStubItem(item);
		if(item->itemClass() != "ocr_line") {
			// Build a mini-widget telling the user he can't edit with that selected.
			clear();
			m_currentLine = item; // for reposition
			repositionWidget();	
			MAIN->getDisplayer()->setFocusProxy(m_stub);
			show();
			return;
		}
		lineItem = item;
	}

	const QVector<HOCRItem*>& siblings = lineItem->parent()->children();
	int targetLine = lineItem->index();
	if(lineItem != m_currentLine || force) {
		// Rebuild widget
		QMap<const HOCRItem*, QWidget*> newLines;
		int insPos = 0;
		for(int i = qMax(0, targetLine - nrLinesBefore), j = qMin(siblings.size() - 1, targetLine + nrLinesAfter); i <= j; ++i) {
			HOCRItem* linei = siblings[i];
			if(m_currentLines.contains(linei)) {
				newLines[linei] = m_currentLines.take(linei);
				insPos = m_linesLayout->indexOf(newLines[linei]) + 1;
			} else {
				QWidget* lineWidget = new QWidget();
				for(HOCRItem* word : siblings[i]->children()) {
					new LineEdit(this, word, lineWidget); // Add as child to lineWidget
				}
				m_linesLayout->insertWidget(insPos++, lineWidget);
				newLines.insert(linei, lineWidget);
			}
		}
		qDeleteAll(m_currentLines);
		m_currentLines = newLines;
		m_currentLine = lineItem;
		repositionWidget();
	}

	LineEdit* focusLineEdit;
	if(item->itemClass() == "ocr_line") {
		focusLineEdit = m_stub;
	} else {
		// Select selected word or first item of middle line
		const QObjectList children = m_currentLines[lineItem]->children();
		focusLineEdit = static_cast<LineEdit*>(children.size() > 0 ? (children[wordItem ? wordItem->index() : 0]) : m_currentLines[lineItem]);
	}
	if(focusLineEdit) {
		MAIN->getDisplayer()->setFocusProxy(focusLineEdit);
		if(!m_treeView->hasFocus()) {
			focusLineEdit->setFocus();
		}
	}
}

void HOCRProofReadWidget::repositionWidget() {

	if(m_currentLine == nullptr) {
		return;
	}

	// Position frame
	Displayer* displayer = MAIN->getDisplayer();
	int frameXmin = std::numeric_limits<int>::max();
	int frameXmax = 0;
	QPoint sceneCorner = displayer->getSceneBoundingRect().toRect().topLeft();
	if(m_currentLines.isEmpty()) {
		frameXmin = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).bottomLeft()).x();
		frameXmax = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).bottomRight()).x() + 8;
	} 
	for(QWidget* lineWidget : m_currentLines) {
		if(lineWidget->children().isEmpty()) {
			continue;
		}
		// First word
		LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->children()[0]);
		QPoint bottomLeft = displayer->mapFromScene(lineEdit->item()->bbox().translated(sceneCorner).bottomLeft());
		frameXmin = std::min(frameXmin, bottomLeft.x()-4); // 4:padding
	}
	QPoint bottomLeft = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).bottomLeft());
	QPoint topLeft = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).topLeft());
	int frameY = bottomLeft.y();

	// Recompute font sizes so that text matches original as closely as possible
	QFont ft = font();
	double avgFactor = 0.0;
	int nFactors = 0;
	// First pass: min scaling factor, move to correct location
	for(QWidget* lineWidget : m_currentLines) {
		for(int i = 0, n = lineWidget->children().count(); i < n; ++i) {
			LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->children()[i]);
			QRect sceneBBox = lineEdit->item()->bbox().translated(sceneCorner);
			QPoint bottomLeft = displayer->mapFromScene(sceneBBox.bottomLeft());
			QPoint bottomRight = displayer->mapFromScene(sceneBBox.bottomRight());
			// Factor weighted by length
			QFont actualFont = lineEdit->item()->fontFamily();
			QFontMetrics fm(actualFont);
			double factor = (bottomRight.x() - bottomLeft.x()) / double(fm.horizontalAdvance(lineEdit->text()));
			avgFactor += lineEdit->text().length() * factor;
			nFactors += lineEdit->text().length();

			lineEdit->move(bottomLeft.x() - frameXmin - 4, 0); // 4: allow for cursor at left
			lineEdit->setFixedWidth(bottomRight.x() - bottomLeft.x() + 8); // 8: border + padding
			frameXmax = std::max(frameXmax, bottomRight.x() + 8);
		}
	}
	avgFactor = avgFactor > 0 ? avgFactor / nFactors : 1.;

	// Second pass: apply font sizes, set line heights
	ft.setPointSizeF(ft.pointSizeF() * avgFactor);
	ft.setPointSizeF(ft.pointSizeF() + m_fontSizeDiff);
	QFontMetrics fm = QFontMetrics(ft);
	for(QWidget* lineWidget : m_currentLines) {
		for(int i = 0, n = lineWidget->children().count(); i < n; ++i) {
			LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->children()[i]);
			QFont lineEditFont = lineEdit->font();
			lineEditFont.setPointSizeF(ft.pointSizeF());
			lineEdit->setFont(lineEditFont);
			lineEdit->setFixedHeight(fm.height() + 5);
		}
		lineWidget->setFixedHeight(fm.height() + 10);
	}
	frameY += 10;
	updateGeometry();
	resize(frameXmax - frameXmin + 2 + 2 * layout()->spacing(), m_currentLines.size() * (fm.height() + 10) + 2 * layout()->spacing() + m_controlsWidget->sizeHint().height());

	// Place widget above line if it overflows page
	QModelIndex current = m_treeView->currentIndex();
	HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());
	const HOCRItem* item = document->itemAtIndex(current);
	QRect sceneBBox = item->page()->bbox().translated(sceneCorner);
	double maxy = displayer->mapFromScene(sceneBBox.bottomLeft()).y();
	if(frameY + height() - maxy > 0) {
		frameY = topLeft.y() - height();
	}

	move(frameXmin - layout()->spacing(), frameY);
	show();
}

void HOCRProofReadWidget::showShortcutsDialog() {
	QString text = QString(_(
	                           "<table>"
	                           "<tr><td>Tab, Shift-Tab</td>"          "<td>D</td> <td> </td> <td>T&nbsp;&nbsp;&nbsp;</td> <td>Next/Prev field</td></tr>"
	                           "<tr><td>Up, Down</td>"                "<td>D</td> <td>T</td> <td>E</td> <td>Previous/Next line</td></tr>"
	                           "<tr><td>Ctrl+Space</td>"              "<td> </td> <td> </td> <td>E</td> <td>Spelling suggestions</td></tr>"
	                           "<tr><td>Ctrl+Enter</td>"              "<td> </td> <td> </td> <td>E</td> <td>Add word to dictionary</td></tr>"
	                           "<tr><td>Ctrl+B</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Toggle bold</td></tr>"
	                           "<tr><td>Ctrl+I</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Toggle italic</td></tr>"
	                           "<tr><td>Ctrl+D</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Divide word at cursor position</td></tr>"
	                           "<tr><td>Ctrl+M</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Merge with previous word</td></tr>"
	                           "<tr><td>Ctrl+Shift+M</td>"            "<td> </td> <td> </td> <td>E</td> <td>Merge with next word</td></tr>"
	                           "<tr><td>Ctrl+W</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Insert new word/line at cursor</td></tr>"
	                           "<tr><td>Ctrl+T</td>"                  "<td>D</td> <td> </td> <td>E</td> <td>Trim word height (heuristic)</td></tr>"
	                           "<tr><td>Delete</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Delete current character</td></tr>"
	                           "<tr><td>Ctrl+Delete</td>"             "<td>D</td> <td>T</td> <td>E</td> <td>Toggle Disable current item</td></tr>"
	                           "<tr><td>Ctrl+Shift+Delete</td>"       "<td>D</td> <td>T</td> <td>E</td> <td>(Hard) delete current item</td></tr>"
	                           "<tr><td>Ctrl+{Left,Right}</td>"       "<td> </td> <td> </td> <td>E</td> <td>Adjust left bounding box edge</td></tr>"
	                           "<tr><td>Ctrl+Shift+{Left,Right}</td>" "<td> </td> <td> </td> <td>E</td> <td>Adjust right bounding box edge</td></tr>"
	                           "<tr><td>Ctrl+{Up,Down}</td>"          "<td> </td> <td> </td> <td>E</td> <td>Adjust top bounding box edge</td></tr>"
	                           "<tr><td>Ctrl+Shift+{Up,Down}</td>"    "<td> </td> <td> </td> <td>E</td> <td>Adjust bottom bounding box edge</td></tr>"
	                           "<tr><td>Ctrl++</td>"                  "<td>D</td> <td> </td> <td>E</td> <td>Increase <em>tool</em> font size</td></tr>"
	                           "<tr><td>Ctrl+-</td>"                  "<td>D</td> <td> </td> <td>E</td> <td>Decrease <em>tool</em> font size</td></tr>"
	                           "<tr><td>Alt+{Left,Right,Up,Down}</td>""<td>D</td> <td> </td> <td> </td> <td>Move item (vertical moves whole lines)</td></tr>"
	                           "<tr><td>PageUp, PageDown</td>"        "<td>D</td> <td> </td> <td>E</td> <td>Previous/Next Page</td></tr>"
	                           "<tr><td>PageUp, PageDown</td>"        "<td> </td> <td>T</td> <td> </td> <td>Up/down one table screen</td></tr>"
	                           "<tr><td>Keypad+{1-5}</td>"            "<td> </td> <td>T</td> <td> </td> <td>Nearest word,line,para,section,page</td></tr>"
	                           "<tr><td>Ctrl+F</td>"                  "<td>D</td> <td>T</td> <td>F</td> <td>Open/Go to Find</td></tr>"
	                           "<tr><td>Ctrl+S</td>"                  "<td>D</td> <td>T</td> <td>F</td> <td>Open Save HOCR</td></tr>"
	                           "<tr><td>F3, Shift+F3</td>"            "<td>D</td> <td>T</td> <td>F</td> <td>Next/Prev Page/Paragraph/Line in Tree</td></tr>"
	                           "<tr><td></td>"                        "<td> </td> <td> </td> <td> </td> <td>(see Output dropdown)</td></tr>"
	                           "<tr><td>Enter+Keypad (hold)</td>"     "<td>D</td> <td> </td> <td> </td> <td>Toggle preview state</td></tr>"
	                           "<tr><td>&lt;print&gt;</td>"           "<td> </td> <td> </td> <td>E</td> <td>Insert the character</td></tr>"
	                           "<tr><td>&lt;print&gt;</td>"           "<td>D</td> <td>T</td> <td> </td> <td>Search to item beginning with &lt;print&gt;</td></tr>"
							   "<tr><td>L-Click</td>"                 "<td>D</td> <td>T</td> <td>E</td> <td>Select</td></tr>"
							   "<tr><td>L-2Click</td>"                "<td> </td> <td>T</td> <td>E</td> <td>Expand/Open for edit</td></tr>"
							   "<tr><td>Shift+L-Click</td>"           "<td>D</td> <td> </td> <td> </td> <td>Select/toggle Enclosing HOCR</td></tr>"
							   "<tr><td>Ctrl+L-Click</td>"            "<td>D</td> <td> </td> <td> </td> <td>Multi-Select/toggle</td></tr>"
							   "<tr><td>Ctrl+Shift+L-Click</td>"      "<td>D</td> <td> </td> <td> </td> <td>Multi-Select/toggle Enclosing HOCR</td></tr>"
							   "<tr><td>R-Click</td>"                 "<td> </td> <td>T</td> <td> </td> <td>Open context menu</td></tr>"
							   "<tr><td>M-Mouse Drag</td>"            "<td>D</td> <td> </td> <td> </td> <td>Pan (when zoomed)</td></tr>"
							   "<tr><td>L-Mouse Drag Box Edge</td>"   "<td>D</td> <td> </td> <td> </td> <td>Resize Box</td></tr>"
							   "<tr><td>L-Mouse Drag Box Center</td>" "<td>D</td> <td> </td> <td> </td> <td>Move Box (when all-cursor shows)</td></tr>"
							   "<tr><td>L-Mouse Drag Other</td>"      "<td>D</td> <td> </td> <td> </td> <td>Pan (when zoomed)</td></tr>"
							   "<tr><td>Wheel</td>"                   "<td>D</td> <td> </td> <td> </td> <td>Pan Up/Down</td></tr>"
							   "<tr><td>Shift+Wheel</td>"             "<td>D</td> <td> </td> <td> </td> <td>Pan Left/Right</td></tr>"
							   "<tr><td>Ctrl+Wheel</td>"              "<td>D</td> <td> </td> <td> </td> <td>Zoom (around position)</td></tr>"
	                           "</table>"
							   "<p>"
							   "D = in Display window; T = in Table window; E = Text Edit active"
							   "</p>"
	                       ));
	QMessageBox* box = new QMessageBox(QMessageBox::NoIcon, _("Keyboard Shortcuts"), text, QMessageBox::Close, MAIN);
	box->setAttribute(Qt::WA_DeleteOnClose, true);
	box->setModal(false);
	box->show();
	box->setFocusPolicy(Qt::NoFocus);
}

QString HOCRProofReadWidget::confidenceStyle(int wconf) const {
	if(wconf < 70) {
		return "background: #ffb2b2;";
	} else if(wconf < 80) {
		return "background: #ffdab0;";
	} else if(wconf < 90) {
		return "background: #fffdb4;";
	}
	return QString();
}

void HOCRProofReadWidget::setConfidenceLabel(int wconf) {
	m_confidenceLabel->setText(_("Confidence: %1").arg(wconf));
	QString style = confidenceStyle(wconf);
	m_confidenceLabel->setStyleSheet(!style.isEmpty() ? QString("QLabel { %1 }").arg(style) : "");
}

void HOCRProofReadWidget::adjustFontSize(int diff) {
	m_fontSizeDiff += diff;
	repositionWidget();
}
