/*********************************************************************************************
    begin                : Sun Jul 20 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                           (C) 2005-2007 by Holger Danielsson (holger.danielsson@versanet.de)
                           (C) 2006 by Michel Ludwig (michel.ludwig@kdemail.net)
 *********************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// 2005-11-02: dani
//  - cleaning up source of central function updateStruct()
//      - always use 'else if', because all conditions are exclusive or
//      - most often used commands are at the top
//  - add some new types of elements (and levels) for the structure view
//  - new commands, which are passed to the structure listview:
//       \includegraphics, \caption
//  - all user defined commands for labels are recognized
//  - changed folder name of KileStruct::BibItem to "bibs", so that "refs"
//    is still unused and can be used for references (if wanted)
//  - \begin, \end to gather all environments. But only figure and table
//    environments are passed to the structure view

// 2005-11-26: dani
//  - add support for \fref, \Fref and \eqref references commands

// 2005-12-07: dani
//  - add support to enable and disable some structure view items

// 2006-01-16 tbraun
// - fix #59945 Now we call (through a signal ) project->buildProjectTree so the bib files are correct,
//   and therefore the keys in \cite completion

// 2006-02-09 tbraun/dani
// - fix #106261#4 improved parsing of (optional) command parameters
// - all comments are removed

//2006-09-09 mludwig
// - generalising the different document types

//2007-02-15
// - signal foundItem() not only sends the cursor position of the parameter,
//   but also the real cursor position of the command

// 2007-03-12 dani
//  - use KileDocument::Extensions

// 2007-03-24 dani
// - preliminary minimal support for Beamer class

// 2007-03-25 dani
// - merge labels and sections in document structure view as user configurable option

// 2007-04-06 dani
// - add TODO/FIXME section to structure view

#include "kiledocumentinfo.h"

#include <qfileinfo.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qregexp.h>
#include <qdatetime.h>

#include <kio/netaccess.h>
#include <kconfig.h>
#include <klocale.h>
#include <kapplication.h>
#include "kiledebug.h"
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <kglobal.h>

#include "codecompletion.h"
#include "kileuntitled.h"
#include "kileconfig.h"

namespace KileDocument
{

bool Info::containsInvalidCharacters(const KUrl& url)
{
	QString filename = url.fileName();
	return filename.contains(" ") || filename.contains("~") || filename.contains("$") || filename.contains("#");
}

KUrl Info::repairInvalidCharacters(const KUrl& url, bool checkForFileExistence /* = true */)
{
	KUrl ret(url);
	do {
		bool isOK;
		QString newURL = KInputDialog::getText(
			i18n("Invalid Characters"),
			i18n("The filename contains invalid characters ($~ #).<br>Please provide \
				another one, or click \"Cancel\" to save anyway."),
			ret.fileName(),
			&isOK);
		if(!isOK)
			break;
		ret.setFileName(newURL);
	} while(containsInvalidCharacters(ret));

	return (checkForFileExistence ? renameIfExist(ret) : ret);
}

KUrl Info::renameIfExist(const KUrl& url)
{
	KUrl ret(url);
	while ( KIO::NetAccess::exists(url, true, kapp->mainWidget()) ) // check for writing possibility
	{
		bool isOK;
		QString newURL = KInputDialog::getText(
			i18n("File Already Exists"),
			i18n("A file with filename '%1' already exists.<br>Please provide \
				another one, or click \"Cancel\" to overwrite it.", ret.fileName()),
			ret.fileName(),
			&isOK);
		if(!isOK)
			break;
		ret.setFileName(newURL);
	}
	return ret;
}

KUrl Info::repairExtension(const KUrl& url, bool checkForFileExistence /* = true */)
{
	KUrl ret(url);

	QString filename = url.fileName();
	if(filename.contains(".") && filename[0] != '.') // There already is an extension
		return ret;

	if(KMessageBox::Yes == KMessageBox::questionYesNo(NULL,
		i18n("The given filename has no extension; do you want one to be automatically added?"),
		i18n("Missing Extension"),
		KStandardGuiItem::yes(),
		KStandardGuiItem::no(),
		"AutomaticallyAddExtension"))
	{
		ret.setFileName(filename + ".tex");
	}
	return (checkForFileExistence ? renameIfExist(ret) : ret);
}

KUrl Info::makeValidTeXURL(const KUrl & url, bool istexfile, bool checkForFileExistence)
{
	KUrl newURL(url);

	//add a .tex extension
	if ( ! istexfile )
		newURL = repairExtension(newURL, checkForFileExistence);

	//remove characters TeX does not accept, make sure the newURL does not exists yet
	if(containsInvalidCharacters(newURL))
		newURL = repairInvalidCharacters(newURL, checkForFileExistence);

	return newURL;
}

Info::Info() : m_bIsRoot(false), m_config(KGlobal::config().data()), documentTypePromotionAllowed(true)
{
	updateStructLevelInfo();
}

Info::~Info(void)
{
	KILE_DEBUG() << "DELETING DOCINFO" << m_url.path() << endl;
}

void Info::updateStructLevelInfo()
{
	KILE_DEBUG() << "===void Info::updateStructLevelInfo()===" << endl;
	// read config for structureview items
	m_showStructureLabels = KileConfig::svShowLabels();
	m_showStructureReferences = KileConfig::svShowReferences();
	m_showStructureBibitems = KileConfig::svShowBibitems();
	m_showStructureGraphics = KileConfig::svShowGraphics();
	m_showStructureFloats = KileConfig::svShowFloats();
	m_showStructureInputFiles = KileConfig::svShowInputFiles();
	m_showStructureTodo = KileConfig::svShowTodo();
	m_showSectioningLabels = KileConfig::svShowSectioningLabels();
	m_openStructureLabels = KileConfig::svOpenLabels();
	m_openStructureReferences = KileConfig::svOpenReferences();
	m_openStructureBibitems = KileConfig::svOpenBibitems();
	m_openStructureTodo = KileConfig::svOpenTodo();
}

void Info::setBaseDirectory(const KUrl& url)
{
	KILE_DEBUG() << "===void Info::setBaseDirectory(const KUrl&" << url << ")===" << endl;
	m_baseDirectory = url;
}

const KUrl& Info::getBaseDirectory() const
{
	return m_baseDirectory;
}

bool Info::isTextDocument()
{
	return false;
}

Type Info::getType()
{
	return Undefined;
}

QString Info::getFileFilter() const
{
	return QString::null;
}

bool Info::isDocumentTypePromotionAllowed()
{
	return documentTypePromotionAllowed;
}

void Info::setDocumentTypePromotionAllowed(bool b)
{
	documentTypePromotionAllowed = b;
}

KUrl Info::url()
{
	return m_url;
}

void Info::count(const QString line, long *stat)
{
	QChar c;
	int state = stStandard;
	bool word = false; // we are in a word

	for (int p = 0; p < line.length(); ++p) {
		c = line[p];

		switch(state) {
		case stStandard	:
#ifdef __GNUC__
#warning Don't use QChar in switch statement!
#endif
//FIXME: change for KDE4
			switch(c.toAscii()) {
				case TEX_CAT0	:
					state = stControlSequence;
					++stat[1];

					//look ahead to avoid counting words like K\"ahler as two words
					if (! line[p+1].isPunct() || line[p+1] == '~' || line[p+1] == '^' )
						word=false;
				break;

				case TEX_CAT14 :
					state=stComment;
				break;

				default:
					if (c.isLetterOrNumber())
					{
						//only start new word if first character is a letter (42test is still counted as a word, but 42.2 not)
						if (c.isLetter() && !word)
						{
							word=true;
							++stat[3];
						}
						++stat[0];
					}
					else
					{
						++stat[2];
						word = false;
					}

				break;
			}
		break;

		case stControlSequence :
			if ( c.isLetter() )
			{
			// "\begin{[a-zA-z]+}" is an environment, and you can't define a command like \begin
				if ( line.mid(p,5) == "begin" )
				{
					++stat[5];
					state = stEnvironment;
					stat[1] +=5;
					p+=4; // after break p++ is executed
				}
				else if ( line.mid(p,3) == "end" )
				{
					stat[1] +=3;
					state = stEnvironment;
					p+=2;
				} // we don't count \end as new environment, this can give wrong results in selections
				else
				{
					++stat[4];
					++stat[1];
					state = stCommand;
				}
			}
			else
			{
				++stat[4];
				++stat[1];
				state = stStandard;
			}
		break;

		case stCommand :
			if ( c.isLetter() )
				++stat[1];
			else if ( c == TEX_CAT0 )
			{
				++stat[1];
				state=stControlSequence;
			}
			else if ( c == TEX_CAT14 )
				state=stComment;
			else
			{
				++stat[2];
				state = stStandard;
			}
		break;

		case stEnvironment :
			if ( c == TEX_CAT2  ) // until we find a closing } we have an environment
			{
				++stat[1];
				state=stStandard;
			}
			else if ( c == TEX_CAT14 )
				state=stComment;
			else
				++stat[1];

		break;

		case stComment : // if we get a selection the line possibly contains \n and so the comment is only valid till \n and not necessarily till line.length()
			if ( c == '\n')
			{
			++stat[2]; // \n was counted as punctuation in the old implementation
			state=stStandard;
			word=false;
			}
		break;

		default :
			kWarning() << "Unhandled state in getStatistics " << state << endl;
		break;
		}
	}
}

QString Info::lastModifiedFile(const QStringList *list /* = 0L */)
{
	KILE_DEBUG() << "==QString Info::lastModifiedFile()=====" << endl;
	QFileInfo fileinfo ( url().path() );
	QString basepath = fileinfo.absolutePath(), last = fileinfo.absoluteFilePath();
	QDateTime time ( fileinfo.lastModified() );

	if ( list == 0L ) list = &m_deps;

	KILE_DEBUG() << "\t" << fileinfo.absoluteFilePath() << " : " << time.toString() << endl;
	for(int i = 0; i < list->count(); ++i) {
		fileinfo.setFile( basepath + '/' + (*list)[i] );
		KILE_DEBUG() << "\t" << fileinfo.absoluteFilePath() << " : " << fileinfo.lastModified().toString() << endl;
		if ( fileinfo.lastModified() >  time )
		{
			time = fileinfo.lastModified();
			last = fileinfo.absoluteFilePath();
			KILE_DEBUG() << "\t\tlater" << endl;
		}
	}

	KILE_DEBUG() << "\treturning " << fileinfo.absoluteFilePath() << endl;
	return last;
}

void Info::updateStruct()
{
	KILE_DEBUG() << "==Info::updateStruct()=======" << endl;
	m_labels.clear();
	m_bibItems.clear();
	m_deps.clear();
	m_bibliography.clear();
	m_packages.clear();
	m_newCommands.clear();
	m_bIsRoot = false;
	m_preamble = QString::null;
}

void Info::updateBibItems()
{
}

void Info::slotCompleted()
{
	emit completed(this);
}


TextInfo::TextInfo(KTextEditor::Document *doc, Extensions *extensions, const QString& defaultHighlightMode) : m_doc(0), m_defaultHighlightMode(defaultHighlightMode)
{
	setDoc(doc);
	if (m_doc)
	{
		KILE_DEBUG() << "TextInfo created for " << m_doc->url() << endl;
	}

 	m_arStatistics = new long[SIZE_STAT_ARRAY];

	if (m_doc)
	{
		m_url = doc->url();
	}
	else
	{
		m_url = KUrl();
	}
	m_extensions = extensions;

}

TextInfo::~TextInfo()
{
	delete [] m_arStatistics;
}


const KTextEditor::Document* TextInfo::getDoc() const
{
	return m_doc;
}

KTextEditor::Document* TextInfo::getDoc()
{
	return m_doc;
}

void TextInfo::setDoc(KTextEditor::Document *doc)
{
	KILE_DEBUG() << "===void TextInfo::setDoc(KTextEditor::Document *doc)===" << endl;

	if(m_doc == doc)
		return;

	detach();
	if(doc)
	{
		m_doc = doc;
		m_url = doc->url();
		connect(m_doc, SIGNAL(fileNameChanged()), this, SLOT(slotFileNameChanged()));
		connect(m_doc, SIGNAL(completed()), this, SLOT(slotCompleted()));
		setHighlightMode(m_defaultHighlightMode);
		installEventFilters();
	}
}

void TextInfo::detach()
{
	if(m_doc)
	{
		m_doc->disconnect(this);
		removeInstalledEventFilters();
	}
	m_doc = 0L;
}

const long* TextInfo::getStatistics()
{
	/* [0] = #c in words, [1] = #c in latex commands and environments,
	   [2] = #c whitespace, [3] = #words, [4] = # latex_commands, [5] = latex_environments */
	m_arStatistics[0]=m_arStatistics[1]=m_arStatistics[2]=m_arStatistics[3]=m_arStatistics[4]=m_arStatistics[5]=0;

	return m_arStatistics;
}

// FIXME for KDE 4.0, rearrange the hole docinfo layout to get rid of this hack
KUrl TextInfo::url()
{
	KUrl url;

	if(m_doc)
		url = m_doc->url();
	else
	{
		QFileInfo info(m_url.path());
		if(info.exists())
			url = m_url;
		else
			url = KUrl();
	}
// 	KILE_DEBUG() << "===KUrl TextInfo::url()===, url is " << url.path() << endl;
	return url;
}

Type TextInfo::getType()
{
	return Text;
}

bool TextInfo::isTextDocument()
{
	return true;
}

void TextInfo::setHighlightMode(const QString &highlight)
{
	KILE_DEBUG() << "==Kile::setHighlightMode(" << m_doc->url() << "," << highlight << " )==================" << endl;

	if (m_doc && !highlight.isEmpty()) {
		m_doc->setHighlightingMode(highlight);
	}
}

void TextInfo::setDefaultHightlightMode(const QString& string)
{
	m_defaultHighlightMode = string;
}

// match a { with the corresponding }
// pos is the position of the {
QString TextInfo::matchBracket(QChar obracket, int &l, int &pos)
{
	QChar cbracket;
	if ( obracket == '{' ) cbracket = '}';
	if ( obracket == '[' ) cbracket = ']';
	if ( obracket == '(' ) cbracket = ')';

	QString line, grab = "";
	int count=0, len;
	++pos;

	TodoResult todo;
	while ( l <= m_doc->lines() )
	{
		line = getTextline(l,todo);
		len = line.length();
		for (int i=pos; i < len; ++i)
		{
			if (line[i] == '\\' && ( line[i+1] == obracket || line[i+1] == cbracket) ) ++i;
			else if (line[i] == obracket) ++count;
			else if (line[i] == cbracket)
			{
				--count;
				if (count < 0)
				{
					pos = i;
					return grab;
				}
			}

			grab += line[i];
		}
		++l;
		pos=0;
	}

	return QString::null;
}

QString TextInfo::getTextline(uint line, TodoResult &todo)
{
	static QRegExp::QRegExp reComments("[^\\\\](%.*$)");

	todo.type = -1;
	QString s = m_doc->line(line);
	if ( ! s.isEmpty() )
	{
		// remove comment lines
		if ( s[0] == '%' )
		{
			searchTodoComment(s,0,todo);
			s = QString::null;
		}
		else
		{
			//remove escaped \ characters
			s.replace("\\\\", "  ");

			//remove comments
			int pos = s.indexOf(reComments);
			if ( pos != -1 )
			{
				searchTodoComment(s,pos,todo);
				s = s.left(reComments.pos(1));
			}
		}
	}
	return s;
}

void TextInfo::searchTodoComment(const QString &s, uint startpos, TodoResult &todo)
{
	static QRegExp::QRegExp reTodoComment("\\b(TODO|FIXME)\\b(:|\\s)?\\s*(.*)");

	if ( s.indexOf(reTodoComment,startpos) != -1 )
	{
		todo.type = ( reTodoComment.cap(1) == "TODO" ) ? KileStruct::ToDo : KileStruct::FixMe;
		todo.colTag = reTodoComment.pos(1);
		todo.colComment = reTodoComment.pos(3);
		todo.comment = reTodoComment.cap(3).trimmed();
	}
}

KTextEditor::View* TextInfo::createView(QWidget *parent, const char *name)
{
	if(!m_doc)
	{
		return NULL;
	}
	KTextEditor::View *view = m_doc->createView(parent);
	installEventFilters(view);
	return view;
}

void TextInfo::slotFileNameChanged()
{
	emit urlChanged(this, url());
}

void TextInfo::installEventFilters(KTextEditor::View* /* view */)
{
	/* do nothing */
}

void TextInfo::removeInstalledEventFilters(KTextEditor::View* /* view */)
{
	/* do nothing */
}

void TextInfo::installEventFilters()
{
	if(!m_doc)
	{
		return;
	}
	QList<KTextEditor::View*> views = m_doc->views();
	for(QList<KTextEditor::View*>::iterator i = views.begin(); i != views.end(); ++i) {
		installEventFilters(*i);
	}
}

void TextInfo::removeInstalledEventFilters()
{
	if(!m_doc)
	{
		return;
	}
	QList<KTextEditor::View*> views = m_doc->views();
	for(QList<KTextEditor::View*>::iterator i = views.begin(); i != views.end(); ++i) {
		removeInstalledEventFilters(*i);
	}
}


LaTeXInfo::LaTeXInfo (KTextEditor::Document *doc, Extensions *extensions, LatexCommands *commands, const QObject* eventFilter) : TextInfo(doc, extensions, "LaTeX"), m_commands(commands), m_eventFilter(eventFilter)
{
	documentTypePromotionAllowed = false;
	updateStructLevelInfo();
}

LaTeXInfo::~LaTeXInfo()
{
}

const long* LaTeXInfo::getStatistics()
{
	/* [0] = #c in words, [1] = #c in latex commands and environments,
	   [2] = #c whitespace, [3] = #words, [4] = # latex_commands, [5] = latex_environments */
	m_arStatistics[0]=m_arStatistics[1]=m_arStatistics[2]=m_arStatistics[3]=m_arStatistics[4]=m_arStatistics[5]=0;
	QString line;
#ifdef __GNUC__
#warning Change the signature of the getStatistics() function to take a view as parameter!
#endif
//FIXME: port for KDE4
/*
	if ( m_doc && m_doc->hasSelection() )
	{
		line = m_doc->selection();
		KILE_DEBUG() << "getStat : line : " << line << endl;
		count(line, m_arStatistics);
	}
	else if (m_doc)
*/
	for (int l=0; l < m_doc->lines(); ++l) {
		line = m_doc->line(l);
		KILE_DEBUG() << "getStat : line : " << line << endl;
		count(line, m_arStatistics);
	}
	return m_arStatistics;
}

Type LaTeXInfo::getType()
{
	return LaTeX;
}

QString LaTeXInfo::getFileFilter() const
{
	return m_extensions->latexDocumentFileFilter() + '\n' + m_extensions->latexPackageFileFilter();
}

void LaTeXInfo::updateStructLevelInfo() {

	KILE_DEBUG() << "===void LaTeXInfo::updateStructLevelInfo()===" << endl;

	// read config stuff
	Info::updateStructLevelInfo();

	// clear all entries
	m_dictStructLevel.clear();

	//TODO: make sectioning and bibliography configurable

	// sectioning
	m_dictStructLevel["\\part"]=KileStructData(1, KileStruct::Sect, "part");
	m_dictStructLevel["\\chapter"]=KileStructData(2, KileStruct::Sect, "chapter");
	m_dictStructLevel["\\section"]=KileStructData(3, KileStruct::Sect, "section");
	m_dictStructLevel["\\subsection"]=KileStructData(4, KileStruct::Sect, "subsection");
	m_dictStructLevel["\\subsubsection"]=KileStructData(5, KileStruct::Sect, "subsubsection");
	m_dictStructLevel["\\paragraph"]=KileStructData(6, KileStruct::Sect, "subsubsection");
	m_dictStructLevel["\\subparagraph"]=KileStructData(7, KileStruct::Sect, "subsubsection");
	m_dictStructLevel["\\bibliography"]=KileStructData(0,KileStruct::Bibliography, "viewbib");

	// hidden commands
	m_dictStructLevel["\\usepackage"]=KileStructData(KileStruct::Hidden, KileStruct::Package);
	m_dictStructLevel["\\newcommand"]=KileStructData(KileStruct::Hidden, KileStruct::NewCommand);
	m_dictStructLevel["\\newlength"]=KileStructData(KileStruct::Hidden, KileStruct::NewCommand);
	m_dictStructLevel["\\newenvironment"]=KileStructData(KileStruct::Hidden, KileStruct::NewEnvironment);
	m_dictStructLevel["\\addunit"]=KileStructData(KileStruct::Hidden, KileStruct::NewCommand); // hack to get support for the fancyunits package until we can configure the commands in the gui (tbraun)
	m_dictStructLevel["\\DeclareMathOperator"]=KileStructData(KileStruct::Hidden, KileStruct::NewCommand); // amsmath package
	m_dictStructLevel["\\caption"]=KileStructData(KileStruct::Hidden,KileStruct::Caption);

	// bibitems
	if ( m_showStructureBibitems )
	{
		m_dictStructLevel["\\bibitem"]= KileStructData(KileStruct::NotSpecified, KileStruct::BibItem, QString::null, "bibs");
	}

	// graphics
	if ( m_showStructureGraphics )
	{
		m_dictStructLevel["\\includegraphics"]=KileStructData(KileStruct::Object,KileStruct::Graphics, "graphics");
	}

	// float environments
	if ( m_showStructureFloats )
	{
		m_dictStructLevel["\\begin"]=KileStructData(KileStruct::Object,KileStruct::BeginEnv);
		m_dictStructLevel["\\end"]=KileStructData(KileStruct::Hidden,KileStruct::EndEnv);

		// some entries, which could never be found (but they are set manually)
		m_dictStructLevel["\\begin{figure}"]=KileStructData(KileStruct::Object,KileStruct::BeginFloat, "frame_image");
		m_dictStructLevel["\\begin{table}"]=KileStructData(KileStruct::Object,KileStruct::BeginFloat, "frame_spreadsheet");
		m_dictStructLevel["\\end{float}"]=KileStructData(KileStruct::Hidden,KileStruct::EndFloat);
	}

	// preliminary minimal beamer support
	m_dictStructLevel["\\frame"]=KileStructData(KileStruct::Object, KileStruct::BeamerFrame, "beamerframe");
	m_dictStructLevel["\\frametitle"]=KileStructData(KileStruct::Hidden, KileStruct::BeamerFrametitle);
	m_dictStructLevel["\\begin{frame}"]=KileStructData(KileStruct::Object, KileStruct::BeamerBeginFrame, "beamerframe");
	m_dictStructLevel["\\end{frame}"]=KileStructData(KileStruct::Hidden, KileStruct::BeamerEndFrame);
	m_dictStructLevel["\\begin{block}"]=KileStructData(KileStruct::Object, KileStruct::BeamerBeginBlock, "beamerblock");

	// add user defined commands

	QStringList list;
	QStringList::ConstIterator it;

	// labels, we also gather them
	m_commands->commandList(list,KileDocument::CmdAttrLabel,false);
	for ( it=list.begin(); it != list.end(); ++it )
		m_dictStructLevel[*it]= KileStructData(KileStruct::NotSpecified, KileStruct::Label, QString::null, "labels");

	// input files
	if ( m_showStructureInputFiles )
	{
		m_commands->commandList(list,KileDocument::CmdAttrIncludes,false);
		for ( it=list.begin(); it != list.end(); ++it )
			m_dictStructLevel[*it]= KileStructData(KileStruct::File, KileStruct::Input, "include");
	}

	// references
	if ( m_showStructureReferences )
	{
		m_commands->commandList(list,KileDocument::CmdAttrReference,false);
		for ( it=list.begin(); it != list.end(); ++it )
			m_dictStructLevel[*it]= KileStructData(KileStruct::Hidden, KileStruct::Reference);
	}
}

void LaTeXInfo::installEventFilters(KTextEditor::View *view)
{
#ifdef __GNUC__
#warning Commenting the event filter stuff out for now!
#endif
//FIXME: port for KDE4
// 	view->focusProxy()->installEventFilter(m_eventFilter);
}

void LaTeXInfo::removeInstalledEventFilters(KTextEditor::View *view)
{
#ifdef __GNUC__
#warning Commenting the event filter stuff out for now!
#endif
//FIXME: port for KDE4
// 	view->focusProxy()->removeEventFilter(m_eventFilter);
}

BracketResult LaTeXInfo::matchBracket(int &l, int &pos)
{
	BracketResult result;
	TodoResult todo;

	if ( m_doc->line(l)[pos] == '[' )
	{
		result.option = TextInfo::matchBracket('[', l, pos);
		int p = 0;
		while ( l < m_doc->lines() )
		{
			if ( (p = getTextline(l,todo).indexOf('{', pos)) != -1 )
			{
				pos = p;
				break;
			}
			else
			{
				pos = 0;
				++l;
			}
		}
	}

	if ( m_doc->line(l)[pos] == '{' )
	{
		result.line = l;
		result.col = pos;
		result.value  = TextInfo::matchBracket('{', l, pos);
	}

	return result;
}

//FIXME refactor, clean this mess up
void LaTeXInfo::updateStruct()
{
	KILE_DEBUG() << "==void TeXInfo::updateStruct: (" << url() << ")=========" << endl;

	if ( getDoc() == 0L )
		return;

	Info::updateStruct();

	QMap<QString,KileStructData>::const_iterator it;
	static QRegExp::QRegExp reCommand("(\\\\[a-zA-Z]+)\\s*\\*?\\s*(\\{|\\[)");
	static QRegExp::QRegExp reRoot("\\\\documentclass|\\\\documentstyle");
	static QRegExp::QRegExp reBD("\\\\begin\\s*\\{\\s*document\\s*\\}");
	static QRegExp::QRegExp reReNewCommand("\\\\renewcommand.*$");
	static QRegExp::QRegExp reNumOfParams("\\s*\\[([1-9]+)\\]");
	static QRegExp::QRegExp reNumOfOptParams("\\s*\\[([1-9]+)\\]\\s*\\[([^\\{]*)\\]"); // the quantifier * isn't used by mistake, because also emtpy optional brackets are correct.

	int teller=0, tagStart, bd = 0;
	int tagEnd, tagLine = 0, tagCol = 0;
	int tagStartLine = 0, tagStartCol = 0;
	BracketResult result;
	QString m, s, shorthand;
	bool foundBD = false; // found \begin { document }
	bool fire = true; //whether or not we should emit a foundItem signal
	bool fireSuspended; // found an item, but it should not be fired (this time)
	TodoResult todo;

	for(int i = 0; i < m_doc->lines(); ++i) {
		if (teller > 100)
		{
			teller=0;
			kapp->processEvents();
		}
		else
			++teller;

		tagStart=tagEnd=0;
		fire = true;
		s = getTextline(i,todo);
		if ( todo.type!=-1 && m_showStructureTodo )
		{
			QString folder = ( todo.type == KileStruct::ToDo ) ? "todo" : "fixme";
			emit( foundItem(todo.comment, i+1, todo.colComment, todo.type, KileStruct::Object, i+1, todo.colTag, QString::null, folder) );
		}


		if ( s.isEmpty() )
			continue;

		//ignore renewcommands
		s.replace(reReNewCommand, "");

		//find all commands in this line
		while (tagStart != -1)
		{
			if ( (!foundBD) && ( (bd = s.indexOf(reBD, tagEnd)) != -1))
			{
				KILE_DEBUG() << "\tfound \\begin{document}" << endl;
				foundBD = true;
				if ( bd == 0 ) m_preamble = m_doc->text(KTextEditor::Range(0, 0, i - 1, m_doc->line(i - 1).length()));
				else m_preamble = m_doc->text(KTextEditor::Range(0, 0, i, bd));
			}

			if ((!foundBD) && (s.indexOf(reRoot, tagEnd) != -1))
			{
				KILE_DEBUG() << "\tsetting m_bIsRoot to true" << endl;
				tagEnd += reRoot.cap(0).length();
				m_bIsRoot = true;
			}

			tagStart = reCommand.search(s,tagEnd);
			m=QString::null;
			shorthand = QString::null;

			if (tagStart != -1)
			{
				tagEnd = tagStart + reCommand.cap(0).length()-1;

				//look up the command in the dictionary
				it = m_dictStructLevel.find(reCommand.cap(1));

				//if it is was a structure element, find the title (or label)
				if (it != m_dictStructLevel.end())
				{
					tagLine = i+1;
					tagCol = tagEnd+1;
					tagStartLine = tagLine;
					tagStartCol = tagStart+1;
					if ( reCommand.cap(1) != "\\frame" )
					{
						result = matchBracket(i, tagEnd);
						m = result.value.trimmed();
						shorthand = result.option.trimmed();
						if ( i >= tagLine ) //matching brackets spanned multiple lines
							s = m_doc->line(i);
						if ( result.line>0 || result.col>0 )
						{
							tagLine = result.line + 1;
							tagCol = result.col + 1;
						}
					//KILE_DEBUG() << "\tgrabbed: " << reCommand.cap(1) << "[" << shorthand << "]{" << m << "}" << endl;
					}
					else
					{
						m = i18n("Frame");
					}
				}

				//title (or label) found, add the element to the listview
				if ( !m.isNull() )
				{
					// no problems so far ...
					fireSuspended = false;

					// remove trailing ./
					if ( (*it).type & (KileStruct::Input | KileStruct::Graphics) )
					{
						if ( m.left(2) == "./" )
							m = m.mid(2,m.length()-2);
					}
					// update parameter for environments, because only
					// floating environments and beamer frames are passed
					if ( (*it).type == KileStruct::BeginEnv )
					{
						if ( m=="figure" || m=="table" )
						{
							it = m_dictStructLevel.find("\\begin{" + m +'}');
						}
						else if ( m == "frame" )
						{
							it = m_dictStructLevel.find("\\begin{frame}");
							m = i18n("Frame");
						}
						else if ( m=="block" || m=="exampleblock" || m=="alertblock")
						{
							const QString untitledBlockDisplayName = i18n("Untitled Block");
							it = m_dictStructLevel.find("\\begin{block}");
							if ( s.at(tagEnd+1) == '{' )
							{
								tagEnd++;
								result = matchBracket(i, tagEnd);
								m = result.value.trimmed();
								if(m.isEmpty()) {
									m = untitledBlockDisplayName;
								}
							}
							else
								m = untitledBlockDisplayName;
						}
						else
							fireSuspended = true;    // only floats and beamer frames, no other environments
					}

					// tell structure view that a floating environment or a beamer frame must be closed
					else if ( (*it).type == KileStruct::EndEnv )
					{
						if ( m=="figure" || m=="table")
						{
							it = m_dictStructLevel.find("\\end{float}");
						}
						else if ( m == "frame" )
						{
							it = m_dictStructLevel.find("\\end{frame}");
						}
						else
							fireSuspended = true;          // only floats, no other environments
					}

					// sectioning commands
					else if ( (*it).type == KileStruct::Sect )
					{
						if ( ! shorthand.isNull() )
							m = shorthand;
					}

					// update the label list
					else if ( (*it).type == KileStruct::Label )
					{
						m_labels.append(m);
						// label entry as child of sectioning
						if ( m_showSectioningLabels )
						{
							emit( foundItem(m, tagLine, tagCol, KileStruct::Label, KileStruct::Object, tagStartLine, tagStartCol, "label", "root") );
							fireSuspended = true;
						}
					}

					// update the references list
					else if ( (*it).type == KileStruct::Reference )
					{
						// m_references.append(m);
						//fireSuspended = true;          // don't emit references
					}

					// update the dependencies
					else if ((*it).type == KileStruct::Input)
					{
						// \input- or \include-commands can be used without extension. So we check
						// if an extension exists. If not the default extension is added
						// ( LaTeX reference says that this is '.tex'). This assures that
						// all files, which are listed in the structure view, have an extension.
						QString ext = QFileInfo(m).completeSuffix();
						if ( ext.isEmpty() )
							m += m_extensions->latexDocumentDefault();
						m_deps.append(m);
					}

					// update the referenced Bib files
					else  if( (*it).type == KileStruct::Bibliography ) {
						KILE_DEBUG() << "===TeXInfo::updateStruct()===appending Bibiliograph file(s) " << m << endl;

						QStringList bibs = m.split(",");
						QString biblio;

						// assure that all files have an extension
						QString bibext = m_extensions->bibtexDefault();
						int bibextlen = bibext.length();

						uint cumlen = 0;
						int nextbib = 0; // length to add to jump to the next bibliography
						for (int b = 0; b < bibs.count(); ++b) {
							nextbib = 0;
							biblio=bibs[b];
							m_bibliography.append(biblio);
							if ( biblio.left(2) == "./" )
							{	nextbib += 2;
								biblio = biblio.mid(2,biblio.length()-2);
							}
							if ( biblio.right(bibextlen) != bibext )
							{
								biblio += bibext;
								nextbib -= bibextlen;
							}
							m_deps.append(biblio);
							emit( foundItem(biblio, tagLine, tagCol+cumlen, (*it).type, (*it).level, tagStartLine, tagStartCol, (*it).pix, (*it).folder) );
							cumlen += biblio.length() + 1 + nextbib;
						}
						fire = false;
					}

					// update the bibitem list
					else if ( (*it).type == KileStruct::BibItem )
					{
						//KILE_DEBUG() << "\tappending bibitem " << m << endl;
						m_bibItems.append(m);
					}

					// update the package list
					else if ( (*it).type == KileStruct::Package )
					{
						QStringList pckgs = m.split(",");
						uint cumlen = 0;
						for(int p = 0; p < pckgs.count(); ++p) {
							QString package = pckgs[p].trimmed();
							if ( ! package.isEmpty() ) {
								m_packages.append(package);
								// hidden, so emit is useless
								// emit( foundItem(package, tagLine, tagCol+cumlen, (*it).type, (*it).level, tagStartLine, tagStartCol, (*it).pix, (*it).folder) );
								cumlen += package.length() + 1;
							}
						}
						fire = false;
					}

					// newcommand found, add it to the newCommands list
					else if ( (*it).type & ( KileStruct::NewCommand | KileStruct::NewEnvironment ) )
					{
						QString optArg, mandArgs;

						//find how many parameters this command takes
						if ( s.indexOf(reNumOfParams, tagEnd + 1) != -1 )
						{
							bool ok;
							int noo = reNumOfParams.cap(1).toInt(&ok);

							if ( ok )
							{
								if(s.indexOf(reNumOfOptParams, tagEnd + 1) != -1)
								{
									KILE_DEBUG() << "Opt param is " << reNumOfOptParams.cap(2) << "%EOL" << endl;
									noo--; // if we have an opt argument, we have one mandatory argument less, and noo=0 can't occur because then latex complains (and we don't macht them with reNumOfParams either)
									optArg = '[' + reNumOfOptParams.cap(2) + ']';
								}

								for ( int noo_index = 0; noo_index < noo; ++noo_index)
								{
									mandArgs +=  '{' + s_bullet + '}';
								}

							}
							if( !optArg.isEmpty() )
							{
								if( (*it).type == KileStruct::NewEnvironment)
								{
									m_newCommands.append(QString("\\begin{%1}%2%3").arg(m).arg(optArg).arg(mandArgs));
								}
								else
									m_newCommands.append(m + optArg + mandArgs);
							}
						}
						if( (*it).type == KileStruct::NewEnvironment)
						{
							m_newCommands.append(QString("\\begin{%1}%3").arg(m).arg(mandArgs));
							m_newCommands.append(QString("\\end{%1}").arg(m));
						}
						else
							m_newCommands.append(m + mandArgs);

						//FIXME  set tagEnd to the end of the command definition
						break;
					}
					// and some other commands, which don't need special actions:
					// \caption, ...

					// KILE_DEBUG() << "\t\temitting: " << m << endl;
					if ( fire && !fireSuspended )
						emit( foundItem(m, tagLine, tagCol, (*it).type, (*it).level, tagStartLine, tagStartCol, (*it).pix, (*it).folder) );
				} //if m
			} // if tagStart
		} // while tagStart
	} //for

	checkChangedDeps();
	emit(doneUpdating());
	emit(isrootChanged(isLaTeXRoot()));
}

void LaTeXInfo::checkChangedDeps()
{
	if( m_depsPrev != m_deps )
	{
		KILE_DEBUG() << "===void LaTeXInfo::checkChangedDeps()===, deps have changed"<< endl;
		emit(depChanged());
		m_depsPrev = m_deps;
	}
}

BibInfo::BibInfo (KTextEditor::Document *doc, Extensions *extensions, LatexCommands* /* commands */) : TextInfo(doc, extensions, "BibTeX")
{
	documentTypePromotionAllowed = false;
}

BibInfo::~BibInfo()
{
}

bool BibInfo::isLaTeXRoot()
{
	return false;
}

void BibInfo::updateStruct()
{
	if ( getDoc() == 0L ) return;

	Info::updateStruct();

	KILE_DEBUG() << "==void BibInfo::updateStruct()========" << endl;

	static QRegExp::QRegExp reItem("^(\\s*)@([a-zA-Z]+)");
	static QRegExp::QRegExp reSpecial("string|preamble|comment");

	QString s, key;
	int col = 0, startcol, startline = 0;

	for(int i = 0; i < m_doc->lines(); ++i) {
		s = m_doc->line(i);
		if ( (s.indexOf(reItem) != -1) && !reSpecial.exactMatch(reItem.cap(2).toLower()) )
		{
			KILE_DEBUG() << "found: " << reItem.cap(2) << endl;
			//start looking for key
			key = "";
			bool keystarted = false;
			int state = 0;
			startcol = reItem.cap(1).length();
			col  = startcol + reItem.cap(2).length();

			while ( col <  static_cast<int>(s.length()) )
			{
				++col;
				if ( col == static_cast<int>(s.length()) )
				{
					do
					{
						++i;
						s = m_doc->line(i);
					} while  ( (s.length() == 0) && (i < m_doc->lines()) );

					if ( i == m_doc->lines() ) break;
					col = 0;
				}

				if ( state == 0 )
				{
					if ( s[col] == '{' ) state = 1;
					else if ( ! s[col].isSpace() ) break;
				}
				else if ( state == 1 )
				{
					if ( s[col] == ',' )
					{
						key = key.trimmed();
						KILE_DEBUG() << "found: " << key << endl;
						m_bibItems.append(key);
						emit(foundItem(key, startline+1, startcol, KileStruct::BibItem, 0, startline+1, startcol, "viewbib", reItem.cap(2).toLower()) );
						break;
					}
					else
					{
						key += s[col];
						if (!keystarted) { startcol = col; startline = i; }
						keystarted=true;
					}
				}
			}
		}
	}

	emit(doneUpdating());
}

Type BibInfo::getType()
{
	return BibTeX;
}

QString BibInfo::getFileFilter() const
{
	return m_extensions->bibtexFileFilter();
}

ScriptInfo::ScriptInfo(KTextEditor::Document *doc, Extensions *extensions ) : TextInfo(doc, extensions, "JavaScript")
{
	documentTypePromotionAllowed = false;
}

ScriptInfo::~ScriptInfo()
{
}

bool ScriptInfo::isLaTeXRoot()
{
	return false;
}

Type ScriptInfo::getType()
{
	return Script;
}

QString ScriptInfo::getFileFilter() const
{
	return m_extensions->scriptFileFilter();
}

}

#include "kiledocumentinfo.moc"
