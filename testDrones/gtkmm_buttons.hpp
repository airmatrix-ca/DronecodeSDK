//gtkmm_example_buttons.hpp
#pragma once //used instead of the ifdef

#include <gtkmm-3.0/gtkmm/window.h>
#include <gtkmm-3.0/gtkmm/button.h>
#include <gtkmm-3.0/gtkmm/box.h>

class Buttons : public Gtk::Window
{
private:
int nums;

public:
Buttons(int nums = 3);
virtual ~Buttons();

protected:
void when_paused(Glib::ustring data);
void when_continued(Glib::ustring data);
void when_returning(Glib::ustring data);
Gtk::Button *button;
Gtk::Box box1;
};