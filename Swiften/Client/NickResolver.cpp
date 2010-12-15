/*
 * Copyright (c) 2010 Kevin Smith
 * Licensed under the GNU General Public License v3.
 * See Documentation/Licenses/GPLv3.txt for more information.
 */

#include "Swiften/Client/NickResolver.h"

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include "Swiften/MUC/MUCRegistry.h"
#include "Swiften/Roster/XMPPRoster.h"
#include "Swiften/VCards/VCardManager.h"

// FIXME: The NickResolver currently relies on the vcard being requested by the client on login.
// The VCardManager should get an onConnected() signal (which is signalled when the stanzachannel is available(, and each time this is emitted,
// the nickresolver should request the vcard.
// FIXME: The ownJID functionality should probably be removed, and NickManager should be used directly.

namespace Swift {

NickResolver::NickResolver(const JID& ownJID, XMPPRoster* xmppRoster, VCardManager* vcardManager, MUCRegistry* mucRegistry) : ownJID_(ownJID) {
	xmppRoster_ = xmppRoster;
	vcardManager_ = vcardManager;
	if (vcardManager_) {
		vcardManager_->onVCardChanged.connect(boost::bind(&NickResolver::handleVCardReceived, this, _1, _2));
	}
	mucRegistry_ = mucRegistry;
	xmppRoster_->onJIDUpdated.connect(boost::bind(&NickResolver::handleJIDUpdated, this, _1, _2, _3));
	xmppRoster_->onJIDAdded.connect(boost::bind(&NickResolver::handleJIDAdded, this, _1));
}

void NickResolver::handleJIDUpdated(const JID& jid, const String& previousNick, const std::vector<String>& /*groups*/) {
	onNickChanged(jid, previousNick);
}

void NickResolver::handleJIDAdded(const JID& jid) {
	String oldNick(jidToNick(jid));
	onNickChanged(jid, oldNick);
}

String NickResolver::jidToNick(const JID& jid) {
	if (jid.toBare() == ownJID_) {
		if (!ownNick_.isEmpty()) {
			return ownNick_;
		}
	}
	String nick;

	if (mucRegistry_ && mucRegistry_->isMUC(jid.toBare()) ) {
		return jid.getResource().isEmpty() ? jid.toBare().toString() : jid.getResource();
	}

	if (xmppRoster_->containsJID(jid) && !xmppRoster_->getNameForJID(jid).isEmpty()) {
		return xmppRoster_->getNameForJID(jid);
	}

	return jid.toBare();
}

void NickResolver::handleVCardReceived(const JID& jid, VCard::ref ownVCard) {
	if (!jid.equals(ownJID_, JID::WithoutResource)) {
		return;
	}
	String initialNick = ownNick_;
	ownNick_ = ownJID_.toString();
	if (ownVCard) {
		if (!ownVCard->getNickname().isEmpty()) {
			ownNick_ = ownVCard->getNickname();
		} else if (!ownVCard->getGivenName().isEmpty()) {
			ownNick_ = ownVCard->getGivenName();
		} else if (!ownVCard->getFullName().isEmpty()) {
			ownNick_ = ownVCard->getFullName();
		}
	}
}

}

