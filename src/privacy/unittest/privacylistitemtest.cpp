/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <QList>
#include <QStringList>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "unittestutil.h"
#include "privacylistitem.h"

class PrivacyListItemTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(PrivacyListItemTest);
	CPPUNIT_TEST(testFromXml_JidType);
	CPPUNIT_TEST(testFromXml_GroupType);
	CPPUNIT_TEST(testFromXml_SubscriptionType);
	CPPUNIT_TEST(testFromXml_NoType);
	CPPUNIT_TEST(testFromXml_AllowAction);
	CPPUNIT_TEST(testFromXml_DenyAction);
	CPPUNIT_TEST(testFromXml_Order);
	CPPUNIT_TEST(testFromXml_MessageChild);
	CPPUNIT_TEST(testFromXml_PresenceInChild);
	CPPUNIT_TEST(testFromXml_PresenceOutChild);
	CPPUNIT_TEST(testFromXml_IQChild);
	CPPUNIT_TEST(testFromXml_AllChildren);
	CPPUNIT_TEST(testFromXml_NoChildren);

	CPPUNIT_TEST(testToXml);

	CPPUNIT_TEST(testIsBlock);
	CPPUNIT_TEST(testIsBlock_NoBlock);

	CPPUNIT_TEST(testBlockItem);

	CPPUNIT_TEST_SUITE_END();

public:
	PrivacyListItemTest();
	virtual ~PrivacyListItemTest();

	void testFromXml_JidType();
	void testFromXml_GroupType();
	void testFromXml_SubscriptionType();
	void testFromXml_NoType();
	void testFromXml_AllowAction();
	void testFromXml_DenyAction();
	void testFromXml_Order();
	void testFromXml_MessageChild();
	void testFromXml_PresenceInChild();
	void testFromXml_PresenceOutChild();
	void testFromXml_IQChild();
	void testFromXml_AllChildren();
	void testFromXml_NoChildren();

	void testToXml();

	void testIsBlock();
	void testIsBlock_NoBlock();

	void testBlockItem();

	PrivacyListItem createItem();
	PrivacyListItem createItemFromXml(const QString&);
	PrivacyListItem createItemFromXmlWithTypeValue(const QString& type, const QString& value);
	PrivacyListItem createItemFromXmlWithoutType();
	PrivacyListItem createItemFromXmlWithAction(const QString&);
	PrivacyListItem createItemFromXmlWithOrder(const QString&);
	PrivacyListItem createItemFromXmlWithChildren(const QStringList& children);
};

CPPUNIT_TEST_SUITE_REGISTRATION(PrivacyListItemTest);

// -----------------------------------------------------------------------------

PrivacyListItemTest::PrivacyListItemTest()
{
}

PrivacyListItemTest::~PrivacyListItemTest()
{
}

PrivacyListItem PrivacyListItemTest::createItemFromXml(const QString& xml)
{
	QDomElement e = UnitTestUtil::createElement(xml);
	PrivacyListItem item;
	item.fromXml(e);
	return item;
}

PrivacyListItem PrivacyListItemTest::createItem()
{
	return createItemFromXml("<item type='jid' value='me@example.com' order='1' action='allow'><presence-in/><presence-out/></item>");
}

PrivacyListItem PrivacyListItemTest::createItemFromXmlWithTypeValue(const QString& type, const QString& value)
{
	return createItemFromXml(QString("<item type='%1' value='%2' order='1' action='allow' />").arg(type).arg(value));
}

PrivacyListItem PrivacyListItemTest::createItemFromXmlWithAction(const QString& action)
{
	return createItemFromXml(QString("<item order='1' action='%1' />").arg(action));
}

PrivacyListItem PrivacyListItemTest::createItemFromXmlWithOrder(const QString& order)
{
	return createItemFromXml(QString("<item order='%1' action='allow' />").arg(order));
}

PrivacyListItem PrivacyListItemTest::createItemFromXmlWithoutType()
{
	return createItemFromXml("<item order='1' action='allow' />");
}

PrivacyListItem PrivacyListItemTest::createItemFromXmlWithChildren(const QStringList& children)
{
	QString xml("<item order='1' action='allow'>");
	foreach(QString child, children) {
		xml += "<" + child + "/>";
	}
	xml += "</item>";
	return createItemFromXml(xml);
}

// -----------------------------------------------------------------------------

void PrivacyListItemTest::testFromXml_JidType()
{
	PrivacyListItem item = createItemFromXmlWithTypeValue("jid", "user@example.com");

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::JidType, item.type());
	CPPUNIT_ASSERT(QString("user@example.com") == item.value());
}

void PrivacyListItemTest::testFromXml_GroupType()
{
	PrivacyListItem item = createItemFromXmlWithTypeValue("group", "mygroup");

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::GroupType, item.type());
	CPPUNIT_ASSERT(QString("mygroup") == item.value());
}

void PrivacyListItemTest::testFromXml_SubscriptionType()
{
	PrivacyListItem item = createItemFromXmlWithTypeValue("subscription", "to");

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::SubscriptionType, item.type());
	CPPUNIT_ASSERT(QString("to") == item.value());
}

void PrivacyListItemTest::testFromXml_NoType()
{
	PrivacyListItem item = createItemFromXmlWithoutType();

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::FallthroughType, item.type());
}

void PrivacyListItemTest::testFromXml_AllowAction()
{
	PrivacyListItem item = createItemFromXmlWithAction("allow");

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::Allow, item.action());
}

void PrivacyListItemTest::testFromXml_DenyAction()
{
	PrivacyListItem item = createItemFromXmlWithAction("deny");

	CPPUNIT_ASSERT_EQUAL(PrivacyListItem::Deny, item.action());
}

void PrivacyListItemTest::testFromXml_Order()
{
	PrivacyListItem item = createItemFromXmlWithOrder("13");

	CPPUNIT_ASSERT_EQUAL(13U, item.order());
}

void PrivacyListItemTest::testFromXml_MessageChild()
{
	PrivacyListItem item = createItemFromXmlWithChildren(QStringList("message"));

	CPPUNIT_ASSERT(item.message());
	CPPUNIT_ASSERT(!item.presenceIn());
	CPPUNIT_ASSERT(!item.presenceOut());
	CPPUNIT_ASSERT(!item.iq());
}

void PrivacyListItemTest::testFromXml_PresenceInChild()
{
	PrivacyListItem item = createItemFromXmlWithChildren(QStringList("presence-in"));

	CPPUNIT_ASSERT(!item.message());
	CPPUNIT_ASSERT(item.presenceIn());
	CPPUNIT_ASSERT(!item.presenceOut());
	CPPUNIT_ASSERT(!item.iq());
}

void PrivacyListItemTest::testFromXml_PresenceOutChild()
{
	PrivacyListItem item = createItemFromXmlWithChildren(QStringList("presence-out"));

	CPPUNIT_ASSERT(!item.message());
	CPPUNIT_ASSERT(!item.presenceIn());
	CPPUNIT_ASSERT(item.presenceOut());
	CPPUNIT_ASSERT(!item.iq());
}

void PrivacyListItemTest::testFromXml_IQChild()
{
	PrivacyListItem item = createItemFromXmlWithChildren(QStringList("iq"));

	CPPUNIT_ASSERT(!item.message());
	CPPUNIT_ASSERT(!item.presenceIn());
	CPPUNIT_ASSERT(!item.presenceOut());
	CPPUNIT_ASSERT(item.iq());
}

void PrivacyListItemTest::testFromXml_AllChildren()
{
	QStringList children;
	children << "message" << "presence-in" << "presence-out" << "iq";

	PrivacyListItem item = createItemFromXmlWithChildren(children);

	CPPUNIT_ASSERT(item.all());
	CPPUNIT_ASSERT(item.message());
	CPPUNIT_ASSERT(item.presenceIn());
	CPPUNIT_ASSERT(item.presenceOut());
	CPPUNIT_ASSERT(item.iq());
}

void PrivacyListItemTest::testFromXml_NoChildren()
{
	PrivacyListItem item = createItemFromXmlWithChildren(QStringList());

	CPPUNIT_ASSERT(item.all());
	CPPUNIT_ASSERT(item.message());
	CPPUNIT_ASSERT(item.presenceIn());
	CPPUNIT_ASSERT(item.presenceOut());
	CPPUNIT_ASSERT(item.iq());
}

void PrivacyListItemTest::testToXml()
{
	PrivacyListItem item1 = createItem();
	QDomDocument doc;

	QDomElement e = item1.toXml(doc);

	doc.appendChild(e);
	PrivacyListItem item2 = createItemFromXml(doc.toString());
	CPPUNIT_ASSERT(item1 == item2);
}

void PrivacyListItemTest::testIsBlock()
{
	PrivacyListItem item = createItemFromXml("<item type='jid' value='me@example.com' order='1' action='deny' />");

	CPPUNIT_ASSERT(item.isBlock());
}

void PrivacyListItemTest::testIsBlock_NoBlock()
{
	PrivacyListItem item = createItemFromXml("<item type='jid' value='me@example.com' order='1' action='deny'><presence-in /></item>");

	CPPUNIT_ASSERT(!item.isBlock());
}

void PrivacyListItemTest::testBlockItem()
{
	PrivacyListItem item = PrivacyListItem::blockItem("me@example.com");

	CPPUNIT_ASSERT(item.isBlock());
	CPPUNIT_ASSERT("me@example.com" == item.value());
}
