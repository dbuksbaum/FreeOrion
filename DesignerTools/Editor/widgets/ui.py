import pygtk
pygtk.require('2.0')
import gtk
from mainwindow import *
from techwindow import *

class InterfaceFacade :
	
	def __init__(self) :
		self.main_window = MainWindowController()
		self.tech_window = None

	def start( self ) :		
		self.main_window.show_all() 
		gtk.main()

	def show_tech_window( self ) :
		self.tech_window = TechWindowController()
		self.tech_window.show_all()	
