/*
 * sxesession.cpp - Sxe Session
 * Copyright (C) 2006  Joonas Govenius
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include "sxesession.h"

#include "QTimer"
#include "QUuid"

// The maxlength of a chdata that gets put in one edit
enum {MAXCHDATA = 1024};

using namespace XMPP;

//----------------------------------------------------------------------------
// SxeSession
//----------------------------------------------------------------------------

SxeSession::SxeSession(const Jid &target, const QString &session, const Jid &ownJid,
						bool groupChat, bool serverSupport, const QList<QString> &features) {
	serverSupport_ = serverSupport;
	groupChat_ = groupChat;
	target_ = target;
	ownJid_ = ownJid;
	session_ = session;
	features_ = features;

	queueing_ = false;
	importing_ = false;

	uuidMaxPostfix_ = 0;
	setUUIDPrefix();
}

SxeSession::~SxeSession() {
	emit sessionEnded(this);
}

void SxeSession::initializeDocument(const QDomDocument &doc) {
	bool origImporting = importing_;

	importing_ = true;

	// reset the document
	doc_ = QDomDocument();
	foreach(SxeRecord* meta, recordByNodeId_.values())
		meta->deleteLater();
	// recordByNode_.clear();
	recordByNodeId_.clear();
	queuedIncomingEdits_.clear();
	queuedOutgoingEdits_.clear();


	// import prolog
	doc_.setContent(parseProlog(doc));

	// import other nodes
	// create all nodes recursively from root
	QDomNodeList children = doc.childNodes();
	for(int i = 0; i < children.size(); i++) {
		// skip the XML declaration <?xml ...?> because it isn't a processing instruction
		if(!(children.at(i).isProcessingInstruction() && children.at(i).toProcessingInstruction().target().toLower() == "xml"))
			generateNewNode(children.at(i), QString(), i);
	}

	importing_ = origImporting;
}

void SxeSession::processIncomingSxeElement(const QDomElement &sxe, const QString &id) {
	if(id.isEmpty() && !importing_) {
		qDebug("Trying to process an SXE element without an associated id!");
		return;
	}

	if(processSxe(sxe, id))
		emit documentUpdated(true);
}

bool SxeSession::processSxe(const QDomElement &sxe, const QString &id) {
	// Don't accept duplicates
	if(!id.isEmpty() && usedSxeIds_.contains(id)) {
		qDebug() << QString("Tried to process a duplicate %1 (received: %2).").arg(sxe.attribute("id")).arg(usedSxeIds_.size()).toAscii();
		return false;
	}

	if(!id.isEmpty())
		usedSxeIds_ += id;

	// store incoming edits when queueing
	if(queueing_) {
		// Make sure the element is not already in the queue.
		foreach(IncomingEdit i, queuedIncomingEdits_)
			if(i.xml == sxe)  return false;

		IncomingEdit incoming;
		incoming.id = id;
		incoming.xml = sxe.cloneNode(true).toElement();

		queuedIncomingEdits_.append(incoming);
		return false;
	}

	// create an SxeEdit for each child of the <sxe/>
	QDomNodeList children = sxe.childNodes();

	QList<SxeEdit*> edits;
	for(uint i=0; i < children.length(); i++) {
		if(children.item(i).nodeName() == "new")
			edits.append(new SxeNewEdit(children.item(i).toElement()));
		else if(children.item(i).nodeName() == "set")
			edits.append(new SxeRecordEdit(children.item(i).toElement()));
		else if(children.item(i).nodeName() == "remove")
			edits.append(new SxeRemoveEdit(children.item(i).toElement()));
	}

	if (edits.size() == 0)  return false;

	// process all the edits
	foreach(SxeEdit* e, edits) {
		SxeRecord* meta;
		if(e->type() == SxeEdit::New)
			meta = createRecord(e->rid());
		else
			meta = record(e->rid());

		if(meta)
			meta->apply(doc_, e);
	}

	return true;
}

const QDomDocument& SxeSession::document() const {
	return doc_;
}

bool SxeSession::groupChat() const {
	return groupChat_;
}

bool SxeSession::serverSupport() const {
	return serverSupport_;
}

const Jid SxeSession::target() const {
	return target_;
}

const QString SxeSession::session() const {
	return session_;
}

const Jid SxeSession::ownJid() const {
	return ownJid_;
}

const QList<QString> SxeSession::features() const {
	return features_;
}

QList<const SxeEdit*> SxeSession::startQueueing() {
	// do nothing if already queueing
	if(queueing_)
		return QList<const SxeEdit*>();

	queueing_ = true;

	// Return all the effective Edits to the session so far (snapshot)
	// make sure that they are added in the right order (parents first)
	QString rootid;
	QList<const SxeEdit*> nonDocElementEdits;
	QMultiHash<QString, QString> ridByParent;

	// first collect all nodes into a hash by their parent
	foreach(SxeRecord* m, recordByNodeId_.values()) {
		if(!m->parent().isEmpty()) {
			ridByParent.insert(m->parent(), m->rid());
		} else if(!m->node().isElement()) {
			nonDocElementEdits += m->edits();
		} else
			rootid = m->rid();
	}

	// starting from the root, add all edits to a list recursively
	QList<const SxeEdit*> edits;

	if(!rootid.isNull())
		arrangeEdits(ridByParent, edits, rootid);

	return nonDocElementEdits + edits;
}

void SxeSession::arrangeEdits(QHash<QString, QString> &ridByParent, QList<const SxeEdit*> &output, const QString &iterator) {
	// add the edits to this node
	if(recordByNodeId_.contains(iterator))
		output += recordByNodeId_.value(iterator)->edits();

	// process all the children
	QString child;
	while(!(child = ridByParent.take(iterator)).isNull()) {
		arrangeEdits(ridByParent, output, child);
	}
}

void SxeSession::stopQueueing() {
	// do nothing if not queueing
	if(!queueing_)
		return;

	queueing_ = false;

	// Process queued elements
	flush();

	if(!queuedIncomingEdits_.isEmpty()) {
		while(!queuedIncomingEdits_.isEmpty()) {
			IncomingEdit queued = queuedIncomingEdits_.takeFirst();
			processSxe(queued.xml, queued.id);
		}

		emit documentUpdated(true);
	}
}

void SxeSession::startImporting(const QDomDocument &doc) {
	importing_ = true;

	// reset the document
	initializeDocument(doc);

	// start queueing outgoing edits
	startQueueing();
}

void SxeSession::stopImporting() {
	stopQueueing();

	importing_ = false;
}

void SxeSession::endSession() {
	deleteLater();
}

const QDomNode SxeSession::insertNodeBefore(const QDomNode &node, const QDomNode &parent, const QDomNode &referenceNode) {
	if(referenceNode.isNull() || referenceNode.previousSibling().isNull()) {
		// insert as the first node
		SxeRecord* firstMeta = record(parent.firstChild());
		double primaryWeight;
		if(firstMeta)
			primaryWeight = firstMeta->primaryWeight() - 1;
		else
			primaryWeight = 0;

		// find out the rid of the parent node
		QString parentId;
		SxeRecord* parentMeta = record(parent);
		if(parentMeta)
			parentId = parentMeta->rid();
		else {
			qDebug("Trying to insert a node to parent without an id");
			return QDomNode();
		}

		return insertNode(node, parentId, primaryWeight);
	} else
		return insertNodeAfter(node, parent, referenceNode.previousSibling());
}

const QDomNode SxeSession::insertNodeAfter(const QDomNode &node, const QDomNode &parent, const QDomNode &referenceNode) {
	if(node.isNull())
		return QDomNode();

	// process each child of a document fragment separately
	if(node.isDocumentFragment()) {
		// insert the first node relative to the specified referenceNode
		QDomNode reference = referenceNode;
		QDomNodeList children = node.childNodes();
		for(int i = 0; i < children.size(); i++) {
			QDomNode newNode = children.at(i);
			insertNodeAfter(newNode, parent, reference);
			// and the rest relative to the previous sibling
			reference = newNode;
		}

		return QDomNode();
	}

	// find out the rid of the parent node
	QString parentId;
	SxeRecord* parentMeta = record(parent);
	if(parentMeta)
		parentId = parentMeta->rid();
	else {
		qDebug("Trying to insert a node to parent without an id");
		return QDomNode();
	}

	// find out the appropriate weight for the node
	double primaryWeight;
	SxeRecord* referenceMeta = record(referenceNode);
	SxeRecord* nextReferenceMeta = record(referenceNode.nextSibling());
	if(parent.childNodes().count() == 0)
		primaryWeight = 0;
	else if(referenceMeta && nextReferenceMeta && referenceNode.parentNode() == parent) {
		// get the average of the weights of the reference and it's next sibling
		primaryWeight = (referenceMeta->primaryWeight() + nextReferenceMeta->primaryWeight()) / 2;
	} else {
		// insert as the last node
		referenceMeta = record(parent.lastChild());
		if(referenceMeta)
			primaryWeight = referenceMeta->primaryWeight() + 1;
		else
			primaryWeight = 0;
	}

	return insertNode(node, parentId, primaryWeight);
}

const QDomNode SxeSession::insertNode(const QDomNode &node, const QString &parentId, double primaryWeight) {
	QDomNode newNode;

	SxeRecord* meta = record(node);
	if(meta) {
		// move an existing node

		// figure out what's to be changed
		QHash<SxeRecordEdit::Key, QString> changes;
		if(meta->parent() != parentId)
			changes.insert(SxeRecordEdit::Parent, parentId);
		if(meta->primaryWeight() != primaryWeight)
			changes.insert(SxeRecordEdit::PrimaryWeight, QString::number(primaryWeight));

		if(changes.size() > 0) {
			// create the edit
			SxeRecordEdit* edit = new SxeRecordEdit(meta->rid(), meta->version() + 1, changes);

			// apply it
			meta->apply(doc_, edit);

			// send the edit to others
			queueOutgoingEdit(edit);

			emit documentUpdated(false);
		}
		return node;

	} else {

		QList<SxeEdit*> edits;
		// create a new node
		QDomNode result = generateNewNode(node, parentId, primaryWeight);

		emit documentUpdated(false);
		return result;
	}
}

void SxeSession::removeNode(const QDomNode &node) {
	if(node.isNull())
		return;

	// create SxeRemoveEdits for all child nodes
	generateRemoves(node);
	flush();

	emit documentUpdated(false);
}

void SxeSession::setAttribute(const QDomNode &node, const QString &attribute, const QString &value, int from, int n) {
	if(!node.isElement() || attribute.isEmpty())
		return;

	if(value.isNull()) {
		if(from < 0) {
			// Interpret passing QString() as value as wishing to remove the attribute
			if(node.toElement().hasAttribute(attribute))
				removeNode(node.toElement().attributeNode(attribute));
		}

		return;
	}

	if(node.toElement().hasAttribute(attribute)) {
		setNodeValue(node.attributes().namedItem(attribute), value, from, n);
	} else {
		if(from >= 0) {
			qDebug("from > 0 although attribute doesn't exist yet.");
			return;
		}

		QDomAttr domattr = document_.createAttribute(attribute);
		domattr.setValue(value);
		insertNodeAfter(domattr, node, QDomNode());
	}
}

void SxeSession::setNodeValue(const QDomNode &node, const QString &value, int from, int n) {
	SxeRecord* meta = record(node);

	if(!meta) {
		qDebug() << "Trying to set value of " << node.nodeName() << " (a non-existent node) to \"" << value << "\"";
		return;
	}

	if(!(node.isAttr() || node.isText())) {
		qDebug() << "Trying to set value of a non-attr/text node " << node.nodeName();
		return;
	}

	// Check whether anythings changing:
	QString newValue;
	if(from >= 0 && n >= 0) {
		if((from + n) > node.nodeValue().length()) {
			qDebug() << QString("from (%1) + n (%2) > (length of existing node value) (%3).").arg(from).arg(n).arg(node.nodeValue().length());
			return;
		}
		newValue = node.nodeValue().replace(from, n, value);
	} else
		newValue = value;

	if(newValue == node.nodeValue())
		return;

	// Create the appropriate RecordEdit
	QHash<SxeRecordEdit::Key, QString> changes;
	changes.insert(SxeRecordEdit::Chdata, value);
	if(from >= 0 && n >= 0) {
		changes.insert(SxeRecordEdit::ReplaceFrom, QString("%1").arg(from));
		changes.insert(SxeRecordEdit::ReplaceN, QString("%1").arg(n));
	}

	// create the edit
	SxeRecordEdit* edit = new SxeRecordEdit(meta->rid(), meta->version() + 1, changes);

	// apply it
	meta->apply(doc_, edit);

	// send the edit to others
	queueOutgoingEdit(edit);

	emit documentUpdated(false);
}

void SxeSession::flush() {
	if(queuedOutgoingEdits_.isEmpty())
		return;

	// create the sxe element
	QDomElement sxe = doc_.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", session_);

	// append all queued edits
	while(!queuedOutgoingEdits_.isEmpty()) {
		sxe.appendChild(queuedOutgoingEdits_.takeFirst());
	}

	// pass the bundle to SxeManager
	emit newSxeElement(sxe, target(), groupChat_);
}

QDomNode SxeSession::generateNewNode(const QDomNode &node, const QString &parent, double primaryWeight) {
	if(!record(node)) {
		// generate the appropriate edit(s) for the node
		QString rid = generateUUIDForSession();
		// create the SxeRecord
		SxeRecord* meta = createRecord(rid);
		if(!meta)
			return QDomNode();

		if((node.isAttr() || node.isText()) && node.nodeValue().length() > MAXCHDATA) {
			// Generate a "stub" of the new node
			QDomNode clone = node.cloneNode();
			QString full = clone.nodeValue();
			clone.setNodeValue("");
			QDomNode newNode = generateNewNode(clone, parent, primaryWeight);
			flush();

			// append the value
			for(int i = 0; i < full.length(); i += MAXCHDATA) {
				setNodeValue(newNode, full.mid(i, MAXCHDATA), i, 0);
				flush();
			}
		} else {
			SxeEdit* edit = new SxeNewEdit(rid, node, parent, primaryWeight, false);

			meta->apply(doc_, edit);
			queueOutgoingEdit(edit);
		}

		// process all the attributes and child nodes recursively
		if(node.isElement()) {
			// attributes
			QDomNamedNodeMap attributes = node.attributes();
			for(int i = 0; i < attributes.count(); i++)
				generateNewNode(attributes.item(i), rid, i);

			// child nodes
			QDomNodeList children = node.childNodes();
			for(int i = 0; i < children.count(); i++)
				generateNewNode(children.at(i), rid, i);
		}

		return meta->node();
	}

	return QDomNode();
}

void SxeSession::generateRemoves(const QDomNode &node) {
	SxeRecord* meta = record(node);
	if(meta) {
		// process all the attributes and child nodes recursively
		if(node.isElement()) {
			// attributes
			QDomNamedNodeMap attributes = node.attributes();
			for(int i = 0; i < attributes.count(); i++)
				generateRemoves(attributes.item(i));

			// child nodes
			QDomNodeList children = node.childNodes();
			for(int i = 0; i < children.count(); i++)
				generateRemoves(children.at(i));
		}

		// generate the appropriate edit for the node
		SxeRemoveEdit* edit = new SxeRemoveEdit(meta->rid());
		queueOutgoingEdit(edit);
		meta->apply(doc_, edit);
	}
}

void SxeSession::reposition(const QDomNode &node, bool remote) {
	Q_UNUSED(remote);
	SxeRecord* meta = record(node);

	if(!meta) {
		qDebug("Trying to reposition a node without record.");
		return;
	}

	// inserting nodes to the document node is a special case
	if(meta->parent().isEmpty()) {
		if(node.isElement() && !(doc_.documentElement().isNull() || doc_.documentElement() == node)) {
			qDebug("Trying to add a root node when one already exists.");
			removeNode(node);
			flush();
			return;
		}

		doc_.appendChild(node);

		return;
	}

	// find the parent node
	SxeRecord* parentMeta = record(meta->parent());
	QDomNode parentNode;
	if(!parentMeta
		|| (parentNode = parentMeta->node()).isNull()) {
		qDebug("non-existent parent. Deleting node.");
		removeNode(node);
		flush();
		return;
	}

	// simply insert if an attribute
	if(node.isAttr()) {
		QDomElement parentElement = parentNode.toElement();
		if(parentElement.isNull()) {
			qDebug("Trying to insert an attribute to a non-element.");
			return;
		}

		// unless an attribute with the same name already exists
		if(parentElement.hasAttribute(node.nodeName())
			&& node != parentElement.attributeNode(node.nodeName())) {

			// qDebug() << QString("Removing an attribute node '%1' because one already exists").arg(node.nodeName());

			// delete the node with smaller secondary weight
			if(removeSmaller(meta, record(parentElement.attributeNode(node.nodeName()))))
				return;

		}

		parentElement.setAttributeNode(node.toAttr());

		return;
	}

	// get the list of siblings
	QDomNodeList children = parentNode.childNodes();

	// default to appending
	QDomNode before;
	bool insertLast = true;
	if(children.length() > 0) {
		// find the child with the smallest weight greater than the weight of the node itself
		// if any, insert the node before that node
		for(uint i=0; i < children.length(); i++) {
			if(children.item(i) != node) {
				SxeRecord* siblingMeta = record(children.item(i));
				if(siblingMeta && *meta < *siblingMeta) {
					// qDebug() << QString("%1 (pw: %2) is less than %3 (pw: %4)").arg(meta->name()).arg(meta->primaryWeight()).arg(siblingMeta->name()).arg(siblingMeta->primaryWeight()).toAscii();
					before = children.item(i);
					insertLast = false;
					break;
				}
			}
		}
	}

	if(insertLast) {
		// qDebug() << QString("Repositioning '%1' (pw: %2) as last.").arg(node.nodeName()).arg(meta->primaryWeight()).toAscii();
		parentNode.appendChild(node);
	} else {
		// qDebug() << QString("Repositioning '%1' (pw: %2) before '%3' (pw: %4).").arg(node.nodeName()).arg(meta->primaryWeight()).arg(before.nodeName()).arg(record(before)->primaryWeight()).toAscii();
		parentNode.insertBefore(node, before);
	}
}

// void SxeSession::addToLookup(const QDomNode &node, bool, const QString &rid) {
	// recordByNode_[node]
	//	 = recordByNodeId_[rid];
// }

void SxeSession::handleNodeToBeAdded(const QDomNode &node, bool remote) {
	emit nodeToBeAdded(node, remote);
	reposition(node, remote);
	emit nodeAdded(node, remote);
}

void SxeSession::handleNodeToBeMoved(const QDomNode &node, bool remote) {
	emit nodeToBeMoved(node, remote);
	reposition(node, remote);
	emit nodeMoved(node, remote);
}

void SxeSession::handleNodeToBeRemoved(const QDomNode &node, bool remote) {
	emit nodeToBeRemoved(node, remote);
	removeRecord(node);
}


void SxeSession::removeRecord(const QDomNode &node) {
	QMutableHashIterator<QString, SxeRecord*> i(recordByNodeId_);

	while(i.hasNext()) {
		if(node == i.next().value()->node()) {
			i.remove();
			return;
		}
	}
}

bool SxeSession::removeSmaller(SxeRecord* meta1, SxeRecord* meta2) {
	if(!meta1)
		return true;
	if(!meta2)
		return false;

	if(meta1->hasSmallerSecondaryWeight(*meta2)) {
		removeNode(meta1->node());
		flush();
		return true;
	} else {
		removeNode(meta2->node());
		flush();
		return false;
	}
}

void SxeSession::addUsedSxeId(QString id) {
	usedSxeIds_ += id;
}

QList<QString> SxeSession::usedSxeIds() {
	return usedSxeIds_;
}

void SxeSession::queueOutgoingEdit(SxeEdit* edit) {
	if(!importing_)
		queuedOutgoingEdits_.append(edit->xml(doc_));
}

SxeRecord* SxeSession::createRecord(const QString &id) {
	if(recordByNodeId_.contains(id)) {
		qDebug() << QString("record by id '%1' already exists.").arg(id).toAscii();
		return NULL;
	}

	SxeRecord* m = new SxeRecord(id);
	recordByNodeId_[id] = m;

	// once the node is actually created, add it to the lookup table
	// connect(m, SIGNAL(nodeAdded(QDomNode, bool, QString)), SLOT(addToLookup(const QDomNode &, bool, const QString &)));

	// remove the node in case of a conflicting edit
	connect(m, SIGNAL(nodeRemovalRequired(QDomNode)), SLOT(removeNode(QDomNode)));

	// reposition and emit public signals as needed when record is changed
	connect(m, SIGNAL(nodeToBeAdded(QDomNode, bool, QString)), SLOT(handleNodeToBeAdded(const QDomNode &, bool)));
	connect(m, SIGNAL(nodeToBeMoved(QDomNode, bool)), SLOT(handleNodeToBeMoved(const QDomNode &, bool)));
	connect(m, SIGNAL(nodeToBeRemoved(QDomNode, bool)), SLOT(handleNodeToBeRemoved(const QDomNode &, bool)));
	connect(m, SIGNAL(chdataToBeChanged(QDomNode, bool)), SIGNAL(chdataToBeChanged(const QDomNode &, bool)));
	connect(m, SIGNAL(chdataChanged(QDomNode, bool)), SIGNAL(chdataChanged(const QDomNode &, bool)));
	// connect(m, SIGNAL(nameChanged(QDomNode, bool)), SIGNAL(nameChanged(const QDomNode &, bool)));

	return m;
}

SxeRecord* SxeSession::record(const QString &id) {
	return recordByNodeId_.value(id);
}

SxeRecord* SxeSession::record(const QDomNode &node) const {
	if(node.isNull())
		return NULL;

	// go through all the SxeRecord's
	foreach(SxeRecord* meta, recordByNodeId_.values()) {
		// qDebug() << QString("id: %1").arg(meta->rid()).toAscii();
		if(node == meta->node())
			return meta;
	}

	return NULL;
}

void SxeSession::setUUIDPrefix(const QString uuidPrefix) {
	if(!uuidPrefix.isNull())
		uuidPrefix_ = uuidPrefix;
	else
		uuidPrefix_ = generateUUID();
}

QString SxeSession::generateUUIDForSession() {
	return QString("%1.%2").arg(uuidPrefix_).arg(++uuidMaxPostfix_, 0, 36); // 36 is the max allowed base
}

QString SxeSession::generateUUID() {
	QString fullstring = QUuid::createUuid().toString();
	// return the string between "{" and "}"
	int start = fullstring.indexOf("{") + 1;
	return fullstring.mid(start, fullstring.lastIndexOf("}") - start);
}

QString SxeSession::parseProlog(const QDomDocument &doc) {
	QString prolog;
	QTextStream stream(&prolog);

	// check for the XML declaration
	if(doc.childNodes().at(0).isProcessingInstruction()
		  && doc.childNodes().at(0).toProcessingInstruction().target().toLower() == "xml")
		doc.childNodes().at(0).save(stream, 1);

	if(!doc.doctype().isNull())
		doc.doctype().save(stream, 1);

	return prolog;
}
