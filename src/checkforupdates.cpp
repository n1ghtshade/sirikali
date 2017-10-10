﻿/*
 *
 *  Copyright (c) 2015
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "checkforupdates.h"

checkUpdates::checkUpdates( QWidget * widget ) : m_widget( widget ),m_running( false )
{
	m_networkRequest.setRawHeader( "Host","api.github.com" ) ;
	m_networkRequest.setRawHeader( "Accept-Encoding","text/plain" ) ;

	m_timer.setInterval( 1000 * utility::networkTimeOut() ) ;

	connect( &m_timer,SIGNAL( timeout() ),this,SLOT( timeOut() ),Qt::QueuedConnection ) ;
}

void checkUpdates::check( bool e )
{
	m_autocheck = e ;

	m_results.clear() ;

	m_running = true ;

	this->checkForUpdate( 0 ) ;
}

void checkUpdates::run( bool e )
{
	if( m_running == false ){

		if( e ){

			if( utility::autoCheck() ){

				this->check( e ) ;
			}
		}else{
			this->check( e ) ;
		}
	}
}

void checkUpdates::timeOut()
{
	m_timer.stop() ;

	m_running = false ;

	if( m_network.cancel( m_networkReply ) ){

		auto s = QString::number( utility::networkTimeOut() ) ;
		auto e = tr( "Network Request Failed To Respond Within %1 Seconds." ).arg( s ) ;

		DialogMsg( m_widget ).ShowUIOK( tr( "ERROR" ),e ) ;
	}
}

void checkUpdates::showResult()
{
	m_running = false ;

	bool show = false ;
	QString e = "\n" ;

	auto _tr = []( const QStringList& l ){

		auto e = QObject::tr( "%1\"%2\" Installed Version Is : %3.\nLatest Version Is : %4." ) ;
		return e.arg( "",l.at( 0 ),l.at( 1 ),l.at( 2 ) ) ;
	} ;

	for( const auto& it : m_results ){

		e += _tr( it ) + "\n\n" ;

		const auto& a = it.at( 1 ) ;

		const auto& b = it.at( 2 ) ;

		if( a != "N/A" && b != "N/A" ){

			if( a != b ){

				show = true ;
			}
		}
	}

	if( m_autocheck ){

		if( show ){

			DialogMsg( m_widget ).ShowUIInfo( tr( "Version Info" ),true,e + "\n" ) ;
		}
	}else{
		DialogMsg( m_widget ).ShowUIInfo( tr( "Version Info" ),true,e + "\n" ) ;
	}
}

QString checkUpdates::InstalledVersion( const siritask::volumeType& e )
{
	if( e == "sirikali" ){

		return THIS_VERSION ;
	}

	auto exe = e.executableFullPath() ;

	if( exe.isEmpty() ){

		return "N/A" ;
	}

	auto args = [ & ](){

		if( e == "cryfs" ){

			return exe ;

		}else if( e == "securefs" ){

			return exe + " version" ;
		}else{
			return exe + " --version" ;
		}
	}() ;

	auto s = Task::await< QStringList >( [ & ](){

		auto s = utility::systemEnvironment() ;

		if( e != "encfs" ){

			return utility::Task( args,-1,s ).splitOutput( ' ',utility::Task::channel::stdOut ) ;
		}else{
			return utility::Task( args,-1,s ).splitOutput( ' ',utility::Task::channel::stdError ) ;
		}
	} ) ;

	auto r = [ & ]()->QString{

		if( e.isOneOf( "cryfs","encfs" ) ){

			if( s.size() > 2 ){

				return s.at( 2 ) ;
			}
		}else{
			if( s.size() > 1 ){

				return s.at( 1 ) ;
			}
		}

		return "N/A" ;
	}() ;

	return r.split( '\n' ).first().remove( ';' ).remove( 'v' ).remove( '\n' ) ;
}

QString checkUpdates::latestVersion( const QByteArray& data)
{
	auto _found_release = []( const QString& e ){

		for( const auto& it : e ){

			/*
			 * A release version has version in format of "A.B.C"
			 *
			 * ie it only has dots and digits. Presence of any other
			 * character makes the release assumed to be a beta/alpha
			 * or prerelease version(something like "A.B.C-rc1" or
			 * "A.B.C.beta6"
			 */
			if( it != '.' && !( it >= '0' && it <= '9' ) ){

				return false ;
			}
		}

		return true ;
	} ;

	for( const auto& it : nlohmann::json::parse( data.constData() ) ){

		auto e = it.find( "tag_name" ) ;

		if( e != it.end() ){

			auto r = QString::fromStdString( e.value() ).remove( 'v' ) ;

			if( _found_release( r ) ){

				return r ;
			}
		}
	}

	return "N/A" ;
}

void checkUpdates::checkForUpdate( backends_t::size_type position )
{
	if( position == m_backends.size() ){

		this->showResult() ;
	}else{
		const auto& e = m_backends[ position ] ;

		position++ ;

		auto exe = e.first ;

		auto f = this->InstalledVersion( exe ) ;

		if( f == "N/A" ){

			m_results += { exe,"N/A","N/A" } ;

			this->checkForUpdate( position ) ;
		}else {
			QUrl url( "https://api.github.com/repos/" + e.second + "/releases" ) ;

			m_networkRequest.setUrl( url ) ;

			m_timer.start() ;

			m_network.get( &m_networkReply,m_networkRequest,
				       [ this,exe,f,position ]( QNetworkReply& e ){

				m_timer.stop() ;

				try{
					m_results += { exe,f,this->latestVersion( e.readAll() ) } ;

				}catch( ... ){

					m_results += { exe,f,"N/A" } ;
				}

				this->checkForUpdate( position ) ;
			} ) ;
		}
	}
}
