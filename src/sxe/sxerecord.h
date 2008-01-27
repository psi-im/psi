/*
 * sxerecord.h - A class for storing the record of an individual node
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

#ifndef SXDEMETADATA_H
#define SXDEMETADATA_H

#include "sxerecordedit.h"
#include "sxenewedit.h"
#include "sxeremoveedit.h"

/*! \brief A class for storing the record of an individual node.*/
class SxeRecord : public QObject {
    Q_OBJECT

    /*! \brief A class used for keeping track of the values at each version.
     *      Note that this info could be retrieved by analyzing the Edits but would be more troublesome
     */
    class VersionData {
    public:
        /*! \brief Default constructor */
        VersionData(QString _parent = QString(), /*QString _ns = QString(),*/ QString _identifier = QString(), QString _data = QString());
        /*! \brief Copy constructor */
        VersionData(const VersionData &other);
        
        int version;
        QString parent;
        double primaryWeight;
        // QString ns;
        QString identifier;
        QString data;
    };

    public:
        /*! \brief Constructor
         *  Constructs a SxeRecordEdit for \a node.
         */
        SxeRecord(QString rid);
        /*! \brief Desctructor */
        ~SxeRecord();

        /*! \brief Returns the node that the record underlies. */
        QDomNode node() const;
        /*! \brief Applies \a edit to the node. Takes ownership of \a edit. */
        void apply(QDomDocument &doc, SxeEdit* edit, bool importing);
        /*! \brief Returns a list of edits to the node.*/
        QList<const SxeEdit*> edits() const;
        /*! \brief Returns the rid of the node that the record belongs to. */
        QString rid() const;
        /*! \brief Returns the rid of the parent.*/
        QString parent() const;
        /*! \brief Returns the primary-weight of the node.*/
        double primaryWeight() const;
        /*! \brief Returns the version of the record. */
        int version() const;
        /*! \brief Returns the name of the node, if any.*/
        QString name() const;
        /*! \brief Returns the name of the node, if any.*/
        QString nameSpace() const;
        /*! \brief Returns the chdata of the node, if any.*/
        QString chdata() const;
        /*! \brief Returns the processing instruction target, if any.*/
        QString processingInstructionTarget() const;
        /*! \brief Returns the processing instruction data, if any.*/
        QString processingInstructionData() const;

        bool hasSmallerSecondaryWeight(const SxeRecord &other) const;

        bool operator==(const SxeRecord &other) const;
        bool operator<(const SxeRecord &other) const;
        bool operator>(const SxeRecord &other) const;

    signals:
        /*! \brief Emitted when the node is first created. */
        void nodeToBeAdded(const QDomNode &node, bool remote, const QString &id);
        /*! \brief Emitted after the node has been placed in the document tree. */
        // void nodeAdded(const QDomNode &node, bool remote);
        /*! \brief Emitted just before the node is removed. */
        void nodeToBeRemoved(const QDomNode &node, bool remote);
        /*! \brief Emitted when the node is removed. */
        void nodeRemoved(const QDomNode &node, bool remote);
        /*! \brief Emitted when primary-weight or parent record is changed. */
        void nodeToBeMoved(const QDomNode &node, bool remote);
        /*! \brief Emitted after the node has been repositioned. */
        // void nodeMoved(const QDomNode &node, bool remote);
        /*! \brief Emitted when the name of the node is changed. */
        // void nameChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted when the chdata of the node is changed. */
        void chdataChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted just before the chdata of the node is changed. */
        void chdataToBeChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted when the target of the processing instruction is changed. */
        void processingInstructionTargetChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted just before the target of the processing instruction is changed. */
        void processingInstructionTargetToBeChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted when the data of the processing instruction is changed. */
        void processingInstructionDataChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted just before the data of the processing instruction is changed. */
        void processingInstructionDataToBeChanged(const QDomNode &node, bool remote);
        /*! \brief Emitted if a conflicting edit of some kind is applied. */
        void nodeRemovalRequired(const QDomNode &node);

    private:
        
        /*! \brief Applies SxeNewEdit \a edit to the node.*/
        bool applySxeNewEdit(QDomDocument &doc, const SxeNewEdit* edit);
        /*! \brief Applies SxeRemoveEdit \a edit to the node.*/
        bool applySxeRemoveEdit(const SxeRemoveEdit* edit);
        /*! \brief Applies SxeRecordEdit \a edit to the node.*/
        bool applySxeRecordEdit(const SxeRecordEdit* edit, bool importing = false);
        
        QList<SxeEdit*> edits_;
        QList<VersionData*> versions_;
        QDomNode node_;
        QString rid_;
        int version_;
};

#endif
