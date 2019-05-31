//gtkmm_example_buttons.hpp
#pragma once //used instead of the ifdef

#include <gtkmm-3.0/gtkmm/window.h>
#include <gtkmm-3.0/gtkmm/button.h>
#include <gtkmm-3.0/gtkmm/box.h>

class Buttons : public Gtk::Window
{
public:
Buttons();
virtual ~Buttons();

protected:
void on_button_clicked();
void when_paused();
void when_continued();
Gtk::Button button1, button2;
Gtk::Box box1;
};