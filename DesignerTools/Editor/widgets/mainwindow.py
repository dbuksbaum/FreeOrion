import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import sys
from tools import not_implemented_feature_dialog

import application

class MainWindowController :
	
	def __init__( self ) :

		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade','main_window')
		
		self.window = self.view.get_widget('main_window') 

		self.view.signal_connect( 'on_new_game_dataset_activated', self.on_new_game_dataset_activated )
		self.view.signal_connect( 'on_open_existing_dataset_activated', self.on_open_existing_dataset_activated )
		self.view.signal_connect( 'on_quit_activated', self.on_quit_activated )
		self.view.signal_connect( 'on_about_activated', self.on_about_activated )
		self.view.signal_connect( 'on_view_techs_item_activate', self.on_view_tech_window )
	
		self.window.connect('delete_event', self.on_window_deleted )

	def __passes_check_dataset_folder( self ) :
		if application.instance.dataset_folder is None :
			msg_dialog = gtk.MessageDialog( parent=self.window,type=gtk.MESSAGE_ERROR,buttons=gtk.BUTTONS_CLOSE,
											message_format='You must select first a game dataset to work upon')
			msg_dialog.run()
			msg_dialog.hide()
			msg_dialog.destroy()
			return False
		return True		
		

	def on_view_tech_window( self, *args ) :
		if not self.__passes_check_dataset_folder() : return
				
		application.instance.ui_facade.show_tech_window()		



	def on_new_game_dataset_activated( self, *args ) :	
		print "Creating new game dataset..."
		not_implemented_feature_dialog(self.window)
				

	def on_open_existing_dataset_activated( self, *args ) :		
		print "Open existing game dataset..."
		file_dlg = gtk.FileChooserDialog( title='Select game dataset folder',
										parent=self.window,
										action=gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER,
										buttons=(gtk.STOCK_OK, gtk.RESPONSE_OK, gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL))
		response = file_dlg.run()
		
		if response == gtk.RESPONSE_OK : # folder was selected
			print "Selected folder was:", file_dlg.get_filename()
			application.instance.dataset_folder = file_dlg.get_filename()
			application.instance.load_dataset()

		file_dlg.hide()
		file_dlg.destroy()
			
	
	def on_quit_activated( self, *args ) :
		self.window.hide()
		self.window.destroy()
		gtk.main_quit()

	def on_about_activated( self, *args ) :
		
		print "Showing about dlg"
		self.dialog_object = gtk.glade.XML('glade-files/widgets/widgets.glade','about_dialog')
		self.dialog_object.get_widget('about_dialog').run()
		self.dialog_object.get_widget('about_dialog').hide()
		self.dialog_object.get_widget('about_dialog').destroy()		

	def on_window_deleted( self, widget, event, data=None ) :
		print >> sys.stdout, "Closing down application!"
		gtk.main_quit()
		return False

	def show_all( self ) :
		self.window.show()


