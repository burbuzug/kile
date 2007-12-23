/***************************************************************************
                           mathenvdialog.cpp
----------------------------------------------------------------------------
    date                 : Dec 06 2005
    version              : 0.21
    copyright            : (C) 2005 by Holger Danielsson
    email                : holger.danielsson@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mathenvdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

#include <KLocale>

#include "codecompletion.h"
#include "kiledebug.h"
#include "kileedit.h"

namespace KileDialog
{

MathEnvironmentDialog::MathEnvironmentDialog(QWidget *parent, KConfig *config, KileInfo *ki, KileDocument::LatexCommands *commands)
	: Wizard(config, parent), m_ki(ki), m_latexCommands(commands)
{
	QWidget *page = new QWidget(this);
	setMainWidget(page);
	setCaption(i18n("Math Environments"));

	QVBoxLayout *vbox = new QVBoxLayout();
	vbox->setMargin(0);
	vbox->setSpacing(KDialog::spacingHint());
	page->setLayout(vbox);

	// environment groupbox
	QGroupBox *envgroup = new QGroupBox(i18n("Environment"), page);

	m_lbEnvironment = new QLabel(i18n("&Name:"), envgroup);
	m_lbStarred = new QLabel(i18n("Without n&umbering:"), envgroup);
	m_lbRows = new QLabel(i18n("Number of &rows:"), envgroup);
	m_lbCols = new QLabel(i18n("Number of c&ols:"), envgroup);
	m_lbSpace = new QLabel(i18n("Space command\nto &separate groups:"), envgroup);
	m_lbTabulator = new QLabel(i18n("Standard &tabulator:"), envgroup);
	m_lbDisplaymath = new QLabel(i18n("Display&math mode:"), envgroup);
	m_lbBullets = new QLabel(i18n("Use &bullets:"), envgroup);

	QFrame *frame = new QFrame(envgroup);
	frame->setFrameStyle(QFrame::HLine);
	frame->setFrameShadow(QFrame::Sunken);
	frame->setLineWidth(1);

	m_coEnvironment = new QComboBox(envgroup);
	m_cbStarred = new QCheckBox(envgroup);
	m_spRows = new QSpinBox(1, 99, 1, envgroup);
	m_spRows->setValue(3);
	m_spCols = new QSpinBox(1, 49, 1, envgroup);
	m_spCols->setValue(3);
	m_edSpace = new KLineEdit("", envgroup);
	m_coTabulator = new QComboBox(envgroup);
	m_coDisplaymath = new QComboBox(envgroup);
	m_cbBullets = new QCheckBox(envgroup);

	QGridLayout *envlayout = new QGridLayout();
	envlayout->setMargin(KDialog::marginHint());
	envlayout->setSpacing(KDialog::spacingHint());
	envgroup->setLayout(envlayout);
	envlayout->setAlignment(Qt::AlignTop);
	envlayout->addWidget(m_lbEnvironment, 0, 0);
	envlayout->addWidget(m_lbStarred, 1, 0);
	envlayout->addWidget(m_lbRows, 2, 0);
	envlayout->addWidget(m_lbCols, 3, 0);
	envlayout->addWidget(m_lbTabulator, 5, 0);
	envlayout->addWidget(m_lbDisplaymath, 6, 0);
	envlayout->addWidget(m_coEnvironment, 0, 1);
	envlayout->addWidget(m_cbStarred, 1, 1);
	envlayout->addWidget(m_spRows, 2, 1);
	envlayout->addWidget(m_spCols, 3, 1);
	envlayout->addWidget(m_coTabulator, 5, 1);
	envlayout->addWidget(m_coDisplaymath, 6, 1);
	envlayout->addWidget(m_lbSpace, 3, 3);
	envlayout->addWidget(m_lbBullets, 5, 3);
	envlayout->addWidget(m_edSpace, 3, 4);
	envlayout->addWidget(m_cbBullets, 5, 4);
	envlayout->addWidget(frame, 4, 0, 1, 5);
	envlayout->setRowSpacing(4, 30);
	envlayout->setColSpacing(2, 20);
	envlayout->setColStretch(4, 1);

	// add widgets
	vbox->addWidget(envgroup);
	vbox->addStretch(1);

	m_lbEnvironment->setBuddy(m_coEnvironment);
	m_lbStarred->setBuddy(m_cbStarred);
	m_lbRows->setBuddy(m_spRows);
	m_lbCols->setBuddy(m_spCols);
	m_lbSpace->setBuddy(m_edSpace);
	m_lbTabulator->setBuddy(m_coTabulator);
	m_lbDisplaymath->setBuddy(m_coDisplaymath);
	m_lbBullets->setBuddy(m_cbBullets);

	// initialize dialog
	m_coDisplaymath->insertItem(QString::null);
	m_coDisplaymath->insertItem("displaymath");
	m_coDisplaymath->insertItem("\\[");
	m_coDisplaymath->insertItem("equation");
	m_coDisplaymath->insertItem("equation*");

	// install environments
	initEnvironments();
	m_coEnvironment->setCurrentText("align");
	slotEnvironmentChanged(m_coEnvironment->currentItem());

	// signals and slots
	connect(m_coEnvironment, SIGNAL(activated(int)), this, SLOT(slotEnvironmentChanged(int)));
	connect(m_spCols, SIGNAL(valueChanged(int)), this, SLOT(slotSpinboxValueChanged(int)));

	m_coEnvironment->setWhatsThis(i18n("Choose an environment."));
	m_cbStarred->setWhatsThis(i18n("Use the starred version of this environment."));
	m_spRows->setWhatsThis(i18n("Choose the number of table rows."));
	m_spCols->setWhatsThis(i18n("Choose the number of table columns or alignment groups."));
	m_edSpace->setWhatsThis(i18n("Define an extra LaTeX command to separate alignment groups."));
	m_coTabulator->setWhatsThis(i18n("Choose one of some predefined tabulators."));
	m_coDisplaymath->setWhatsThis(i18n("Some environments are only valid in math mode. You can surround them with one of these display math modes."));
	m_cbBullets->setWhatsThis(i18n("Insert bullets in each cell. Alt+Ctrl+Right and Alt+Ctrl+Left will move very quick from one cell to another."));
}

void MathEnvironmentDialog::initEnvironments()
{
	// read all math environments and insert them into the combobox
	QStringList list;
	QStringList::ConstIterator it;
	m_latexCommands->commandList(list, (uint)(KileDocument::CmdAttrAmsmath | KileDocument::CmdAttrMath), false);
	for (it = list.begin(); it != list.end(); ++it)
	{
		m_coEnvironment->insertItem(*it);
	}
}

bool MathEnvironmentDialog::isGroupsParameterEnv()
{
	return (m_parameter == "{n}");
}

bool MathEnvironmentDialog::isParameterEnv()
{
	return (m_parameter.indexOf("{") >= 0);
}

////////////////////////////// determine the whole tag //////////////////////////////

void MathEnvironmentDialog::slotEnvironmentChanged(int index)
{
	KILE_DEBUG() << "environment changed: " << m_coEnvironment->text(index) << endl;
	m_envname = m_coEnvironment->text(index);

	// look for environment parameter in dictionary
	KileDocument::LatexCmdAttributes attr;
	if (m_latexCommands->commandAttributes(m_envname, attr))
	{
		m_starred = attr.starred;
		m_mathmode = attr.mathmode;
		m_columns = (attr.tabulator == "&");
		m_groups  = (attr.tabulator == "&=");
		m_fixedcolumns  = (attr.tabulator == "&=&");
		m_tabulator = attr.tabulator;
		m_parameter = attr.parameter;
	}

	// set starred version
	m_cbStarred->setChecked(false);
	m_lbStarred->setEnabled(m_starred);
	m_cbStarred->setEnabled(m_starred);

	// determine column/group entries
	QString labeltext = i18n("Number of cols:");
	bool spinstate = false;
	int minvalue = 1;
	int maxvalue = 1;
	int value = 1;

	if (m_columns)
	{
		spinstate = true;
		if (m_envname != "cases")            // 1,49,3
		{
			maxvalue = 49;
			value = 3;
		}
		else
		{
			minvalue = 2;                     // 2,2,2
			maxvalue = 2;
			value = 2;
		}
	} else
		if (m_groups)
		{
			spinstate = true;
			labeltext = i18n("Number of groups:");
			maxvalue = 19;                       // 1,19,1
		} else
			if (m_fixedcolumns)
			{
				spinstate = true;
				minvalue = 3;                        // 3,3,3
				maxvalue = 3;
				value = 3;
			} else
				if (m_envname == "split")
				{
					spinstate = true;
					maxvalue = 2;                        // 1,2,1
				}

	// set column/group entries
	m_lbCols->setText(labeltext);
	m_spCols->setMinValue(minvalue);
	m_spCols->setMaxValue(maxvalue);
	m_spCols->setValue(value);

	m_lbCols->setEnabled(spinstate);
	m_spCols->setEnabled(spinstate);
	slotSpinboxValueChanged(m_spCols->value());

	// set tabulator entries
	m_coTabulator->clear();
	QStringList tablist;
	if (m_tabulator == "&=&")
		tablist << "&=&" << "& &" << "&<&" << "&<=&" << "&>&" << "&>=&"
		<< "&\\ne&" << "&\\approx&" << "&\\equiv&" << "&\\conq&" ;
	else
		if (m_tabulator == "&=")
			tablist << "&=" << "& " << "&<" << "&<=" << "&>" << "&>="
			<< "&\\ne" << "&\\approx" << "&\\equiv" << "&\\conq" ;
		else
			if (! m_tabulator.isEmpty())
				tablist << "&";
	bool tabstate = (tablist.count() > 0);
	m_lbTabulator->setEnabled(tabstate);
	m_coTabulator->setEnabled(tabstate);
	if (tabstate)
		m_coTabulator->insertStringList(tablist);

	// set displaymathmode entries
	m_lbDisplaymath->setEnabled(m_mathmode);
	m_coDisplaymath->setEnabled(m_mathmode);
}

void MathEnvironmentDialog::slotSpinboxValueChanged(int index)
{
	bool state = (index > 1 && m_groups && isGroupsParameterEnv());
	m_lbSpace->setEnabled(state);
	m_edSpace->setEnabled(state);
}

void MathEnvironmentDialog::slotButtonClicked(int button)
{
	if (button == Ok) {
		// environment
		QString envname = (m_cbStarred->isChecked()) ? m_envname + '*' : m_envname;
		QString indent = m_ki->editorExtension()->autoIndentEnvironment();

		// use bullets?
		QString bullet = (m_cbBullets->isChecked()) ? s_bullet : QString::null;

		// normal tabulator
		QString tab = m_coTabulator->currentText();
		tab.replace("<=", "\\le");
		tab.replace(">=", "\\ge");
		QString tabulator = bullet + ' ' + tab + ' ';

		// number of rows
		int numrows = m_spRows->value();

		// get number of groups/columns and tabulator to separate these
		QString topgrouptabulator, grouptabulator;
		int numgroups;
		bool aligngroups;
		if (m_groups)
		{
			aligngroups = true;
			numgroups = (m_tabulator != "&") ? m_spCols->value() : 1;
			if (m_edSpace->isEnabled())
			{
				QString spaces;
				topgrouptabulator = "  &" + m_edSpace->text() + "  ";
				grouptabulator = "  &  " + spaces.fill(' ', m_edSpace->text().length());
			}
			else
			{
				topgrouptabulator = "  &  ";
				grouptabulator = "  &  ";
			}
		}
		else
		{
			aligngroups = false;
			if (! m_fixedcolumns)
				numgroups = (m_columns) ? m_spCols->value() - 1 : 0;
			else
				numgroups = 1;
		}

		// get displaymath mode
		QString displaymathbegin = QString::null;
		QString displaymathend = QString::null;
		if (m_coDisplaymath->isEnabled())
		{
			QString mathmode = m_coDisplaymath->currentText();
			if (! mathmode.isEmpty())
			{
				if (mathmode == "\\[")
				{
					displaymathbegin = "\\[\n";
					displaymathend   = "\\]\n";
				}
				else
				{
					displaymathbegin = QString("\\begin{%1}\n").arg(mathmode);
					displaymathend   = QString("\\end{%1}\n").arg(mathmode);
				}
			}
		}

		// build tag
		m_td.tagBegin = displaymathbegin;

		QString parameter;
		if (isGroupsParameterEnv())
			parameter = QString("{%2}").arg(numgroups);
		else
			if (isParameterEnv())
				parameter = '{' + bullet + '}';

		// open environment
		m_td.tagBegin += QString("\\begin{%1}").arg(envname) + parameter + '\n';

		for (int row = 0; row < numrows; ++row)
		{
			m_td.tagBegin += indent;
			for (int col = 0; col < numgroups; ++col)
			{
				m_td.tagBegin += tabulator;
				// is there more than one group or column?
				if (aligngroups && col < numgroups - 1)
				{
					if (row == 0)
						m_td.tagBegin += bullet + topgrouptabulator;
					else
						m_td.tagBegin += bullet + grouptabulator;
				}
			}
			// last row without CR
			if (row < numrows - 1)
				m_td.tagBegin += bullet + " \\\\\n";
			else
				m_td.tagBegin += bullet;
		}

		// close environment
		m_td.tagEnd = QString("\n\\end{%1}\n").arg(envname);
		m_td.tagEnd += displaymathend;

		m_td.dy = (displaymathbegin.isEmpty()) ? 1 : 2;
		m_td.dx = indent.length();

		accept();
	}
	KDialog::slotButtonClicked(button);
}

}

#include "mathenvdialog.moc"