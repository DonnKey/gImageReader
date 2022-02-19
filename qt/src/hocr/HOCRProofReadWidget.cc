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
#include <QPointer>
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

		setReadOnly(!m_wordItem->isEnabled());
		setStyle(document);
		home(false);

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
	void setStyle(const HOCRDocument *document, bool current = false) {
		QModelIndex index = document->indexAtItem(m_wordItem);
		setStyleSheet( getStyle( document->indexIsMisspelledWord(index), m_wordItem->isEnabled(), current ) );
	}

public:
	int m_computedWidth = 0;

private:
	// Disable auto tab handling
	bool focusNextPrevChild(bool) override { return false; }

	HOCRProofReadWidget* m_proofReadWidget = nullptr;
	QPointer<const HOCRItem> m_wordItem = nullptr;
	bool m_blockSetText = false;
	static int m_savedCursor;
	static const HOCRItem* m_currentEditedItem;
	HOCRDocument* m_document;

	QString getStyle(bool misspelled, bool enabled, bool current) const {
		QStringList styles;
		if(!enabled) {
			styles.append( "color: grey;" );
		} else if(misspelled) {
			styles.append( "color: red;" );
		}
		if (current) {
			styles.append("background-color: rgba(255,255,255,164);");
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
		m_proofReadWidget->repositionPointer();
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
				setStyle(document);
			}
			if(roles.contains(Qt::CheckStateRole)) {
				// setEnabled doesn't do the right thing, unfortunately
				setReadOnly(!m_wordItem->isEnabled());
				setStyle(document);
			}
			m_proofReadWidget->repositionWidget();
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
			m_proofReadWidget->repositionPointer();
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
		ClassOrdinal depth = classNumber(m_wordItem);
		if (depth > target) {
			QModelIndex newIndex = m_document->indexAtItem(m_wordItem->parent());
			for (int d = depth-1; d > target; d--) {
				newIndex = newIndex.parent();
			}
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			m_proofReadWidget->repositionWidget();
		} else if (depth != target) {
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			QModelIndex newIndex = m_document->prevOrNextIndex(true, index, classNames[target]);
			m_proofReadWidget->documentTree()->setCurrentIndex(newIndex);
			m_proofReadWidget->repositionWidget();
		}
	}
	void keyPressEvent(QKeyEvent* ev) override {
		if (m_wordItem == nullptr) {
			MAIN->getDisplayer()->keyPressEvent(ev);
			return;
		}

		m_document = static_cast<HOCRDocument*>(m_proofReadWidget->documentTree()->model());
		HOCRProofReadWidget* widget = m_proofReadWidget; // some ops change 'this', and we need the widget after

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
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			switch(action) {
			case nextLine:
				// Move to first word of next line
				index = m_document->prevOrNextIndex(true, index, "ocr_line");
				index = m_document->prevOrNextIndex(true, index, "ocrx_word");
				break;
			case prevLine:
				// Move to last word of prev line (from a word within this line)
				index = m_document->prevOrNextIndex(false, index, "ocr_line");
				index = m_document->prevOrNextIndex(false, index, "ocrx_word");
				break;
			case prevWhole:
				// Move to last word of prev line (from something enclosing)
			case prevWord:
				index = m_document->prevOrNextIndex(false, index, "ocrx_word");
				break;
			case nextWord:
				index = m_document->prevOrNextIndex(true, index, "ocrx_word");
				break;
			case beginCurrent: {
				// Move to first word below any parent items.
				const HOCRItem* item = m_wordItem;
				while (item->children().size() > 0 && item->itemClass() != "ocrx_word") {
					item = item->children().first();
				}
				index = m_document->indexAtItem(item);
				break;
			} 
			}
			widget->documentTree()->setCurrentIndex(index);
			widget->repositionWidget();
		} else if(ev->key() == Qt::Key_Space && ev->modifiers() == Qt::ControlModifier) {
			// Spelling menu
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			QMenu menu;
			m_document->addSpellingActions(&menu, index);
			menu.exec(mapToGlobal(QPoint(0, -menu.sizeHint().height())));
		} else if((ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return) && ev->modifiers() & Qt::ControlModifier) {
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->addWordToDictionary(index);
		} else if(ev->key() == Qt::Key_B && ev->modifiers() == Qt::ControlModifier) {
			// Bold
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->editItemAttribute(index, "bold", m_wordItem->fontBold() ? "0" : "1");
		} else if(ev->key() == Qt::Key_I && ev->modifiers() == Qt::ControlModifier) {
			// Italic
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->editItemAttribute(index, "italic", m_wordItem->fontItalic() ? "0" : "1");
		} else if(ev->key() == Qt::Key_T && ev->modifiers() == Qt::ControlModifier) {
			// Trim
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->fitToFont(index);
		} else if((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down) && ev->modifiers() & Qt::ControlModifier) {
			// Adjust bbox top/bottom
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			QRect bbox = m_wordItem->bbox();
			if(ev->modifiers() & Qt::ShiftModifier) {
				bbox.setBottom(bbox.bottom() + (ev->key() == Qt::Key_Up ? -1 : 1));
			} else {
				bbox.setTop(bbox.top() + (ev->key() == Qt::Key_Up ? -1 : 1));
			}
			QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
			m_document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if((ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Right) && ev->modifiers() & Qt::ControlModifier) {
			// Adjust bbox left/right
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			QRect bbox = m_wordItem->bbox();
			if(ev->modifiers() & Qt::ShiftModifier) {
				bbox.setRight(bbox.right() + (ev->key() == Qt::Key_Left ? -1 : 1));
			} else {
				bbox.setLeft(bbox.left() + (ev->key() == Qt::Key_Left ? -1 : 1));
			}
			QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
			m_document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down || ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Right) && ev->modifiers() & Qt::AltModifier) {
			// Move bbox 
			QModelIndex index;
			if ((ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down) && m_wordItem->itemClass() == "ocrx_word") {
				HOCRItem* parent = m_wordItem->parent();
				index = m_document->indexAtItem(parent);
				widget->documentTree()->setCurrentIndex(index);
			} else {
				index = m_document->indexAtItem(m_wordItem);
			}

			switch(ev->key()) {
			case Qt::Key_Up:
				m_document->xlateItem(index, -1, 0);
				break;
			case Qt::Key_Down:
				m_document->xlateItem(index, +1, 0);
				break;
			case Qt::Key_Left: 
				m_document->xlateItem(index, 0, -1);
				break;
			case Qt::Key_Right:
				m_document->xlateItem(index, 0, +1);
				break;
			}
		} else if(ev->key() == Qt::Key_W && ev->modifiers() == Qt::ControlModifier) {
			// Add word
			QPoint p = QCursor::pos();
			static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->addWordAtCursor();
		} else if(ev->key() == Qt::Key_D && ev->modifiers() == Qt::ControlModifier) {
			// Divide
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->splitItemText(index, cursorPosition());
		} else if(ev->key() == Qt::Key_M && ev->modifiers() & Qt::ControlModifier) {
			// Merge
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->mergeItemText(index, (ev->modifiers() & Qt::ShiftModifier) != 0);
		} else if(ev->key() == Qt::Key_Underscore && ev->modifiers() & Qt::ControlModifier) {
			// Merge (right) with underscore
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->mergeItemText(index, true, "_");
		} else if(ev->key() == Qt::Key_Delete && ev->modifiers() == (Qt::ControlModifier|Qt::ShiftModifier)) {
			QPersistentModelIndex index = m_document->indexAtItem(m_wordItem);
			int currPage = m_wordItem->page()->pageNr();
            QPersistentModelIndex newIndex = m_document->prevOrNextIndex(true, index, "ocrx_word");
			if (m_document->itemAtIndex(newIndex)->page()->pageNr() != currPage) {
				newIndex = m_document->prevOrNextIndex(false, index, "ocrx_word");
			}
			widget->documentTree()->setCurrentIndex(newIndex);
			m_document->removeItem(index); // must be last!
		} else if(ev->key() == Qt::Key_Delete && ev->modifiers() == Qt::ControlModifier) {
			QModelIndex index = m_document->indexAtItem(m_wordItem);
			m_document->toggleEnabledCheckbox(index);
		} else if(ev->key() == Qt::Key_1 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCRX_word);
		} else if(ev->key() == Qt::Key_2 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_line);
		} else if(ev->key() == Qt::Key_3 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_par);
		} else if(ev->key() == Qt::Key_4 && ev->modifiers() & Qt::KeypadModifier) {
			moveToClass(OCR_carea);
		} else if(ev->key() == Qt::Key_5 && ev->modifiers() & Qt::KeypadModifier) {
			QModelIndex newIndex = m_document->indexAtItem(m_wordItem->page());
			widget->documentTree()->setCurrentIndex(newIndex);
			widget->repositionWidget();
		} else if(ev->key() == Qt::Key_Plus && ev->modifiers() & Qt::ControlModifier) {
			widget->adjustFontSize(+1);
		} else if((ev->key() == Qt::Key_Minus || ev->key() == Qt::Key_Underscore) && ev->modifiers() & Qt::ControlModifier) {
			widget->adjustFontSize(-1);
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
		if (m_currentEditedItem == m_wordItem) {
			// The editor might be recreated... the wordItem is unique
			setCursorPosition(m_savedCursor);
		}
	}
	void focusOutEvent(QFocusEvent* ev) override {
		m_currentEditedItem = m_wordItem;
		m_savedCursor = cursorPosition();
		QLineEdit::focusOutEvent(ev);
	}
};

const HOCRItem* HOCRProofReadWidget::LineEdit::m_currentEditedItem = nullptr;
int HOCRProofReadWidget::LineEdit::m_savedCursor = 0;

bool HOCRProofReadWidget::eventFilter(QObject* target, QEvent* ev) {
	if(ev->type() != QEvent::KeyPress && ev->type() != QEvent::KeyRelease) {
		return false;
	}
	QPoint p = QCursor::pos();
	OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());

	if (!MAIN->getDisplayer()->underMouse() && !m_treeView->underMouse()) {
		return false;
	}
	if (dynamic_cast<Displayer*>(target) == nullptr && dynamic_cast<TreeViewHOCR*>(target) == nullptr) {
		// for popups in the preview area
		return false;
	}

	QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
	if(kev->modifiers() == Qt::KeypadModifier && kev->key() == Qt::Key_Enter) {
		if(!kev->isAutoRepeat()) {
			editor->showPreview(
				kev->type() == QEvent::KeyPress 
				? OutputEditorHOCR::showMode::invert
				: OutputEditorHOCR::showMode::show);
		}
		return true;
	}
	return false;
}

class HOCRProofReadWidget::PointerWidget : public QWidget {
public:
	PointerWidget(QWidget* parent = nullptr) : QWidget(parent) {
		setAttribute(Qt::WA_TransparentForMouseEvents);
	}

	void triangle(QPointF p1, QPointF p2, QPointF at, int pageDpi) {
		const double minimum = 10*(pageDpi/100.);
		m_p1 = p1;
		m_p2 = p2;
		m_at = at;
		if(p2.x() < p1.x()) {
			m_p1 = p2;
			m_p2 = p1;
		}
		double diff = m_p2.x()-m_p1.x();
		if(diff < minimum) {
			QPointF t = QPointF((minimum-diff)/2.0,0);
			m_p1 -= t;
			m_p2 += t;
		}
	}
	void setWindow(QRectF wind) {
		m_wind = wind;
	}

private:
	void paintEvent(QPaintEvent* event) override {
		QPainter painter(this);

		painter.setWindow(m_wind.toRect());
		QColor c = QPalette().highlight().color();
		QColor c1 = QColor(c.red(), c.green(), c.blue(), 64);
		QColor c2 = QColor(c.red(), c.green(), c.blue(), 128);
		QLinearGradient gradient(QPoint(m_at.x(),m_p1.y()), m_at);
		gradient.setColorAt(0, c1);
		gradient.setColorAt(1, c2);
		QBrush brush(gradient);
		painter.setBrush(brush);
		painter.setRenderHint(QPainter::Antialiasing, true);
		QPolygonF pointer = QPolygonF({m_p1, m_p2, m_at});
		QPainterPath path;
		path.addPolygon(pointer);
		painter.fillPath(path, brush);
		painter.setRenderHint(QPainter::Antialiasing, false);
	}

	QPointF m_p1;
	QPointF m_p2;
	QPointF m_at;
	QRectF m_wind;
};

// Arbitrary constants affecting layout (and used in multiple places)
const int widgetMargins = 2;
const int framePadding = 4;
const int spinnerPadding = 4;
const int cursorPadding = 4; // extra space for the cursor
const int editBoxPadding = 2*cursorPadding;
const int editLineSpacing = 10;
const int clipMargin = 20;

HOCRProofReadWidget::HOCRProofReadWidget(TreeViewHOCR* treeView, QWidget* parent)
	: QFrame(parent), m_treeView(treeView) {
	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(widgetMargins, widgetMargins, widgetMargins, widgetMargins);
	layout->setSpacing(widgetMargins);
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
	m_controlsWidget->layout()->setSpacing(widgetMargins);
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
	numBeforeWidget->layout()->setContentsMargins(spinnerPadding, spinnerPadding, spinnerPadding, spinnerPadding);
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
	numAfterWidget->layout()->setContentsMargins(spinnerPadding, spinnerPadding, spinnerPadding, spinnerPadding);
	numAfterWidget->layout()->setSpacing(2);
	numAfterWidget->layout()->addWidget(new QLabel(_("Lines after:")));

	m_spinLinesAfter = new QSpinBox();
	m_spinLinesAfter->setRange(0, 10);
	numAfterWidget->layout()->addWidget(m_spinLinesAfter);

	QWidgetAction* numAfterAction = new QWidgetAction(settingsMenu);
	numAfterAction->setDefaultWidget(numAfterWidget);
	settingsMenu->addAction(numAfterAction);

	QWidget* gapWidget = new QWidget();
	gapWidget->setLayout(new QHBoxLayout());
	gapWidget->layout()->setContentsMargins(spinnerPadding, spinnerPadding, spinnerPadding, spinnerPadding);
	gapWidget->layout()->setSpacing(2);
	gapWidget->layout()->addWidget(new QLabel(_("Separation:")));

	m_gapWidth = new QSpinBox();
	m_gapWidth->setRange(0, 200);
	m_gapWidth->setSingleStep(10);
	gapWidget->layout()->addWidget(m_gapWidth);

	QWidgetAction* gapAction = new QWidgetAction(settingsMenu);
	gapAction->setDefaultWidget(gapWidget);
	settingsMenu->addAction(gapAction);

	QToolButton* settingsButton = new QToolButton();
	settingsButton->setAutoRaise(true);
	settingsButton->setIcon(QIcon::fromTheme("preferences-system"));
	settingsButton->setPopupMode(QToolButton::InstantPopup);
	settingsButton->setMenu(settingsMenu);
	m_controlsWidget->layout()->addWidget(settingsButton);

	m_pointer = new PointerWidget(parent);
	m_lineMap = new LineMap;
	m_updateTimer.setSingleShot(true);
	m_widgetTimer.setSingleShot(true);
	m_pointerTimer.setSingleShot(true);

	setObjectName("proofReadWidget");
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setAutoFillBackground(true);

	setStyleSheet("QLineEdit { border: 1px solid #ddd; }");

	connect(m_treeView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] { updateWidget(true); });

	HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());

	// Clear for rebuild if structure changes
	connect(document, &HOCRDocument::rowsAboutToBeRemoved, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsAboutToBeInserted, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsAboutToBeMoved, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::layoutAboutToBeChanged, this, &HOCRProofReadWidget::clear);
	connect(document, &HOCRDocument::rowsRemoved, this, [this] { updateWidget(true); });
	connect(document, &HOCRDocument::rowsInserted, this, [this] { updateWidget(true); });
	connect(document, &HOCRDocument::rowsMoved, this, [this] { updateWidget(true); });
	connect(document, &HOCRDocument::layoutChanged, this, [this] { updateWidget(); });
	connect(MAIN->getDisplayer(), &Displayer::imageChanged, this, [this] { updateWidget(); });
	connect(MAIN->getDisplayer(), &Displayer::viewportChanged, this, [this] { updateWidget(); });
	connect(m_spinLinesBefore, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateWidget(true); });
	connect(m_spinLinesAfter, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateWidget(true); });
	connect(m_gapWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateWidget(true); });
	connect(&m_updateTimer, &QTimer::timeout, this, &HOCRProofReadWidget::innerUpdateWidget);
	connect(&m_widgetTimer, &QTimer::timeout, this, &HOCRProofReadWidget::innerRepositionWidget);
	connect(&m_pointerTimer, &QTimer::timeout, this, [this] { repositionPointer(false);});

	ADD_SETTING(SpinSetting("proofReadLinesBefore", m_spinLinesBefore, 1));
	ADD_SETTING(SpinSetting("proofReadLinesAfter", m_spinLinesAfter, 1));
	ADD_SETTING(SpinSetting("proofReadGapWidth", m_gapWidth, 50));

	QApplication::instance()->installEventFilter(this);
	// Start hidden
	hide();
}

void HOCRProofReadWidget::showEvent(QShowEvent *event) {
	QFrame::showEvent(event);
	m_pointer->show();
	LineEdit* focusLineEdit = static_cast<LineEdit*>(MAIN->getDisplayer()->focusProxy());
	if (focusLineEdit != nullptr) {
		focusLineEdit->setFocus();
	}
}

void HOCRProofReadWidget::hideEvent(QHideEvent *event) {
	LineEdit* focusLineEdit = static_cast<LineEdit*>(MAIN->getDisplayer()->focusProxy());
	if (focusLineEdit != nullptr) {
		focusLineEdit->clearFocus();
	}
	m_pointer->hide();
	QFrame::hideEvent(event);
}

void HOCRProofReadWidget::showWidget(bool showit) {
	if(showit) {
		m_hidden = false;
		updateWidget();
		if (m_currentLine != nullptr) {
			show();
			QModelIndex current = m_treeView->currentIndex();
			HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());
			const HOCRItem* item = document->itemAtIndex(current);
		}
	} else {
		m_hidden = true;
		clear();
	}
}

void HOCRProofReadWidget::setProofreadEnabled(bool enabled) {
	m_enabled = enabled;
	if (enabled) {
		repositionWidget();
	} else {
		hide();
		clear();
	}
}

void HOCRProofReadWidget::clear() {
	m_updateTimer.stop();
	m_widgetTimer.stop();
	m_pointerTimer.stop();
	if (m_currentLine == nullptr) {
		return;
	}
	m_currentLine = nullptr;
	for (auto ii:(*m_lineMap)) {
		qDeleteAll(*(ii.second));
		delete ii.second;
		delete ii.first;
	}
	m_lineMap->clear();
	while (m_linesLayout->count() > 0) {
		QLayoutItem* child = m_linesLayout->takeAt(0);
		if (child->widget() != nullptr) {
			delete child->widget();
		}
		delete child;
	}
	m_confidenceLabel->setText("");
	m_confidenceLabel->setStyleSheet("");
	MAIN->getDisplayer()->setFocusProxy(nullptr);
	if (MAIN->focusWidget() == nullptr) {
		// We might have just deleted the focus item (see end of updateWidget)
		// so focus on the displayer. (Happens, e.g., with page down key into unscanned window.)
		MAIN->getDisplayer()->setFocus();
	}
	hide();
}

void HOCRProofReadWidget::updateWidget(bool force) {
	// We use restarting timers (3 places) because we need the prior step to have been painted
	// because we "read" screen positions. Also there are many repeated calls that would just
	// waste time (we've seen as many as 80), so just do one last one using the timer.
	m_force |= force;
	m_updateTimer.start(10);
	m_widgetTimer.stop();
	m_pointerTimer.stop();
}

void HOCRProofReadWidget::innerUpdateWidget() {
	if (m_hidden) {
		return;
	}
	bool force = m_force;
	m_force = false;
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
			if (!m_hidden) {
				show();
			}
			return;
		}
		lineItem = item;
	}

	// Note: QT will rearrange the order of its widget children as it sees fit. (raise() will do that.)
	// Keep a (2D) map of lineEdit widgets in the order we need them in (e.g. for tabbing) in parallel.
	// QMap will create entries for operator[] on the right... use value() instead if probing for existence.
	const QVector<HOCRItem*>& siblings = lineItem->parent()->children();
	int targetLine = lineItem->index();
	if(lineItem != m_currentLine || force) {
		// Rebuild widget
		LineMap* newLines = new LineMap;
		int insPos = 0;
		for(int i = qMax(0, targetLine - nrLinesBefore), j = qMin(siblings.size() - 1, targetLine + nrLinesAfter); i <= j; ++i) {
			HOCRItem* linei = siblings[i];
			if(m_lineMap->contains(linei)) {
				(*newLines)[linei] = m_lineMap->take(linei);
				m_linesLayout->insertWidget(insPos++, (*newLines)[linei].first);
			} else {
				QWidget* lineWidget = new QWidget();
				(*newLines)[linei].first = lineWidget;
				RowMap *row = new RowMap;
				(*newLines)[linei].second = row;
				for(HOCRItem* word : siblings[i]->children()) {
					LineEdit* editor = new LineEdit(this, word, lineWidget); // Add as child to lineWidget
					(*row)[word] = editor;
				}
				m_linesLayout->insertWidget(insPos++, lineWidget);
			}
		}
		for (auto ii:*m_lineMap) {
			qDeleteAll(*(ii.second));
			delete ii.second;
			delete ii.first;
		}
		delete m_lineMap;
		m_lineMap = newLines;
		m_currentLine = lineItem;
	}
	repositionWidget();

	LineEdit* focusLineEdit;
	if(item->itemClass() == "ocr_line") {
		focusLineEdit = m_stub;
	} else {
		// Select selected word or first item of middle line
		const HOCRItem* currentWord = wordItem ? wordItem : lineItem->children().at(0);
		focusLineEdit = (*(*m_lineMap)[m_currentLine].second)[currentWord];
	}
	if(focusLineEdit) {
		MAIN->getDisplayer()->setFocusProxy(focusLineEdit);
		focusLineEdit->setFocus();
	}
}

void HOCRProofReadWidget::repositionWidget() {
	m_widgetTimer.start(10);
	m_pointerTimer.stop();
}

void HOCRProofReadWidget::innerRepositionWidget() {
	if(m_currentLine == nullptr) {
		hide();
		return;
	}

	const HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());
	const QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid()) {
		hide();
		return;
	}
	const HOCRItem* currentItem = document->itemAtIndex(current);

	if (currentItem->itemClass() == "ocr_page") {
		// We need the stub for keys, but don't want it to show in the whole-page case.
		// setVisible(false) (==hide()) doesn't leave it active enough.
		m_pointer->resize(0,0);
		move(0,0);
		resize(0,0);
		return;
	}

	// Position frame
	Displayer* displayer = MAIN->getDisplayer();
	int frameXmin = std::numeric_limits<int>::max();
	int frameXmax = 0;
	QPoint sceneCorner = displayer->getSceneBoundingRect().toRect().topLeft();
	if(m_lineMap->isEmpty()) {
		frameXmin = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).bottomLeft()).x();
		frameXmax = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).bottomRight()).x();
	} 
	for(LineMap::const_iterator lineIter = m_lineMap->begin(); lineIter != m_lineMap->end(); ++lineIter) { 
        const HOCRItem* item = lineIter.key();
		if(item->children().isEmpty()) {
			continue;
		}
		// line widget bounding bbox
		QPoint bottomLeft = displayer->mapFromScene(item->bbox().translated(sceneCorner).bottomLeft());
		frameXmin = std::min(frameXmin, bottomLeft.x());
	}
	frameXmin -= framePadding;

	// Recompute font sizes so that text matches original as closely as possible
	QFont ft = font();
	double avgFactor = 0.0;
	int nFactors = 0;
	m_sceneBoxLeft = std::numeric_limits<int>::max();
	m_sceneBoxRight = 0;

	// First pass: min scaling factor, move to correct location
	for(LineMap::const_iterator lineIter = m_lineMap->begin(); lineIter != m_lineMap->end(); ++lineIter) { 
		RowMap* row = lineIter.value().second;
		for(RowMap::const_iterator rowIter = row->begin(); rowIter != row->end(); ++rowIter) {
			LineEdit* lineEdit = (*row)[rowIter.key()];
			QRect bbox = lineEdit->item()->bbox();
			m_sceneBoxLeft = std::min(m_sceneBoxLeft, bbox.left());
			m_sceneBoxRight = std::max(m_sceneBoxRight, bbox.right());
			QRect sceneBBox = bbox.translated(sceneCorner);
			QPoint bottomLeft = displayer->mapFromScene(sceneBBox.bottomLeft());
			QPoint bottomRight = displayer->mapFromScene(sceneBBox.bottomRight());
			// Factor weighted by length
			int editWidth;
			int lineEditEnd;
			if (std::abs(qRound(lineEdit->item()->parent()->textangle())-1) % 90 > 45) {
				// assuming 90 is the only other choice
				QPoint topLeft = displayer->mapFromScene(sceneBBox.topLeft());
				editWidth = bottomLeft.y() - topLeft.y();
				lineEditEnd = bottomLeft.x() + editWidth;
			} else { 
				editWidth = bottomRight.x() - bottomLeft.x();
				lineEditEnd = bottomRight.x();
			}
			QFont actualFont = lineEdit->item()->fontFamily();
			QFontMetricsF fm(actualFont);
			double factor = editWidth / double(fm.horizontalAdvance(lineEdit->text()));
			avgFactor += lineEdit->text().length() * factor;
			nFactors += lineEdit->text().length();

			lineEdit->move(bottomLeft.x() - frameXmin - cursorPadding, 0);
			lineEdit->m_computedWidth = editWidth;
			lineEdit->setFixedWidth(lineEdit->m_computedWidth + editBoxPadding);
			lineEdit->setStyle(document);
			frameXmax = std::max(frameXmax, lineEditEnd);
		}
	}
	avgFactor = avgFactor > 0 ? avgFactor / nFactors : 1.;
	frameXmax += framePadding;

	// Second pass: apply font sizes, set line heights
	ft.setPointSizeF(ft.pointSizeF() * avgFactor);
	ft.setPointSizeF(ft.pointSizeF() + m_fontSizeDiff);
	QFontMetricsF fm(ft);
	int linePos = 0;
	for(LineMap::const_iterator lineIter = m_lineMap->begin(); lineIter != m_lineMap->end(); ++lineIter) { 
		RowMap* row = lineIter.value().second;
		for(RowMap::const_iterator rowIter = row->begin(); rowIter != row->end(); ++rowIter) {
			LineEdit *lineEdit = (*row)[rowIter.key()];
			QFont lineEditFont = lineEdit->font();
			lineEditFont.setPointSizeF(ft.pointSizeF());
			lineEdit->setFont(lineEditFont);
			lineEdit->setFixedHeight(fm.height() + 5);
		}
		QWidget* lineWidget = lineIter->first;
		lineWidget->setFixedHeight(fm.height() + 10);
		linePos++;
	}
	// Nothing short of show() is completely reliable in getting the sizes right.
	blockSignals(true);
	show();
	blockSignals(false);
	setMinimumWidth(2*clipMargin);
	resize(frameXmax - frameXmin + widgetMargins + 2 * layout()->spacing(), 
		m_lineMap->size() * (fm.height() + editLineSpacing) + 2 * layout()->spacing() + m_controlsWidget->sizeHint().height());

	Q_ASSERT(m_currentLine != nullptr);
	int frameY = repositionPointer(true);
	move(frameXmin - layout()->spacing(), frameY);
	update();
	repositionPointer();
}

void HOCRProofReadWidget::repositionPointer() {
	m_pointerTimer.start(10);
}

int HOCRProofReadWidget::repositionPointer(bool computeOnly) {
	if (m_currentLine == nullptr) {
		return std::numeric_limits<int>::min();
	}
	Displayer* displayer = MAIN->getDisplayer();
	QPoint sceneCorner = displayer->getSceneBoundingRect().toRect().topLeft();
	const HOCRDocument* document = static_cast<HOCRDocument*>(m_treeView->model());
	const QModelIndex current = m_treeView->currentIndex();
	const HOCRItem* currentItem = document->itemAtIndex(current);
	QRectF bbox = currentItem->bbox();
	int pageDpi = currentItem->page()->resolution();
	double maxy = displayer->viewport()->rect().bottom();
	int gap = m_gapWidth->value()*pageDpi/100;

	int editLineTop = 0;
	int editLineBottom = height() - m_controlsWidget->sizeHint().height() - 2*layout()->spacing() - widgetMargins - frameWidth();
	if(m_currentLine->itemClass() == "ocr_line") {
		QWidget* lineWidget = (*m_lineMap)[m_currentLine].first;
		if (!lineWidget->children().isEmpty()) {
			LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->children()[0]);
			editLineTop = lineEdit->mapTo(this,lineEdit->rect().topLeft()).y();
			editLineBottom = lineEdit->mapTo(this,lineEdit->rect().bottomLeft()).y();
		}
	} else if(m_currentLine->itemClass() != "ocrx_word") {
		gap = 0;
	}

	int ptrHeight;
	int wordY;
	QPointF base1;
	QPointF base2;
	QPointF target;

	int frameY = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).translated(0,gap).bottomLeft()).y();
	if(frameY + height() > maxy) {
		// invert
	    frameY = displayer->mapFromScene(m_currentLine->bbox().translated(sceneCorner).translated(0,-gap).topLeft()).y();
		frameY -= height();
		wordY = frameY + editLineBottom;
		ptrHeight = displayer->mapFromScene(bbox.translated(sceneCorner).topLeft()).y() - wordY;
		base1 = QPointF(bbox.bottomLeft().x(), 0);
		base2 = QPointF(bbox.bottomRight().x(),0);
		target = QPointF(bbox.center().x(), ptrHeight);
	} else {
		wordY = displayer->mapFromScene(bbox.translated(sceneCorner).bottomLeft()).y();
		ptrHeight = (frameY-wordY) + editLineTop;
		base1 = QPointF(bbox.bottomLeft().x(), ptrHeight);
		base2 = QPointF(bbox.bottomRight().x(),ptrHeight);
		target = QPointF(bbox.center().x(), 0);
	}

	if (computeOnly) {
		return frameY;
	}

	m_sceneBoxLeft -= clipMargin;
	m_sceneBoxRight += clipMargin;
	int widgetBoxLeft = displayer->mapFromScene(QPoint(m_sceneBoxLeft+sceneCorner.x(),0)).x();
	int widgetBoxRight = displayer->mapFromScene(QPoint(m_sceneBoxRight+sceneCorner.x(),0)).x();

	m_pointer->setWindow(QRect(m_sceneBoxLeft, 0, m_sceneBoxRight-m_sceneBoxLeft, ptrHeight));
	m_pointer->resize(widgetBoxRight - widgetBoxLeft, ptrHeight);
	m_pointer->move(widgetBoxLeft, wordY);
	m_pointer->triangle(base1, base2, target, pageDpi);
	m_pointer->updateGeometry();

	for(LineMap::const_iterator lineIter = m_lineMap->begin(); lineIter != m_lineMap->end(); ++lineIter) { 
		RowMap* row = lineIter.value().second;
		LineEdit *lineEdit = row->value(currentItem, nullptr);
		if (lineEdit != nullptr) {
			// The current item is raised, widened to show all text, and slightly transparent
			lineEdit->raise();
			QFontMetricsF fm(lineEdit->font());
			int apparentWidth = fm.horizontalAdvance(lineEdit->text());
			int displayWidth = std::max(apparentWidth, lineEdit->m_computedWidth);
			lineEdit->setFixedWidth(displayWidth + editBoxPadding);
			lineEdit->setStyle(document, true);
			break;
		}
	}
	m_pointer->update();
	return 0;
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
	                           "<tr><td>Ctrl+_</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Merge with next word insert _</td></tr>"
	                           "<tr><td>Ctrl+W</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Insert new word/line at cursor</td></tr>"
	                           "<tr><td>Ctrl+T</td>"                  "<td>D</td> <td> </td> <td>E</td> <td>Trim word height (heuristic)</td></tr>"
	                           "<tr><td>Delete</td>"                  "<td> </td> <td> </td> <td>E</td> <td>Delete current character</td></tr>"
	                           "<tr><td>Delete</td>"                  "<td> </td> <td>T</td> <td> </td> <td>(Hard) delete current item</td></tr>"
	                           "<tr><td>Ctrl+Delete</td>"             "<td>D</td> <td>T</td> <td>E</td> <td>Toggle Disable current item</td></tr>"
	                           "<tr><td>Ctrl+Shift+Delete</td>"       "<td>D</td> <td> </td> <td> </td> <td>(Hard) delete current item</td></tr>"
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
	                           "<tr><td>Keypad+Enter (hold)</td>"     "<td>D</td> <td> </td> <td> </td> <td>Toggle preview state</td></tr>"
	                           "<tr><td>&lt;print&gt;</td>"           "<td> </td> <td> </td> <td>E</td> <td>Insert the character</td></tr>"
	                           "<tr><td>&lt;print&gt;</td>"           "<td>D</td> <td>T</td> <td> </td> <td>Search to item beginning with &lt;print&gt;</td></tr>"
							   "<tr><td>L-Click</td>"                 "<td>D</td> <td>T</td> <td>E</td> <td>Select</td></tr>"
							   "<tr><td>L-2Click</td>"                "<td> </td> <td>T</td> <td>E</td> <td>Expand/Open for edit</td></tr>"
							   "<tr><td>Shift+L-Click</td>"           "<td>D</td> <td> </td> <td> </td> <td>Select/toggle Enclosing HOCR</td></tr>"
							   "<tr><td>Shift+L-Click</td>"           "<td> </td> <td>T</td> <td> </td> <td>Group Select</td></tr>"
							   "<tr><td>Ctrl+L-Click</td>"            "<td>D</td> <td>T</td> <td> </td> <td>Multi-Select/toggle</td></tr>"
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
