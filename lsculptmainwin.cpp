#include <iostream>
#include <sstream>
#include <QWidget>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QFileDialog>
#include <QByteArray>
#include <QProgressDialog>
#include <QMessageBox>
#include <QCloseEvent>

#include "LSculpt_functions.h"
#include "lsculptmainwin.h"
#include "ui_lsculptmainwin.h"
#include "argpanel.h"

#include "LDVLib.h"

using namespace std;

#define lsculpt_version 0.4

LSculptMainWin::LSculptMainWin(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::LSculptMainWin)
{
	ui->setupUi(this);

	QWidget *center = new QWidget(this);
	QHBoxLayout *layout = new QHBoxLayout(center);

	panel = new ArgPanel(this);
	connect(panel, SIGNAL(runLSculptBtnClicked()), this, SLOT(invokeLSculpt()));

	ldvWin = new QWidget(this);
	ldvWin->setMinimumWidth(panel->minimumWidth());
	ldvWin->setMinimumHeight(panel->minimumHeight());

	layout->addWidget(panel);
	layout->addWidget(ldvWin);
	center->setLayout(layout);

	LDVSetLDrawDir("C:\\LDraw");
	pLDV = LDVInit(ldvWin->winId());
	LDVGLInit(pLDV);

	statusBar()->showMessage("Welcome to LSculpt's new, partially finished UI");
	setCentralWidget(center);
	setWindowModified(false);
	QString title = QString("LSculpt %1 [*]").arg(lsculpt_version);
	setWindowTitle(title);
}

LSculptMainWin::~LSculptMainWin()
{
	LDVDeInit(pLDV);
	delete ui;
}

// Ugly global variable, for now.  Cleaner than getting a non-static C++ function callback into low-level LSculpt
QProgressDialog *progress;
void incrProgress(const char * label)
{
	progress->setLabelText(label);
	progress->setValue(progress->value() + 1);
}

void LSculptMainWin::initProgressDialog()
{
	int maxSteps = 7 + 2;  // Keep this relatively in sync with the # of progress_cb calls in main_wrapper
	progress = new QProgressDialog("Clearing Preview", "Abort Update", 0, maxSteps, this->ldvWin, Qt::CustomizeWindowHint | Qt::WindowTitleHint);
	progress->setWindowModality(Qt::WindowModal);
	progress->setWindowTitle("Updating...");
	progress->setMinimumDuration(0);
	progress->setValue(1);  // Force immediate display
}

void LSculptMainWin::invokeLSculpt()
{
	if (this->currentFilename.isEmpty())
	{
		this->statusBar()->showMessage("Open a 3D mesh before running LSculpt");
		return;
	}

	initProgressDialog();
	incrProgress("Begin Update");

	char emptyLDraw[80] = "C:\\LSculpt\\debug\\empty.ldr";
	LDVSetFilename(pLDV, emptyLDraw);
	LDVLoadModel(pLDV, false);

	// Setup input & output filename (output is
	QByteArray ba = this->currentFilename.toLatin1();
	char *infile = ba.data();
	char outfile[80] = "";

	// Redirect cout & cerr to local string buffer
	streambuf *coutBuf = cout.rdbuf();
	streambuf *cerrBuf = cerr.rdbuf();

	stringbuf buffer;
	cout.rdbuf(&buffer);
	cerr.rdbuf(&buffer);

	// Build set of arguments from UI, pass to LSculpt, then run LSculpt
	ArgumentSet args = panel->getArguments(infile);
	setArgumentSet(args);
	setOutFile(args, infile, outfile);
	main_wrapper(infile, outfile, incrProgress);

	// Copy redirected string buffer to status bar
	statusBar()->showMessage(buffer.str().c_str());

	// Reset cout & cerr
	cout.rdbuf(coutBuf);
	cerr.rdbuf(cerrBuf);

	LDVSetFilename(pLDV, outfile);
	if (isLoaded)
	{
		LDVLoadModel(pLDV, false);
	}
	else
	{
		LDVLoadModel(pLDV, true);
		isLoaded = true;
	}

	setWindowModified(true);
	progress->setValue(progress->maximum());  // Ensure progress goes away
}

void LSculptMainWin::closeEvent(QCloseEvent *event)
{
	if (offerSave())
		event->accept();
	else
		event->ignore();
}

void LSculptMainWin::import3DMesh()
{
	if (offerSave())
	{
		QString filename = QFileDialog::getOpenFileName(this, "Import 3D mesh", QString(), "3D mesh files (*.ply *.stl *.obj);;All Files (*)");
		if (!filename.isEmpty())
		{
			currentFilename = filename;
			statusBar()->showMessage("Loaded: " + filename);
			isLoaded = false;
			invokeLSculpt();
			QString title = QString("LSculpt %1 - %2 [*]").arg(lsculpt_version).arg(filename);
			setWindowTitle(title);
			setWindowModified(true);
		}
	}
}

// Returns true if successfully saved, false otherwise
bool LSculptMainWin::exportToLDraw()
{
	QString filename = QFileDialog::getSaveFileName(this, "Save LDraw File As", QString(), "LDraw files (*.dat *.ldr *.mpd);;All Files (*)");
	if (filename.isEmpty())
		return false;

	QByteArray ba = filename.toLatin1();
	char *infile = ba.data();

	if (save_ldraw(infile))
	{
		this->statusBar()->showMessage("Saved LDraw File: " + filename);
		setWindowModified(false);
		return true;
	}

	this->statusBar()->showMessage("Error Saving LDraw File: " + filename);
	return false;
}

// Returns true if we should proceed with whatever is checking save state, false if we should cancel.
// Will prompt user for save if necessary.
bool LSculptMainWin::offerSave()
{
	if (!this->isWindowModified())
		return true;

	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(this,
								  tr("LSculpt - Unsaved Changes"),
								  tr("Save unsaved changes?"),
								  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (reply == QMessageBox::Yes)
		return this->exportToLDraw();
	return (reply == QMessageBox::No);
}

void LSculptMainWin::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	LDVSetSize(pLDV, ldvWin->width(), ldvWin->height());
}

void LSculptMainWin::changeEvent(QEvent *e)
{
	QMainWindow::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}
