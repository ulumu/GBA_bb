/*
 * bbDialog.cpp
 *
 *  Created on: Feb 25, 2013
 *      Author: bkchan
 */

#include "bbDialog.h"

bbDialog::bbDialog() {
	mDialog = 0;
}

bbDialog::~bbDialog() {
	if (mDialog) dialog_destroy(mDialog);
}


void bbDialog::showAlert(const char *string)
{
    const char* okay_button_context = "Okay";

    if (string)
    {
		dialog_create_alert(&mDialog);
		dialog_set_alert_message_text(mDialog, string);
		dialog_add_button(mDialog, DIALOG_OK_LABEL, true, okay_button_context, true);

		dialog_show(mDialog);
    }
}


void bbDialog::showPopuplistDialog(const char **list, int listCount, const char *title)
{
	const char*  cancel_button_context = "Canceled";
	const char*  okay_button_context   = "Okay";

	if (list && (listCount > 0) )
	{
		dialog_create_popuplist(&mDialog);
		dialog_set_popuplist_items(mDialog, list, listCount);
		if (title) dialog_set_title_text(mDialog, title);

		dialog_add_button(mDialog, DIALOG_CANCEL_LABEL, true, cancel_button_context, true);
		dialog_add_button(mDialog, DIALOG_OK_LABEL,     true, okay_button_context,   true);

		dialog_set_popuplist_multiselect(mDialog, false);

		dialog_show(mDialog);
	}
}
