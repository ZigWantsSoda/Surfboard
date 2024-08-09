import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk

class SimpleWindow(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self, title="Simple Window")
        self.set_default_size(800, 600)
        self.connect("destroy", Gtk.main_quit)

if __name__ == "__main__":
    win = SimpleWindow()
    win.show_all()
    Gtk.main()

