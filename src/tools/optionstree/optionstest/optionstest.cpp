/*
 * optionstest.cpp - Test class for OptionsTree
 * Copyright (C) 2006  Kevin Smith
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

#include <QtCore>
#include <QKeySequence>
#include "optionstree.h"

int main (int argc, char const* argv[])
{
	OptionsTree tree;
	QVariantList l;
	l << QVariant(QString("item1")) << qVariantFromValue(QKeySequence("CTRL+L"));
	QStringList sl;
	sl << "String 1" << "String 2";
	
	tree.setOption("paris",QVariant(QString("sirap")));
	tree.setOption("Benvolio",QVariant(QString("Benvolio")));
	tree.setOption("Benvolio",QVariant(QString("Not benvolio!!")));
	tree.setOption("capulet.Juliet",QVariant(QString("girly")));
	tree.setOption("verona.montague.romeo",QVariant(QString("poisoned")));
	tree.setOption("capulet.Nursey",QVariant(QString("matchmaker")));
	//should fail, values can't have subnodes
	tree.setOption("capulet.Juliet.dead",QVariant(true));
	tree.setOption("verona.city",QVariant(true));
	tree.setOption("verona.lovers",QVariant(2));
	tree.setOption("verona.size",QVariant(QSize(210,295)));
	tree.setOption("verona.stuff",l);
	tree.setOption("verona.stringstuff",sl);
	tree.setComment("verona","Fair city");
	tree.setComment("paris","Bloke or city?");
	tree.setComment("verona.montague.romeo","Watch what this one drinks");
	
	foreach(QString name, tree.allOptionNames())
	{
		qWarning(qPrintable(name));
		qWarning(qPrintable(tree.getOption(name).toString()));
	}
	
	tree.saveOptions("options.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
	
	OptionsTree tree2;
	tree2.loadOptions("options.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
	foreach(QString name, tree2.allOptionNames())
	{
		qWarning(qPrintable(name));
		qWarning(qPrintable(tree2.getOption(name).toString()));
	}
	tree2.saveOptions("options2.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
	
	OptionsTree tree3;
	tree3.loadOptions("options.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
	tree3.saveOptions("options3.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
	return 0;
}
