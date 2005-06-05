import pygtk
pygtk.require('2.0')
import gtk

def not_implemented_feature_dialog( window ) :
		msg_dialog = gtk.MessageDialog( parent=window,type=gtk.MESSAGE_ERROR,buttons=gtk.BUTTONS_CLOSE,
										message_format='This feature has been not implemented yet!')
		msg_dialog.run()
		msg_dialog.hide()
		msg_dialog.destroy()

def recoverable_error( window, message ) :
	msg_dialog = gtk.MessageDialog( parent=window, type=gtk.MESSAGE_ERROR,buttons=gtk.BUTTONS_CLOSE,
									message_format = message )
	msg_dialog.run()
	msg_dialog.hide()
	msg_dialog.destroy()
