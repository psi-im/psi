/*
 * sxerecord.cpp - A class for storing the record of an individual node
 * Copyright (C) 2007  Joonas Govenius
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

#include "sxerecord.h"

//----------------------------------------------------------------------------
// SxeRecord::VersionData
//----------------------------------------------------------------------------

SxeRecord::VersionData::VersionData(QString _parent, /*QString _ns,*/ QString _identifier, QString _data)  {
    version = 0;
    primaryWeight = 0;
    parent = _parent;
    // ns = _ns;
    identifier = _identifier;
    data = _data;
}

SxeRecord::VersionData::VersionData(const VersionData &other) : parent(other.parent), /*ns(other.ns),*/ identifier(other.identifier), data(other.data)  {
    version = other.version;
    primaryWeight = other.primaryWeight;
}


//----------------------------------------------------------------------------
// SxeRecord
//----------------------------------------------------------------------------

SxeRecord::SxeRecord(QString rid) {
    rid_ = rid;
    version_ = 0;
};

SxeRecord::~SxeRecord() {
    while(!versions_.isEmpty())
        delete versions_.takeFirst();
    while(!edits_.isEmpty())
        delete edits_.takeFirst();
};

QDomNode SxeRecord::node() const {
    return node_;
}

void SxeRecord::apply(QDomDocument &doc, SxeEdit* edit, bool importing) {
    if(edit->rid() == rid()) {
        if((edit->type() == SxeEdit::New && applySxeNewEdit(doc, dynamic_cast<const SxeNewEdit*>(edit)))
        || (edit->type() == SxeEdit::Remove && applySxeRemoveEdit(dynamic_cast<const SxeRemoveEdit*>(edit)))
        || (edit->type() == SxeEdit::Record && applySxeRecordEdit(dynamic_cast<const SxeRecordEdit*>(edit), importing))) {
            edits_ += edit;
        }
    } else {
        qDebug(QString("Tried to apply an edit meant for %1 to %2.").arg(edit->rid()).arg(rid()).toAscii());
    }
}

QList<const SxeEdit*> SxeRecord::edits() const {
    QList<const SxeEdit*> edits;
    foreach(SxeEdit* e, edits_) {
        edits.append(e);
    }
    return edits;
};

bool SxeRecord::applySxeNewEdit(QDomDocument &doc, const SxeNewEdit* edit) {
    if(!(edits_.size() == 0 && version_ == 0 && node_.isNull())) {
        qDebug("Someone's not behaving! Tried to apply a SxeNewEdit to an existing node.");
        emit nodeRemovalRequired(node_);
        return false;
    }

    // create the new node
    if(edit->nodeType() == QDomNode::ElementNode) {

        if(edit->nameSpace().isEmpty())
            node_ = doc.createElement(edit->name());
        else
            node_ = doc.createElementNS(edit->nameSpace(), edit->name());

    } else if(edit->nodeType() == QDomNode::AttributeNode) {

        if(edit->nameSpace().isEmpty())
            node_ = doc.createAttribute(edit->name());
        else
            node_ = doc.createAttributeNS(edit->nameSpace(), edit->name());

        node_.toAttr().setValue(edit->chdata());

    } else if(edit->nodeType() == QDomNode::TextNode) {

        node_ = doc.createTextNode(edit->chdata());

    } else if(edit->nodeType() == QDomNode::CommentNode) {

        node_ = doc.createComment(edit->chdata());

    } else if(edit->nodeType() == QDomNode::ProcessingInstructionNode) {

        node_ = doc.createProcessingInstruction(edit->processingInstructionTarget(), edit->processingInstructionData());

    } else
        return false;

    VersionData* data = new VersionData();

    data->version = 0;
    data->parent = edit->parent();
    data->primaryWeight = edit->primaryWeight();
    data->identifier = edit->name();
    data->data = edit->chdata();

    versions_ += data;

    emit nodeToBeAdded(node_, edit->remote(), edit->rid());
    
    return true;
}

bool SxeRecord::applySxeRemoveEdit(const SxeRemoveEdit* edit) {
    if(!node_.isNull()) {
        emit nodeToBeRemoved(node_, edit->remote());

        if(node_.isAttr()) {
            QDomNode parent = node_.parentNode();
            while(!parent.isElement() && !parent.isNull())
                parent = parent.parentNode();

            if(!parent.isNull()) {
                parent.toElement().removeAttributeNode(node_.toAttr());
                emit nodeRemoved(node_, edit->remote());
            } // else, the attr hadn't been added to the doc yet.
        } else {
            node_.parentNode().removeChild(node_);
            emit nodeRemoved(node_, edit->remote());
        }
        
        // delete the record
        deleteLater();
    }
    
    return true;
}

bool SxeRecord::applySxeRecordEdit(const SxeRecordEdit* edit, bool importing) {
        if(!node_.isNull() && edits_.size() > 0) {
            if(importing && version_ < edit->version())
                // don't check for "in-order" 
                version_ = edit->version();
            else
                version_++;

            QString oldParent = parent();
            double oldPrimaryWeight = primaryWeight();

            if(edit->version() == version_) {
                // process the "in order" edit
                
                // clone the previous VersionData
                VersionData* data = new VersionData(*versions_.last());
                data->version = version_;

                versions_ += data;

                foreach(SxeRecordEdit::Key key, edit->keys()) {
                    if(key == SxeRecordEdit::Parent && edit->value(key) != parent()) {
                        data->parent = edit->value(key);
                    } else if(key == SxeRecordEdit::PrimaryWeight && edit->value(key).toDouble() != primaryWeight()) {
                        data->primaryWeight = edit->value(key).toDouble();
                    } else if(key == SxeRecordEdit::Name && edit->value(key) != name()) {
                        data->identifier = edit->value(key);
                        // TODO: change the name somehow... by copying all the child nodes to a new node i suppose
                        // emit nameChanged(node_, edit->remote());
                    } else if(key == SxeRecordEdit::Chdata) {
                        QString newValue;
                        
                        // Check for partial replacements
                        QList<SxeRecordEdit::Key> keys = edit->keys();
                        if(keys.contains(SxeRecordEdit::ReplaceFrom)
                            && keys.contains(SxeRecordEdit::ReplaceN)) {
                            bool ok1, ok2;
                            int from = edit->value(SxeRecordEdit::ReplaceFrom).toInt(&ok1);
                            int n = edit->value(SxeRecordEdit::ReplaceN).toInt(&ok2);
                            newValue = data->data;
                            if(ok1 && ok2 && from >= 0 && n >= 0 && from + n <= data->data.length()) {
                                newValue.replace(from, n, edit->value(key));
                            } else {
                                if(!ok1)
                                    qDebug(QString("Could not convert 'replacefrom' = '%1' to int.").arg(edit->value(SxeRecordEdit::ReplaceFrom)));
                                if(!ok2)
                                    qDebug(QString("Could not convert 'replacen' = '%1' to int.").arg(edit->value(SxeRecordEdit::ReplaceN)));
                                if(from < 0)
                                    qDebug(QString("'replacefrom' = '%1' is negative.").arg(edit->value(SxeRecordEdit::ReplaceFrom)));
                                if(n < 0)
                                    qDebug(QString("'replacen' = '%1' is negative.").arg(edit->value(SxeRecordEdit::ReplaceN)));
                                if(from + n > data->data.length())
                                    qDebug(QString("from (%1) + n (%2) > data->data.length() (%3).").arg(from).arg(n).arg(data->data.length()));
                            }
                        } else {
                            newValue = edit->value(key);
                        }

                        // Only process further if some change resulted
                        if(newValue != data->data) {
                            data->data = newValue;

                            emit chdataToBeChanged(node_, edit->remote());
                            // qDebug(QString("Setting '%1' to \"%2\"").arg(node_.nodeName()).arg(data->data).toAscii());
                            if(node_.isAttr() || node_.isText() || node_.isComment())
                                node_.setNodeValue(data->data);

                            emit chdataChanged(node_, edit->remote());
                        }
                    } else if(key == SxeRecordEdit::ProcessingInstructionTarget && edit->value(key) != processingInstructionTarget()) {
                        data->identifier = edit->value(key);

                        if(node_.isProcessingInstruction()) {
                            // emit processingInstructionTargetToBeChanged(node_, edit->remote());

                            // TODO: figure out a way to change the target
                            //      will probably need to recreate the pi

                            // emit processingInstructionTargetChanged(node_, edit->remote());
                        }
                    } else if(key == SxeRecordEdit::ProcessingInstructionData && edit->value(key) != processingInstructionData()) {
                        data->data = edit->value(key);

                        if(node_.isProcessingInstruction()) {
                            emit processingInstructionDataToBeChanged(node_, edit->remote());
                            node_.toProcessingInstruction().setData(data->data);
                            emit processingInstructionDataChanged(node_, edit->remote());
                        }
                    }
                }

            } else if (edit->version() < version_) {

                // Revert the changes down to version of the SxeEdit - 1.
                // by deleting the higher versions and edits
                while(versions_.size() > 1 && versions_.last()->version >= edit->version())
                    delete versions_.takeLast();
                while(edits_.size() > 0 && edits_.last()->type() == SxeEdit::Record && dynamic_cast<const SxeRecordEdit*>(edits_.last())->version() >= edit->version())
                    delete edits_.takeLast();

                if((node_.isElement() || node_.isAttr())
                    && node_.nodeName() != name()) {
                    // TODO: update the name somehow
                }

                if((node_.isText() || node_.isAttr() || node_.isComment())
                    && node_.nodeValue() != chdata()) {
                    node_.setNodeValue(chdata());
                }

                if(node_.isProcessingInstruction()) {
                    if(node_.toProcessingInstruction().target() != processingInstructionTarget()) {
                        // TODO: change the target somehow
                    }

                    if(node_.toProcessingInstruction().data() != processingInstructionData()) {
                        node_.toProcessingInstruction().setData(processingInstructionData());
                    }
                }
                
            } else {
                // This should never happen given the seriality condition.
                // Reason to worry about misbehaviour of peers but not much to do from here.
                qWarning(QString("Configure to node '%1' version '%2' arrived when the node had version '%3'.").arg(edit->rid()).arg(version_).arg(edit->version() - 1).toAscii());
            }

            if(parent() != oldParent || primaryWeight() != oldPrimaryWeight)
                emit nodeToBeMoved(node_, edit->remote());

            return true;

        }
        return false;    
}

QString SxeRecord::rid() const {
    return rid_;
};

QString SxeRecord::parent() const {
    if(versions_.isEmpty())
        return "";
    else
        return versions_.last()->parent;
}

double SxeRecord::primaryWeight() const {
    if(versions_.isEmpty())
        return 0;
    else
        return versions_.last()->primaryWeight;
}

int SxeRecord::version() const {
    return version_;
}

QString SxeRecord::name() const {
    if(versions_.isEmpty())
        return QString();
    else
        return versions_.last()->identifier;
}

QString SxeRecord::nameSpace() const {
    if(node_.isNull())
        return QString();
    else
        return node_.namespaceURI();
}

QString SxeRecord::chdata() const {
    if(versions_.isEmpty())
        return QString();
    else
        return versions_.last()->data;
}

QString SxeRecord::processingInstructionTarget() const {
    if(versions_.isEmpty())
        return QString();
    else
        return versions_.last()->identifier;
}

QString SxeRecord::processingInstructionData() const {
    if(versions_.isEmpty())
        return QString();
    else
        return versions_.last()->data;
}

bool SxeRecord::hasSmallerSecondaryWeight(const SxeRecord &other) const {
    // compare the "secondary weight" (the id's)
    QString selfid = rid();
    QString otherid = other.rid();
    for(int i = 0; i < selfid.length() && i < otherid.length(); i++) {
        if(selfid[i].unicode() < otherid[i].unicode())
            return true;
        else if(selfid[i].unicode() > otherid[i].unicode())
            return false;
    }
    // if no difference was found. one must be shorter
    return (selfid.length() < otherid.length());
}

bool SxeRecord::operator==(const SxeRecord &other) const {
    return rid() == other.rid();
}

bool SxeRecord::operator<(const SxeRecord &other) const {
    if(other == *this)
        return false;

    if(primaryWeight() < other.primaryWeight())
        return true;
    else if(primaryWeight() == other.primaryWeight()) {
        return hasSmallerSecondaryWeight(other);
    } else
        return false;
}

bool SxeRecord::operator>(const SxeRecord &other) const {
    return !(*this == other || *this < other);
}
