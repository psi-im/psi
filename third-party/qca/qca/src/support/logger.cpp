/*
 * Copyright (C) 2007 Brad Hards <bradh@frogmouth.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qca_support.h"

namespace QCA {

AbstractLogDevice::AbstractLogDevice(const QString &name, QObject *parent) :
        QObject( parent ), m_name( name )
{
}

AbstractLogDevice::~AbstractLogDevice()
{}

QString AbstractLogDevice::name() const
{
        return m_name;
}

void AbstractLogDevice::logTextMessage( const QString &message, Logger::Severity severity )
{
        Q_UNUSED( message );
        Q_UNUSED( severity );
}

void AbstractLogDevice::logBinaryMessage( const QByteArray &blob, Logger::Severity severity )
{
        Q_UNUSED( blob );
        Q_UNUSED( severity );
}

Logger::Logger()
{
        // d pointer?
	m_logLevel = Logger::Notice;
}

Logger::~Logger()
{
        // delete d;
}

QStringList Logger::currentLogDevices() const
{
        return m_loggerNames;
}

void Logger::registerLogDevice(AbstractLogDevice* logger)
{
        m_loggers.append( logger );
        m_loggerNames.append( logger->name() );
}

void Logger::unregisterLogDevice(const QString &loggerName)
{
        for ( int i = 0; i < m_loggers.size(); ++i )
        {
                if ( m_loggers[i]->name() == loggerName )
                {
                        m_loggers.removeAt( i );
                        --i; // we backstep, to make sure we check the new entry in this position.
                }
        }
        for ( int i = 0; i < m_loggerNames.size(); ++i )
        {
                if ( m_loggerNames[i] == loggerName )
                {
                        m_loggerNames.removeAt( i );
                        --i; // we backstep, to make sure we check the new entry in this position.
                }
        }
}

void Logger::setLevel (Severity level)
{
	m_logLevel = level;
}

void Logger::logTextMessage(const QString &message, Severity severity )
{
	if (severity <= level ()) {
		for ( int i = 0; i < m_loggers.size(); ++i )
		{
			m_loggers[i]->logTextMessage( message, severity );
		}
	}
}

void Logger::logBinaryMessage(const QByteArray &blob, Severity severity )
{
	if (severity <= level ()) {
		for ( int i = 0; i < m_loggers.size(); ++i )
		{
			m_loggers[i]->logBinaryMessage( blob, severity );
		}
	}
}

}


