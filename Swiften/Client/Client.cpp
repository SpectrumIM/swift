/*
 * Copyright (c) 2010 Remko Tronçon
 * Licensed under the GNU General Public License v3.
 * See Documentation/Licenses/GPLv3.txt for more information.
 */

#include "Swiften/Client/Client.h"

#include <boost/bind.hpp>

#include "Swiften/Network/MainBoostIOServiceThread.h"
#include "Swiften/Network/BoostIOServiceThread.h"
#include "Swiften/Client/ClientSession.h"
#include "Swiften/StreamStack/PlatformTLSLayerFactory.h"
#include "Swiften/Network/Connector.h"
#include "Swiften/Network/BoostConnectionFactory.h"
#include "Swiften/Network/BoostTimerFactory.h"
#include "Swiften/TLS/PKCS12Certificate.h"
#include "Swiften/Session/BasicSessionStream.h"
#include "Swiften/Queries/IQRouter.h"

namespace Swift {

Client::Client(const JID& jid, const String& password) : jid_(jid), password_(password) {
	iqRouter_ = new IQRouter(this);
	connectionFactory_ = new BoostConnectionFactory(&MainBoostIOServiceThread::getInstance().getIOService());
	timerFactory_ = new BoostTimerFactory(&MainBoostIOServiceThread::getInstance().getIOService());
	tlsLayerFactory_ = new PlatformTLSLayerFactory();
}

Client::~Client() {
	if (session_ || connection_) {
		std::cerr << "Warning: Client not disconnected properly" << std::endl;
	}
	delete tlsLayerFactory_;
	delete timerFactory_;
	delete connectionFactory_;
	delete iqRouter_;
}

bool Client::isAvailable() {
	return session_ && session_->getState() == ClientSession::Initialized;
}

void Client::connect() {
	connect(jid_.getDomain());
}

void Client::connect(const JID& jid) {
	jid_ = jid;
	connect();
}

void Client::connect(const String& host) {
	assert(!connector_); // Crash on reconnect is here.
	connector_ = Connector::create(host, &resolver_, connectionFactory_, timerFactory_);
	connector_->onConnectFinished.connect(boost::bind(&Client::handleConnectorFinished, this, _1, connector_));
	connector_->setTimeoutMilliseconds(60*1000);
	connector_->start();
}

void Client::handleConnectorFinished(boost::shared_ptr<Connection> connection, Connector::ref connector) {
	bool currentConnection = connector_ && (connector.get() == connector_.get());
	// TODO: Add domain name resolver error
	if (!currentConnection) {
		/* disconnect() was called, this connection should be thrown away*/
		if (connection) {
			connection->disconnect();
		}
		return;
	}
	connector_.reset();
	if (!connection) {
		onError(ClientError::ConnectionError);
	}
	else {
		assert(!connection_);
		connection_ = connection;

		assert(!sessionStream_);
		sessionStream_ = boost::shared_ptr<BasicSessionStream>(new BasicSessionStream(connection_, &payloadParserFactories_, &payloadSerializers_, tlsLayerFactory_, timerFactory_));
		if (!certificate_.isEmpty()) {
			sessionStream_->setTLSCertificate(PKCS12Certificate(certificate_, password_));
		}
		sessionStream_->onDataRead.connect(boost::bind(&Client::handleDataRead, this, _1));
		sessionStream_->onDataWritten.connect(boost::bind(&Client::handleDataWritten, this, _1));
		sessionStream_->initialize();

		session_ = ClientSession::create(jid_, sessionStream_);
		session_->onInitialized.connect(boost::bind(&Client::handleSessionInitialized, this));
		session_->onStanzaAcked.connect(boost::bind(&Client::handleStanzaAcked, this, _1));
		session_->onFinished.connect(boost::bind(&Client::handleSessionFinished, this, _1));
		session_->onNeedCredentials.connect(boost::bind(&Client::handleNeedCredentials, this));
		session_->onStanzaReceived.connect(boost::bind(&Client::handleStanza, this, _1));
		session_->start();
	}
}

void Client::disconnect() {
	if (connector_) {
		connector_.reset();
	}
	if (session_) {
		session_->finish();
	}
	else {
		closeConnection();
	}
}

void Client::closeConnection() {
	if (sessionStream_) {
		sessionStream_.reset();
	}
	if (connection_) {
		connection_->disconnect();
		connection_.reset();
	}
}

void Client::send(boost::shared_ptr<Stanza> stanza) {
	if (!isAvailable()) {
		std::cerr << "Warning: Client: Trying to send a stanza while disconnected." << std::endl;
		return;
	}
	session_->sendStanza(stanza);
}

void Client::sendIQ(boost::shared_ptr<IQ> iq) {
	send(iq);
}

void Client::sendMessage(boost::shared_ptr<Message> message) {
	send(message);
}

void Client::sendPresence(boost::shared_ptr<Presence> presence) {
	send(presence);
}

String Client::getNewIQID() {
	return idGenerator_.generateID();
}

void Client::handleStanza(boost::shared_ptr<Stanza> stanza) {
	boost::shared_ptr<Message> message = boost::dynamic_pointer_cast<Message>(stanza);
	if (message) {
		onMessageReceived(message);
		return;
	}

	boost::shared_ptr<Presence> presence = boost::dynamic_pointer_cast<Presence>(stanza);
	if (presence) {
		onPresenceReceived(presence);
		return;
	}

	boost::shared_ptr<IQ> iq = boost::dynamic_pointer_cast<IQ>(stanza);
	if (iq) {
		onIQReceived(iq);
		return;
	}
}

void Client::setCertificate(const String& certificate) {
	certificate_ = certificate;
}

void Client::handleSessionFinished(boost::shared_ptr<Error> error) {
	session_.reset();
	closeConnection();
	onAvailableChanged(false);
	if (error) {
		ClientError clientError;
		if (boost::shared_ptr<ClientSession::Error> actualError = boost::dynamic_pointer_cast<ClientSession::Error>(error)) {
			switch(actualError->type) {
				case ClientSession::Error::AuthenticationFailedError:
					clientError = ClientError(ClientError::AuthenticationFailedError);
					break;
				case ClientSession::Error::CompressionFailedError:
					clientError = ClientError(ClientError::CompressionFailedError);
					break;
				case ClientSession::Error::ServerVerificationFailedError:
					clientError = ClientError(ClientError::ServerVerificationFailedError);
					break;
				case ClientSession::Error::NoSupportedAuthMechanismsError:
					clientError = ClientError(ClientError::NoSupportedAuthMechanismsError);
					break;
				case ClientSession::Error::UnexpectedElementError:
					clientError = ClientError(ClientError::UnexpectedElementError);
					break;
				case ClientSession::Error::ResourceBindError:
					clientError = ClientError(ClientError::ResourceBindError);
					break;
				case ClientSession::Error::SessionStartError:
					clientError = ClientError(ClientError::SessionStartError);
					break;
				case ClientSession::Error::TLSError:
					clientError = ClientError(ClientError::TLSError);
					break;
				case ClientSession::Error::TLSClientCertificateError:
					clientError = ClientError(ClientError::ClientCertificateError);
					break;
			}
		}
		else if (boost::shared_ptr<SessionStream::Error> actualError = boost::dynamic_pointer_cast<SessionStream::Error>(error)) {
			switch(actualError->type) {
				case SessionStream::Error::ParseError:
					clientError = ClientError(ClientError::XMLError);
					break;
				case SessionStream::Error::TLSError:
					clientError = ClientError(ClientError::TLSError);
					break;
				case SessionStream::Error::InvalidTLSCertificateError:
					clientError = ClientError(ClientError::ClientCertificateLoadError);
					break;
				case SessionStream::Error::ConnectionReadError:
					clientError = ClientError(ClientError::ConnectionReadError);
					break;
				case SessionStream::Error::ConnectionWriteError:
					clientError = ClientError(ClientError::ConnectionWriteError);
					break;
			}
		}
		onError(clientError);
	}
}

void Client::handleNeedCredentials() {
	assert(session_);
	session_->sendCredentials(password_);
}

bool Client::getStreamManagementEnabled() const {
	if (session_) {
		return session_->getStreamManagementEnabled();
	}
	return false;
}

void Client::handleDataRead(const String& data) {
	onDataRead(data);
}

void Client::handleDataWritten(const String& data) {
	onDataWritten(data);
}

void Client::handleStanzaAcked(boost::shared_ptr<Stanza> stanza) {
	onStanzaAcked(stanza);
}

void Client::handleSessionInitialized() {
	jid_ = session_->getLocalJID();
	onConnected();
	onAvailableChanged(true);
}

}
