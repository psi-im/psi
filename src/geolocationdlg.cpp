/*
 * geolocationdlg.cpp
 * Copyright (C) 2009  Evgeny Khryukin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "geolocationdlg.h"

#include "xmpp_pubsubitem.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "psiaccount.h"
#include "pepmanager.h"
#include "geolocation.h"
#include <QLineEdit>

GeoLocationDlg::GeoLocationDlg(QList<PsiAccount*> list) : QDialog(0), pa_(list)
{
	setAttribute(Qt::WA_DeleteOnClose);
	if(pa_.isEmpty())
		close();
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
 	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setGeoLocation()));
	connect(ui_.pb_reset, SIGNAL(clicked()), SLOT(reset()));

	PsiAccount *pa = pa_.first();
	GeoLocation geoloc = pa->geolocation();
	if(geoloc.isNull())
		return;

	if (geoloc.alt().hasValue())
		ui_.le_altitude->setText(QString::number(geoloc.alt().value()));

	if (!geoloc.area().isEmpty())
		ui_.le_area->setText(geoloc.area());

	if (geoloc.bearing().hasValue())
		ui_.le_bearing->setText(QString::number(geoloc.bearing().value()));

	if (!geoloc.building().isEmpty())
		ui_.le_building->setText(geoloc.building());

	if (!geoloc.country().isEmpty())
		ui_.le_country->setText(geoloc.country());

	if (!geoloc.datum().isEmpty())
		ui_.le_datum->setText(geoloc.datum());

	if (!geoloc.description().isEmpty())
		ui_.le_description->setText(geoloc.description());

	if (geoloc.error().hasValue())
		ui_.le_error->setText(QString::number(geoloc.error().value()));

	if (!geoloc.floor().isEmpty())
		ui_.le_floor->setText(geoloc.floor());

	if (geoloc.lat().hasValue())
		ui_.le_latitude->setText(QString::number(geoloc.lat().value()));

	if (!geoloc.locality().isEmpty())
		ui_.le_locality->setText(geoloc.locality());

	if (geoloc.lon().hasValue())
		ui_.le_longitude->setText(QString::number(geoloc.lon().value()));

	if (!geoloc.postalcode().isEmpty())
		ui_.le_postalcode->setText(geoloc.postalcode());

	if (!geoloc.region().isEmpty())
		ui_.le_region->setText(geoloc.region());

	if (!geoloc.room().isEmpty())
		ui_.le_room->setText(geoloc.room());

	if (!geoloc.street().isEmpty())
		ui_.le_street->setText(geoloc.street());

	if (!geoloc.text().isEmpty())
		ui_.le_text->setText(geoloc.text());
}

void GeoLocationDlg::reset()
{
	foreach(QLineEdit *le, this->findChildren<QLineEdit*>()) {
		le->setText("");
	}
}

void GeoLocationDlg::setGeoLocation()
{
	GeoLocation geoloc;

	if(!ui_.le_altitude->text().isEmpty())
		geoloc.setAlt(ui_.le_altitude->text().toFloat());

	if(!ui_.le_bearing->text().isEmpty())
		geoloc.setBearing(ui_.le_bearing->text().toFloat());

	if(!ui_.le_error->text().isEmpty())
		geoloc.setError(ui_.le_error->text().toFloat());

	if(!ui_.le_latitude->text().isEmpty())
		geoloc.setLat(ui_.le_latitude->text().toFloat());

	if(!ui_.le_longitude->text().isEmpty())
		geoloc.setLon(ui_.le_longitude->text().toFloat());

	if(!ui_.le_datum->text().isEmpty())
		geoloc.setDatum(ui_.le_datum->text());

	if(!ui_.le_description->text().isEmpty())
		geoloc.setDescription(ui_.le_description->text());

	if(!ui_.le_country->text().isEmpty())
		geoloc.setCountry(ui_.le_country->text());

	if(!ui_.le_region->text().isEmpty())
		geoloc.setRegion(ui_.le_region->text());

	if(!ui_.le_locality->text().isEmpty())
		geoloc.setLocality(ui_.le_locality->text());

	if(!ui_.le_area->text().isEmpty())
		geoloc.setArea(ui_.le_area->text());

	if(!ui_.le_street->text().isEmpty())
		geoloc.setStreet(ui_.le_street->text());

	if(!ui_.le_building->text().isEmpty())
		geoloc.setBuilding(ui_.le_building->text());

	if(!ui_.le_floor->text().isEmpty())
		geoloc.setFloor(ui_.le_floor->text());

	if(!ui_.le_room->text().isEmpty())
		geoloc.setRoom(ui_.le_room->text());

	if(!ui_.le_postalcode->text().isEmpty())
		geoloc.setPostalcode(ui_.le_postalcode->text());

	if(!ui_.le_text->text().isEmpty())
		geoloc.setText(ui_.le_text->text());

	foreach(PsiAccount *pa, pa_) {
		if (geoloc.isNull()) {
			pa->pepManager()->disable(PEP_GEOLOC_TN, PEP_GEOLOC_NS, "current");
		}
		else {
			pa->pepManager()->publish(PEP_GEOLOC_NS, PubSubItem("current",geoloc.toXml(*pa->client()->rootTask()->doc())));
		}
	}
	close();
}

