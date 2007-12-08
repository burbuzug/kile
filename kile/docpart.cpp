/***************************************************************************
    begin                : Sun Jul 29 2001
    copyright            : (C) 2001 - 2003 by Brachet Pascal, (C) 2004 by Jeroen Wijnhout
    email                : Jeroen.Wijnhout@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "docpart.h"

#include <kconfig.h>
#include <kstandardaction.h>
#include <kglobal.h>
#include "kiledebug.h"
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kmimetype.h>
#include <kmimetypetrader.h>
#include <ktrader.h>
#include <krun.h>
#include <khtml_part.h>
#include <khtml_settings.h>

DocumentationViewer::DocumentationViewer(QWidget *parent) : KHTMLPart(parent, parent, BrowserViewGUI)
{
	m_hpos = 0;
#ifdef __GNUC__
#warning Check why this KConfig object is needed at line 33!
#endif
//FIXME: check for KDE4
//	KConfig konqConfig("konquerorrc");
//	konqConfig.setGroup("HTML Settings");
	//const KHTMLSettings * set = settings();
	//( const_cast<KHTMLSettings *>(set) )->init( &konqConfig, false );
	QString rc = KGlobal::dirs()->findResource("appdata", "docpartui.rc");
	setXMLFile(rc);
	(void) KStandardAction::back(this, SLOT(back()), (QObject*)actionCollection());
	(void) KStandardAction::forward(this, SLOT(forward()), (QObject*)actionCollection());
	(void) KStandardAction::home(this, SLOT(home()), (QObject*)actionCollection());
}

DocumentationViewer::~DocumentationViewer()
{
}

bool DocumentationViewer::urlSelected(const QString &url, int button, int state, const QString &_target, const KParts::OpenUrlArguments &args, const KParts::BrowserArguments & /* browserArgs */)
{
	KUrl cURL = completeURL(url);
	QString mime = KMimeType::findByUrl(cURL).data()->name();

	//load this URL in the embedded viewer if KHTML can handle it, or when mimetype detection failed
	KService::Ptr service = KService::serviceByDesktopName("khtml");
	if ( ( mime == KMimeType::defaultMimeType() ) || (service && service->hasServiceType(mime)) )
	{
		KHTMLPart::urlSelected(url, button, state, _target, args);
		openUrl(cURL) ;
		addToHistory(cURL.url());
	}
	//KHTML can't handle it, look for an appropriate application
	else
	{
		KService::List offers = KMimeTypeTrader::self()->query(mime, "Type == 'Application'");
		if (offers.isEmpty()) {
			KMessageBox::error((QWidget*)this, i18n("No KDE service found for the MIME type \"%1\".", mime));
			return false;
		}
		KUrl::List lst;
		lst.append(cURL);
		KRun::run(*(offers.first()), lst, (QWidget*)this);
	}
	return true;
}

void DocumentationViewer::home()
{
	if (!m_history.isEmpty()) {
		openUrl( KUrl(m_history.first()) );
	}
}

void DocumentationViewer::forward()
{
	if ( forwardEnable() ) 
	{
		++m_hpos;
		openUrl(KUrl(m_history[m_hpos]));
		emit updateStatus( backEnable() , forwardEnable() );
	}
}


void DocumentationViewer::back()
{
	if ( backEnable() ) {
		--m_hpos;
		openUrl(KUrl(m_history[m_hpos]));
		emit updateStatus( backEnable() , forwardEnable() );
	}
}


void DocumentationViewer::addToHistory( const QString & url )
{
	if ( m_history.count() > 0 )
	{
		while ( m_hpos < m_history.count()-1  )
				m_history.pop_back();
	}
	
	if ( !m_history.isEmpty() ) ++m_hpos;
	
	m_history.append(url);
	
	m_hpos = m_history.count()-1;
	emit updateStatus( backEnable() , forwardEnable() );
}


bool DocumentationViewer::backEnable()
{
	return m_hpos > 0;
}


bool DocumentationViewer::forwardEnable()
{
	return m_hpos < m_history.count()-1;
}

#include "docpart.moc"
