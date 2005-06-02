import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade

class TechWindowController :
	
	def __init__( self ) :
		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade','tech_window')
		self.window = self.view.get_widget('tech_window')

		

		self.window.connect('delete_event', self.on_window_deleted )

	def on_window_deleted( self, widget, event, data=None ) :
		self.window.destroy()

	def show_all( self ) :
		self.window.show()
