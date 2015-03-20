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
#include <bps/navigator.h>

class bbDialog {
public:
	bbDialog();
	virtual ~bbDialog();

	void showAlert(const char *title, const char *content);
	int  showPopuplistDialog(const char **list, int listCount, const char *title);
	void showNotification(const char *content);

private:
	dialog_instance_t mDialog;
};

#endif /* BBDIALOG_H_ */
