import os, sys

import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import gtk.gdk

from tools import not_implemented_feature_dialog, recoverable_error
from prereq_adder import *
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
		self.view.signal_connect('on_add_prerequisite_clicked', self.__on_add_prerequisite )
		self.view.signal_connect('on_remove_prerequisite_clicked', self.__on_remove_prerequisites )	
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
		
	def __on_add_prerequisite( self, button, *args ) :
		print "Adding prerequisites..."

		dialog = PrerequisitesAdderController()		

		techs_to_add = dialog.execute()

		if len(techs_to_add) == 0 : return
	
		for tech in techs_to_add :
			try:
				self.current_tech.prerequisites.index(tech)
				continue
			except ValueError :
				self.current_tech.prerequisites.append(tech)
				self.current_tech.modified = True

		self.__display_prerequisites()


	def __on_remove_prerequisites( self, button, *args ) :
		if self.current_tech is None : return

		print "Removing a prerequisite"
		prereq_view = self.view.get_widget('prerequisites_view')
		_, paths = prereq_view.get_selection().get_selected_rows()
		techs_to_remove = []
		if len(paths) > 0 :
			for path in paths :
				iter = self.prereq_list.get_iter(path)
				tech_id = self.prereq_list.get_value(iter,0)
				self.current_tech.prerequisites.remove(tech_id)
			
			self.current_tech.modified = True
			self.__display_prerequisites()

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
		#print "Techs modified count", len(self.modified_techs.values())

	def __get_tech_for_edit( self, tech ) :
		#print "Tech", tech.name, "was selected"
		self.__undo_bookkeeping()
		try :
			self.current_tech = self.modified_techs[tech.name]
		except KeyError :
			self.current_tech = tech.copy()
		

	def __on_tech_activated( self, treeview, path, view_column, *args ) :
		iter = self.tech_tree.get_iter(path)
		tech_id = self.tech_tree.get_value( iter, 0 )
		tech_set = application.instance.tech_set
		try:
			tech = tech_set.tech_entries[tech_id]
			self.__get_tech_for_edit( tech )
			self.__display_current_tech()			
		except KeyError :
			pass
			#print tech_id, "is not a tech"
			

	def __display_current_tech( self ) :
		print "Displaying", self.current_tech.name
		self.__display_scalars()
		self.__display_lists()

	def __display_scalars( self ) :
		strings = application.instance.supported_languages['English']
		name_id_txt = self.view.get_widget('tech_name_identifier_entry')
		name_id_txt.set_text( self.current_tech.name )
		name_str_txt = self.view.get_widget('tech_name_string_entry' )
		try:
			name_str_txt.set_text( strings[self.current_tech.name] )
		except KeyError :
			pass

		type_id_txt = self.view.get_widget('tech_type_identifier_entry')
		type_id_txt.set_text( self.current_tech.type )
		
		type_str_txt = self.view.get_widget('tech_type_string_entry')
		try:
			type_str_txt.set_text( strings[self.current_tech.type] )
		except KeyError :
			pass

		cat_id_txt = self.view.get_widget('tech_cat_identifier_entry' )
		cat_id_txt.set_text( self.current_tech.category )
	
		cat_str_txt = self.view.get_widget('tech_cat_string_entry' )
		try:
			cat_str_txt.set_text( strings[self.current_tech.category] )
		except KeyError :
			pass

		desc_str_txt = self.view.get_widget('description_view')
		desc_txt_buffer = gtk.TextBuffer()
	
		try:
			desc_txt_buffer.set_text( strings[self.current_tech.description] )
		except KeyError :
			pass		

		desc_str_txt.set_buffer( desc_txt_buffer )

		cost_button = self.view.get_widget('tech_cost_button')
		cost_button.set_value( self.current_tech.research_cost )
		
		turns_button = self.view.get_widget('tech_turns_button')
		turns_button.set_value( self.current_tech.research_turns )	

		icon = self.__load_tech_icon()	
		
		tech_image = self.view.get_widget('tech_image')
		tech_image.set_from_pixbuf( icon )

	def __load_tech_icon( self ) :
		basedir = application.instance.dataset_folder
		artdir = 'data/art'
		if self.current_tech.graphic is None or len(self.current_tech.graphic) == 0:
			icon_path = 'no_image.png'
		else :
			icon_path = "%s/%s/%s"%(basedir,artdir,self.current_tech.graphic)
			if not os.path.exists(icon_path) :
				recoverable_error(self.window,"Specified icon file does not exist: %s"%icon_path)
				icon_path = 'no_image.png'
		return gtk.gdk.pixbuf_new_from_file(icon_path)	
	
	def __display_prerequisites( self ) :
		english_strings = application.instance.supported_languages['English']

		prereq_view = self.view.get_widget('prerequisites_view')
		prereq_view.set_model(None)
		self.prereq_list.clear()
		for prereq in self.current_tech.prerequisites :
			try:
				self.prereq_list.append((prereq,english_strings[prereq]))
			except KeyError :
				self.prereq_list.append((prereq,""))
					
		prereq_view.set_model(self.prereq_list)

	def __display_effects( self ) :
		english_strings = application.instance.supported_languages['English']
		effects_view = self.view.get_widget('effects_view')	
		effects_view.set_model(None)
		self.effects_list.clear()

		for effect in self.current_tech.effects :
			try:
				self.effects_list.append((effect,english_strings[effect]) )
			except KeyError :
				self.effects_list.append((effect,""))

		effects_view.set_model(self.effects_list)

	def __display_unlocked_items( self ) :
		english_strings = application.instance.supported_languages['English']
		items_view = self.view.get_widget('unlocked_items_view')
		items_view.set_model(None)
		self.items_list.clear()
		
		for item in self.current_tech.unlocked_items :
			try:
				self.items_list.append((item,english_strings[item]) )
			except KeyError :
				self.items_list.append((item,""))

		items_view.set_model(self.items_list)

	def __display_lists( self ) :
		self.__display_prerequisites()
		self.__display_unlocked_items()
		self.__display_effects()


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
