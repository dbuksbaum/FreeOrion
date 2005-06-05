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

		self.current_tech = None
		self.modified_techs = dict()
		
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
		
		techs_list.connect( 'row-activated', self.__on_tech_activated )

	def __undo_bookkeeping( self ) :
		if self.current_tech is None : return
		if self.current_tech.modified is True :
			self.modified_techs[self.current_tech.name] = self.current_tech
		print "Techs modified count", len(self.modified_techs.values())

	def __get_tech_for_edit( self, tech ) :
		print "Tech", tech.name, "was selected"
		self.__undo_bookkeeping()
		try :
			self.current_tech = self.modified_techs[tech.name]
		except KeyError :
			self.current_tech = tech.copy()
		

	def __on_tech_activated( self, treeview, path, view_column, *args ) :
		print "Row on techs list at path", path, "activated"
		iter = self.tech_tree.get_iter(path)
		tech_id = self.tech_tree.get_value( iter, 0 )
		tech_set = application.instance.tech_set
		try:
			tech = tech_set.tech_entries[tech_id]
			self.__get_tech_for_edit( tech )			
		except KeyError :
			print tech_id, "is not a tech"
			

	def __setup_prerequisites_view( self ) :
		prereqs_list = self.view.get_widget('prerequisites_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		prereqs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		prereqs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		prereqs_list.set_search_column(0)
	
	def __setup_unlocked_items_view( self ) :
		items_list = self.view.get_widget('unlocked_items_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		items_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		items_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		items_list.set_search_column(0)
	
	def __setup_effects_view( self ) :
		effects_list = self.view.get_widget('effects_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		effects_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		effects_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		effects_list.set_search_column(0)


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
		self.tech_tree.clear()
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
