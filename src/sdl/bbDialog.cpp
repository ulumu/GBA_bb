/*
 * bbDialog.cpp
 *
 *  Created on: Feb 25, 2013
 *      Author: bkchan
 */

#include "bbDialog.h"
#include "System.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define CHECK(x)                    \
	if (BPS_SUCCESS != x)           \
	{	                            \
		SLOG("Failed calling [%s], err:%d", #x, errno); \
		return;                     \
	}

#define CHECKR(x)                      \
	if (BPS_SUCCESS != x)              \
	{	                               \
		SLOG("Failed calling [%s], err:%d", #x, errno); \
		return -1;                     \
	}

bbDialog::bbDialog() {
	mDialog = 0;
	CHECK( dialog_request_events(0) );
}

bbDialog::~bbDialog() {
	if (mDialog) dialog_destroy(mDialog);
	mDialog = 0;
}


void bbDialog::showAlert(const char *title, const char *content)
{
    const char* okay_button_context = "Okay";

    if (title || content)
    {
    	CHECK( dialog_create_alert(&mDialog) );

		if (mDialog)
		{
			if (title)   CHECK( dialog_set_title_text(mDialog, title) );
			if (content) CHECK( dialog_set_alert_message_text(mDialog, content) );
			CHECK( dialog_add_button(mDialog, DIALOG_OK_LABEL, true, okay_button_context, true) );

			CHECK( dialog_show(mDialog) );

			while (1) {
				bps_event_t *event = 0;

				bps_get_event(&event, -1);    // -1 means that the function waits
											  // for an event before returning

				if (event)
				{
					if (bps_event_get_domain(event) == dialog_get_domain())
					{
						CHECK( dialog_event_get_selected_index(event) );
						CHECK( dialog_event_get_selected_label(event) );
						CHECK( dialog_event_get_selected_context(event) );
						break;
					}
				}
			}
		}
    }
}


int bbDialog::showPopuplistDialog(const char **list, int listCount, const char *title)
{
	const char*  cancel_button_context = "Canceled";
	const char*  okay_button_context   = "Okay";
	const char*  label;
	int          index = -1;

	if (list && (listCount > 0) )
	{
		CHECKR( dialog_create_popuplist(&mDialog) );
		CHECKR( dialog_set_popuplist_items(mDialog, list, listCount) );
		if (title) CHECKR( dialog_set_title_text(mDialog, title) );

		CHECKR( dialog_add_button(mDialog, DIALOG_CANCEL_LABEL, true, cancel_button_context, true) );
		CHECKR( dialog_add_button(mDialog, DIALOG_OK_LABEL,     true, okay_button_context,   true) );

		CHECKR( dialog_set_popuplist_multiselect(mDialog, false) );

		CHECKR( dialog_show(mDialog) );

		while(1)
		{
			bps_event_t *event = 0;

			bps_get_event(&event, -1);

			if (event)
			{
				if (bps_event_get_domain(event) == dialog_get_domain())
				{
					int *response[1];
					int num;

					label = dialog_event_get_selected_label(event);

					if(strcmp(label, DIALOG_OK_LABEL) == 0)
					{
						CHECKR( dialog_event_get_popuplist_selected_indices(event, (int**)response, &num) );
						if(num != 0)
						{
							//*response[0] is the index that was selected
							index = *response[0];
						}
						bps_free(response[0]);
					}

					break;
				}
			}
		}
	}

	return index;
}


void bbDialog::showNotification(const char *content)
{
	if (content)
	{
		CHECK( dialog_create_toast(&mDialog) );

		if (content) CHECK( dialog_set_toast_message_text(mDialog, content) );

		CHECK( dialog_set_toast_position(mDialog, DIALOG_POSITION_TOP_CENTER) );

		CHECK( dialog_show(mDialog) );

		while(1)
		{
			bps_event_t *event = 0;

			bps_get_event(&event, -1);

			if (event)
			{
				if (bps_event_get_domain(event) == dialog_get_domain())
				{
					break;
				}
			}
		}
	}
}

