/*
 * Copyright (c) 2013 Tobias Markmann
 * Licensed under the simplified BSD license.
 * See Documentation/Licenses/BSD-simplified.txt for more information.
 */

#include <Swift/QtUI/UserSearch/QtContactListWidget.h>

#include <Swift/QtUI/UserSearch/ContactListModel.h>
#include <Swift/QtUI/UserSearch/ContactListDelegate.h>
#include <Swift/QtUI/QtUISettingConstants.h>
#include <Swift/Controllers/Settings/SettingsProvider.h>
#include <Swift/QtUI/QtVCardWidget/QtRemovableItemDelegate.h>

#include <QHeaderView>

namespace Swift {

QtContactListWidget::QtContactListWidget(QWidget* parent, SettingsProvider* settings) : QTreeView(parent), settings_(settings), limited_(false) {
	contactListModel_ = new ContactListModel(true);
	setModel(contactListModel_);

	connect(contactListModel_, SIGNAL(onListChanged(std::vector<Contact>)), this, SLOT(handleListChanged(std::vector<Contact>)));
	connect(contactListModel_, SIGNAL(onListChanged(std::vector<Contact>)), this, SIGNAL(onListChanged(std::vector<Contact>)));
	connect(contactListModel_, SIGNAL(onJIDsDropped(std::vector<JID>)), this, SIGNAL(onJIDsAdded(std::vector<JID>)));

	setSelectionMode(QAbstractItemView::SingleSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setDragEnabled(true);
	setAcceptDrops(true);
	setDropIndicatorShown(true);
	setUniformRowHeights(true);

	setAlternatingRowColors(true);
	setIndentation(0);
	setHeaderHidden(true);
	setExpandsOnDoubleClick(false);
	setItemsExpandable(false);
	setEditTriggers(QAbstractItemView::DoubleClicked);

	contactListDelegate_ = new ContactListDelegate(settings->getSetting(QtUISettingConstants::COMPACT_ROSTER));
	removableItemDelegate_ = new QtRemovableItemDelegate(style());

	setItemDelegateForColumn(0, contactListDelegate_);
	setItemDelegateForColumn(1, removableItemDelegate_);

	header()->resizeSection(1, removableItemDelegate_->sizeHint(QStyleOptionViewItem(), QModelIndex()).width());

	header()->setStretchLastSection(false);
#if QT_VERSION >= 0x050000
	header()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
	header()->setResizeMode(0, QHeaderView::Stretch);
#endif
}

QtContactListWidget::~QtContactListWidget() {
	delete contactListDelegate_;
	delete removableItemDelegate_;
}

void QtContactListWidget::setList(const std::vector<Contact>& list) {
	contactListModel_->setList(list);
}

std::vector<Contact> QtContactListWidget::getList() const {
	return contactListModel_->getList();
}

void QtContactListWidget::setMaximumNoOfContactsToOne(bool limited) {
	limited_ = limited;
	if (limited) {
		handleListChanged(getList());
	} else {
		setAcceptDrops(true);
		setDropIndicatorShown(true);
	}
}

void QtContactListWidget::updateContacts(const std::vector<Contact>& contactUpdates) {
	std::vector<Contact> contacts = contactListModel_->getList();
	foreach(const Contact& contactUpdate, contactUpdates) {
		for(size_t n = 0; n < contacts.size(); n++) {
			if (contactUpdate.jid == contacts[n].jid) {
				contacts[n] = contactUpdate;
				break;
			}
		}
	}
	contactListModel_->setList(contacts);
}

void QtContactListWidget::handleListChanged(std::vector<Contact> list) {
	if (limited_) {
		setAcceptDrops(list.size() <= 1);
		setDropIndicatorShown(list.size() <= 1);
	}
}

void QtContactListWidget::handleSettingsChanged(const std::string&) {
	contactListDelegate_->setCompact(settings_->getSetting(QtUISettingConstants::COMPACT_ROSTER));
}

}