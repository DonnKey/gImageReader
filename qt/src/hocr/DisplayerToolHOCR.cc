/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.cc
 * Copyright (C) 2016-2022 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"

#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QKeyEvent>


DisplayerToolHOCR::DisplayerToolHOCR(Displayer* displayer, QObject* parent)
	: DisplayerTool(displayer, parent) {
	displayer->setCursor(Qt::ArrowCursor);
	MAIN->getRecognizer()->setRecognizeMode(_("Recognize"));
}

DisplayerToolHOCR::~DisplayerToolHOCR() {
	if (m_helpBox != nullptr) {
		m_helpBox->close();
	}
	m_helpBox = nullptr;
	clearSelection();
}

QList<QImage> DisplayerToolHOCR::getOCRAreas() {
	return QList<QImage>() << m_displayer->getImage(m_displayer->getSceneBoundingRect());
}

void DisplayerToolHOCR::mousePressEvent(QMouseEvent* event) {
	m_pressed = true;
	m_mousePressPoint = event->pos();
	if(event->button() == Qt::LeftButton && m_currentAction >= ACTION_DRAW_GRAPHIC_RECT && m_currentAction <= ACTION_DRAW_WORD_RECT) {
		clearSelection();
		m_selection = new DisplayerSelection(this,  m_displayer->mapToSceneClamped(event->pos()));
		connect(m_selection, &DisplayerSelection::geometryChanged, this, &DisplayerToolHOCR::selectionChanged);
		m_displayer->scene()->addItem(m_selection);
		event->accept();
	}
}

void DisplayerToolHOCR::mouseMoveEvent(QMouseEvent* event) {
	if (m_helpBox != nullptr && m_mouseMoves++ > 1) {
		m_helpBox->close();
		m_helpBox = nullptr;
		m_displayer->repaint();
	}
	if(m_selection && m_currentAction >= ACTION_DRAW_GRAPHIC_RECT && m_currentAction <= ACTION_DRAW_WORD_RECT) {
		QPointF p = m_displayer->mapToSceneClamped(event->pos());
		m_selection->setPoint(p);
		m_displayer->ensureVisible(QRectF(p, p));
		event->accept();
	}
}

void DisplayerToolHOCR::mouseReleaseEvent(QMouseEvent* event) {
	// Don't do anything if the release event does not follow a press event...
	if(!m_pressed) {
		return;
	}
	m_pressed = false;
	if(m_selection && m_currentAction >= ACTION_DRAW_GRAPHIC_RECT && m_currentAction <= ACTION_DRAW_WORD_RECT) {
		if(m_selection->rect().width() < 5.0 || m_selection->rect().height() < 5.0) {
			clearSelection();
		} else {
			QRect r = m_selection->rect().translated(-m_displayer->getSceneBoundingRect().toRect().topLeft()).toRect();
			emit bboxDrawn(r, m_currentAction);
		}
		event->accept();
	} else {
		if ((event->pos() - m_mousePressPoint).manhattanLength() < QApplication::startDragDistance()) {
			QPoint p = m_displayer->mapToSceneClamped(event->pos()).toPoint();
			emit positionPicked(p, event);
		}
	}
	setAction(ACTION_NONE, false);
}

void DisplayerToolHOCR::keyPressEvent(QKeyEvent* event) {
	if(m_currentAction >= ACTION_DRAW_GRAPHIC_RECT && m_currentAction <= ACTION_DRAW_WORD_RECT) {
		// Any key resets draw mode, but ignored otherwise
		if (m_helpBox != nullptr) {
			m_helpBox->close();
			m_helpBox = nullptr;
			m_displayer->repaint();
		}
		setAction(ACTION_NONE);
		event->accept();
		return;
	}
}

static QString hintText = QString(_(
						"<table>"
						"<tr><td>Cross Cursor&nbsp;&nbsp;&nbsp;</td><td>Draw New Region</td></tr>"
						"</table>"
					));

void DisplayerToolHOCR::setAction(Action action, bool clearSel) {
	if(action != m_currentAction) {
		emit actionChanged(action);
	}
	if(clearSel) {
		clearSelection();
	}
	m_currentAction = action;
	if(m_currentAction >= ACTION_DRAW_GRAPHIC_RECT && m_currentAction <= ACTION_DRAW_WORD_RECT) {
		m_displayer->setCursor(Qt::CrossCursor);
		m_displayer->setFocus();

		m_helpBox = new QLabel(m_displayer);
		m_helpBox->setAttribute(Qt::WA_DeleteOnClose, true);
		m_helpBox->setText(hintText);
		m_helpBox->setStyleSheet("background-color: yellow; border: 1px solid black;");
		m_helpBox->show(); // render to get final size

		m_mouseMoves = 0;
		QRect hbGeom = m_helpBox->geometry();
		QPoint offset = m_displayer->mapToParent(QPoint(hbGeom.width(),0));
		QPoint topRight = m_displayer->geometry().topRight();
		m_helpBox->move(topRight-offset);
		hbGeom = m_helpBox->geometry();
		m_displayer->cursor().setPos(m_displayer->mapToGlobal(QPoint(hbGeom.bottomLeft().x()-2,hbGeom.bottomLeft().y()+2)));
		m_helpBox->repaint();
	} else {
		m_displayer->setCursor(Qt::ArrowCursor);
	}
}

void DisplayerToolHOCR::setSelection(const QRect& rect, const QRect& minRect) {
	setAction(ACTION_NONE, false);
	QRect r = rect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft());
	QRect mr = minRect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft());
	if(!m_selection) {
		m_selection = new DisplayerSelection(this, r.topLeft());
		connect(m_selection, &DisplayerSelection::geometryChanged, this, &DisplayerToolHOCR::selectionChanged);
		m_displayer->scene()->addItem(m_selection);
	}
	m_selection->setAnchorAndPoint(r.topLeft(), r.bottomRight());
	m_selection->setMinimumRect(mr);
	if (!mr.contains(r)) {
		// Don't jump to page center when zoomed.
		m_displayer->ensureWidgetVisible(m_selection);
	}
}

QImage DisplayerToolHOCR::getSelection(const QRect& rect) const {
	return m_displayer->getImage(rect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft()));
}

void DisplayerToolHOCR::clearSelection() {
	delete m_selection;
	m_selection = nullptr;
}

void DisplayerToolHOCR::selectionChanged(QRectF rect, bool affectsChildren) {
	QRect r = rect.translated(-m_displayer->getSceneBoundingRect().toRect().topLeft()).toRect();
	emit bboxChanged(r, affectsChildren);
}
