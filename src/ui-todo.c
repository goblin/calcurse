/*
 * Calcurse - text-based organizer
 *
 * Copyright (c) 2004-2013 calcurse Development Team <misc@calcurse.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the
 *        following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the
 *        following disclaimer in the documentation and/or other
 *        materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Send your feedback or comments to : misc@calcurse.org
 * Calcurse home page : http://calcurse.org
 *
 */

#include "calcurse.h"

/* Request user to enter a new todo item. */
void ui_todo_add(void)
{
	int ch = 0;
	const char *mesg = _("Enter the new TODO item:");
	const char *mesg_id =
	    _("Enter the TODO priority [1 (highest) - 9 (lowest)]:");
	char todo_input[BUFSIZ] = "";

	status_mesg(mesg, "");
	if (getstring(win[STA].p, todo_input, BUFSIZ, 0, 1) ==
	    GETSTRING_VALID) {
		while ((ch < '1') || (ch > '9')) {
			status_mesg(mesg_id, "");
			ch = wgetch(win[KEY].p);
		}
		todo_add(todo_input, ch - '0', NULL);
		ui_todo_load_items();
	}
}

/* Delete an item from the TODO list. */
void ui_todo_delete(void)
{
	const char *del_todo_str =
	    _("Do you really want to delete this task?");
	const char *erase_warning =
	    _("This item has a note attached to it. "
	      "Delete (t)odo or just its (n)ote?");
	const char *erase_choice = _("[tn]");
	const int nb_erase_choice = 2;
	int answer;

	if (!LLIST_FIRST(&todolist) ||
	    (conf.confirm_delete
	     && (status_ask_bool(del_todo_str) != 1))) {
		wins_erase_status_bar();
		return;
	}

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));

	if (item->note)
		answer = status_ask_choice(erase_warning, erase_choice,
					   nb_erase_choice);
	else
		answer = 1;

	switch (answer) {
	case 1:
		todo_delete(item);
		ui_todo_load_items();
		break;
	case 2:
		todo_delete_note(item);
		break;
	default:
		wins_erase_status_bar();
		return;
	}
}

/* Edit the description of an already existing todo item. */
void ui_todo_edit(void)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	const char *mesg = _("Enter the new TODO description:");

	status_mesg(mesg, "");
	updatestring(win[STA].p, &item->mesg, 0, 1);
}

/* Pipe a todo item to an external program. */
void ui_todo_pipe(void)
{
	char cmd[BUFSIZ] = "";
	char const *arg[] = { cmd, NULL };
	int pout;
	int pid;
	FILE *fpout;

	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));

	status_mesg(_("Pipe item to external command:"), "");
	if (getstring(win[STA].p, cmd, BUFSIZ, 0, 1) != GETSTRING_VALID)
		return;

	wins_prepare_external();
	if ((pid = shell_exec(NULL, &pout, *arg, arg))) {
		fpout = fdopen(pout, "w");
		todo_write(item, fpout);
		fclose(fpout);
		child_wait(NULL, &pout, pid);
		press_any_key();
	}
	wins_unprepare_external();
}

/* Display todo items in the corresponding panel. */
void ui_todo_draw(int n, WINDOW *win, int y, int hilt, void *cb_data)
{
	llist_item_t *i = *((llist_item_t **)cb_data);
	struct todo *todo = LLIST_TS_GET_DATA(i);
	int mark[2];
	int width = lb_todo.sw.w;
	char buf[width * UTF8_MAXLEN];
	int j;

	mark[0] = todo->id > 0 ? '0' + todo->id : 'X';
	mark[1] = todo->note ? '>' : '.';

	if (hilt)
		custom_apply_attr(win, ATTR_HIGHEST);

	if (utf8_strwidth(todo->mesg) < width) {
		mvwprintw(win, y, 0, "%c%c %s", mark[0], mark[1], todo->mesg);
	} else {
		for (j = 0; todo->mesg[j] && width > 0; j++) {
			if (!UTF8_ISCONT(todo->mesg[j]))
				width -= utf8_width(&todo->mesg[j]);
			buf[j] = todo->mesg[j];
		}
		if (j)
			buf[j - 1] = 0;
		else
			buf[0] = 0;
		mvwprintw(win, y, 0, "%c%c %s...", mark[0], mark[1], buf);
	}

	if (hilt)
		custom_remove_attr(win, ATTR_HIGHEST);

	*((llist_item_t **)cb_data) = i->next;
}

int ui_todo_height(int n, void *cb_data)
{
	return 1;
}

void ui_todo_load_items(void)
{
	int n = 0;
	llist_item_t *i;

	/* TODO: Optimize this by keeping the list size in a variable. */
	LLIST_FOREACH(&todolist, i)
		n++;

	listbox_load_items(&lb_todo, n);
}

void ui_todo_sel_move(int delta)
{
	listbox_sel_move(&lb_todo, delta);
}

/* Updates the TODO panel. */
void ui_todo_update_panel(int which_pan)
{
	/*
	 * This is used and modified by display_todo_item() to avoid quadratic
	 * running time.
	 */
	llist_item_t *p = LLIST_FIRST(&todolist);

	listbox_set_cb_data(&lb_todo, &p);
	listbox_display(&lb_todo);
}

/* Change an item priority by pressing '+' or '-' inside TODO panel. */
void ui_todo_chg_priority(int diff)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	int id = item->id + diff;
	struct todo *item_new;

	if (id < 1)
		id = 1;
	else if (id > 9)
		id = 9;

	item_new = todo_add(item->mesg, id, item->note);
	todo_delete(item);
	listbox_set_sel(&lb_todo, todo_get_position(item_new));
}

void ui_todo_popup_item(void)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	item_in_popup(NULL, NULL, item->mesg, _("TODO:"));
}

void ui_todo_flag(void)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	todo_flag(item);
}

void ui_todo_view_note(void)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	todo_view_note(item, conf.pager);
}

void ui_todo_edit_note(void)
{
	if (!LLIST_FIRST(&todolist))
		return;

	struct todo *item = todo_get_item(listbox_get_sel(&lb_todo));
	todo_edit_note(item, conf.editor);
}