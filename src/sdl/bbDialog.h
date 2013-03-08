/*
 * bbDialog.h
 *
 *  Created on: Feb 25, 2013
 *      Author: bkchan
 */

#ifndef BBDIALOG_H_
#define BBDIALOG_H_

#include <bps/bps.h>
#include <bps/dialog.h>

class bbDialog {
public:
	bbDialog();
	virtual ~bbDialog();

	void showAlert(const char *string);
	void showPopuplistDialog(const char **list, int listCount, const char *title);

private:
	dialog_instance_t mDialog;
};

#endif /* BBDIALOG_H_ */
