import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade

from tools import not_implemented_feature_dialog
import application

class TechWindowController :
	
	def __init__( self ) :
		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade','tech_window')
		self.window = self.view.get_widget('tech_window')

		# signal connection

		self.view.signal_connect('on_approve_changes_item_activate', self.on_changes_approved )
		self.view.signal_connect('on_discard_changes_item_activate', self.on_changes_discarded )
		self.view.signal_connect('on_find_tech_item_activate', self.on_find_tech )
		self.view.signal_connect('on_language_item_activate', self.on_change_language_display )	
		self.window.connect('delete_event', self.on_window_deleted )

		# models for tech tree, prerequisite, unlocked items and effects lists
		self.tech_tree = gtk.TreeStore(str,str)
		self.prereq_list = gtk.ListStore(str,str)
		self.items_list = gtk.ListStore(str,str)
		self.effects_list = gtk.ListStore(str,str)

		# treeviews setup
		self.__setup_techs_list_view()
		self.__setup_prerequisites_view()
		self.__setup_unlocked_items_view()
		self.__setup_effects_view()
		
	def __setup_techs_list_view( self ) :
		techs_list = self.view.get_widget('techs_list_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		techs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		techs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		techs_list.set_search_column(0)

	def __setup_prerequisites_view( self ) :
		pass
	
	def __setup_unlocked_items_view( self ) :
		pass
	
	def __setup_effects_view( self ) :
		pass


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
		self.__feed_data_on_widgets()
		self.window.show_all()

	def __feed_data_on_widgets( self ) :
		self.__read_tech_list()

	def __read_tech_list( self ) :

		self.view.get_widget('techs_list_view').set_model(None)
		tech_set = application.instance.tech_set
		english_strings = application.instance.supported_languages['English']
		
		for category_identifier, tech_list in tech_set.tech_categories.items() :
			try:
				piter = self.tech_tree.append( None, (category_identifier, english_strings[category_identifier]) )
			except KeyError :
				piter = self.tech_tree.append( None, (category_identifier, "") )
			for tech in  tech_list :
				try:	
					self.tech_tree.append( piter, (tech.name, english_strings[tech.name]) )
				except KeyError :
					self.tech_tree.append( piter, (tech.name, "") )
		
		self.view.get_widget('techs_list_view').set_model(self.tech_tree)
