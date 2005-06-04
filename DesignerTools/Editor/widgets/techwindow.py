import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade

from tools import not_implemented_feature_dialog

class TechWindowController :
	
	def __init__( self ) :
		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade','tech_window')
		self.window = self.view.get_widget('tech_window')

		self.view.signal_connect('on_approve_changes_item_activate', self.on_changes_approved )
		self.view.signal_connect('on_discard_changes_item_activate', self.on_changes_discarded )
		self.view.signal_connect('on_find_tech_item_activate', self.on_find_tech )
		self.view.signal_connect('on_language_item_activate', self.on_change_language_display )	

		self.window.connect('delete_event', self.on_window_deleted )


	def on_changes_approved( self, *args ) :
		self.window.hide()
		self.window.destroy()
	
	def on_changes_discarded( self, *args ) :
		self.window.hide()
		self.window.destroy()
	
	def on_find_tech( self, *args ) :
		not_implemented_feature_dialog(self.window)	
	
	def on_change_language_display( self, *args ) :
		not_implemented_feature_dialog(self.window)

	def on_window_deleted( self, widget, event, data=None ) :
		self.window.destroy()

	def show_all( self ) :
		self.window.show()
