/*
 * Copyright (c) 2010 Kevin Smith
 * Licensed under the GNU General Public License v3.
 * See Documentation/Licenses/GPLv3.txt for more information.
 */

#include <Swift/QtUI/QtTabbable.h>

#include <QApplication>
#include <QKeyEvent>

#include <Swiften/Base/foreach.h>
#include <Swiften/Base/Platform.h>

#include <Swift/QtUI/QtChatTabs.h>

namespace Swift {

namespace {
#ifdef SWIFTEN_PLATFORM_MACOSX
const Qt::KeyboardModifier ctrlHardwareKeyModifier = Qt::MetaModifier;
#else
const Qt::KeyboardModifier ctrlHardwareKeyModifier = Qt::ControlModifier;
#endif
}

QtTabbable::QtTabbable() : QWidget() {
	shortcuts << new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), window(), SLOT(close()));
	shortcuts << new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), window(), SIGNAL(requestPreviousTab()));
	shortcuts << new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), window(), SIGNAL(requestNextTab()));
	shortcuts << new QShortcut(QKeySequence(Qt::ALT + Qt::Key_A), window(), SIGNAL(requestActiveTab()));
}

QtTabbable::~QtTabbable() {
	foreach (QShortcut* shortcut, shortcuts) {
		delete shortcut;
	}
	emit windowClosing();
}

bool QtTabbable::isWidgetSelected() {
	/*isActiveWindow() shouldn't be necessary, but I don't trust it as far as I can throw it*/
	if (!isActiveWindow()) {
		return false;
	}
	QtChatTabs* parent = qobject_cast<QtChatTabs*>(window());
	return parent ? parent->getCurrentTab() == this : isAncestorOf(QApplication::focusWidget());
}

bool QtTabbable::event(QEvent* event) {
	QKeyEvent* keyEvent = dynamic_cast<QKeyEvent*>(event);
	if (keyEvent) {
		// According to Qt's focus documentation, one can only override CTRL+TAB via reimplementing QWidget::event().
#ifdef SWIFTEN_PLATFORM_LINUX
		if (keyEvent->modifiers() == ctrlHardwareKeyModifier && keyEvent->key() == Qt::Key_Tab && event->type() != QEvent::KeyRelease) {
#else
		if (keyEvent->modifiers() == ctrlHardwareKeyModifier && keyEvent->key() == Qt::Key_Tab) {
#endif
			emit requestNextTab();
			return true;
		}
	}
	return QWidget::event(event);
}

}
